#!/usr/bin/env python3
"""Run deterministic OpenAI SSE scenarios against pi v0.80.0 and pi-cpp."""

from __future__ import annotations

import argparse
import difflib
import json
import os
import subprocess
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any

PI_REPOSITORY = "https://github.com/earendil-works/pi"
PI_TAG = "v0.80.0"
PI_COMMIT = "f08e968c83d92bce5f5fd2f7f20ef37f8cf04a39"

REPO_ROOT = Path(__file__).resolve().parents[2]
SCENARIO_FILE = REPO_ROOT / "tests" / "reference" / "scenarios" / "openai.json"
PI_RUNNER = REPO_ROOT / "tests" / "reference" / "pi" / "runner.ts"


def canonical_lines(trace: list[Any]) -> list[str]:
    return [
        json.dumps(item, sort_keys=True, separators=(",", ":"), ensure_ascii=False)
        for item in trace
    ]


def parse_trace(stdout: str, label: str) -> list[Any]:
    trace: list[Any] = []
    for line_number, raw in enumerate(stdout.splitlines(), start=1):
        if not raw.strip():
            continue
        try:
            trace.append(json.loads(raw))
        except json.JSONDecodeError as error:
            raise RuntimeError(
                f"{label} emitted non-JSON trace line {line_number}: {raw!r}"
            ) from error
    if not trace:
        raise RuntimeError(f"{label} emitted an empty trace")
    return trace


def run_command(command: list[str], *, cwd: Path | None = None, env: dict[str, str] | None = None) -> list[Any]:
    completed = subprocess.run(
        command,
        cwd=cwd,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if completed.returncode != 0:
        joined = " ".join(command)
        raise RuntimeError(
            f"command failed ({completed.returncode}): {joined}\n"
            f"--- stdout ---\n{completed.stdout}\n"
            f"--- stderr ---\n{completed.stderr}"
        )
    return parse_trace(completed.stdout, command[0])


class ScenarioServer(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self, chunks: list[str]):
        super().__init__(("127.0.0.1", 0), ScenarioHandler)
        self.chunks = chunks


class ScenarioHandler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def do_POST(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler contract
        if self.path != "/v1/chat/completions":
            self.send_error(404)
            return

        length = int(self.headers.get("Content-Length", "0"))
        if length:
            self.rfile.read(length)

        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Connection", "close")
        self.end_headers()

        try:
            for chunk in self.server.chunks:  # type: ignore[attr-defined]
                self.wfile.write(chunk.encode("utf-8"))
                self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError):
            pass
        finally:
            self.close_connection = True

    def log_message(self, _format: str, *_args: object) -> None:
        return


def verify_reference_checkout(pi_root: Path) -> None:
    actual_commit = subprocess.check_output(
        ["git", "-C", str(pi_root), "rev-parse", "HEAD"],
        text=True,
    ).strip()
    if actual_commit != PI_COMMIT:
        raise RuntimeError(
            "reference checkout mismatch: "
            f"expected {PI_COMMIT}, got {actual_commit}"
        )

    package_json = json.loads((pi_root / "packages" / "ai" / "package.json").read_text(encoding="utf-8"))
    if package_json.get("version") != "0.80.0":
        raise RuntimeError(
            "reference package version mismatch: "
            f"expected 0.80.0, got {package_json.get('version')!r}"
        )

    tsx = pi_root / "node_modules" / ".bin" / "tsx"
    if not tsx.exists():
        raise RuntimeError(
            f"tsx not found at {tsx}; run npm ci --ignore-scripts in the pi reference checkout"
        )


def compare_traces(expected: list[Any], actual: list[Any], expected_name: str, actual_name: str) -> None:
    if expected == actual:
        return

    expected_lines = canonical_lines(expected)
    actual_lines = canonical_lines(actual)
    diff = "\n".join(
        difflib.unified_diff(
            expected_lines,
            actual_lines,
            fromfile=expected_name,
            tofile=actual_name,
            lineterm="",
        )
    )
    raise RuntimeError(f"differential trace mismatch:\n{diff}")


def write_trace(path: Path, trace: list[Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    text = "\n".join(canonical_lines(trace)) + "\n"
    path.write_text(text, encoding="utf-8")


def read_trace(path: Path) -> list[Any]:
    if not path.exists():
        raise RuntimeError(f"canonical fixture is missing: {path}")
    return parse_trace(path.read_text(encoding="utf-8"), str(path))


def run_scenario(
    name: str,
    chunks: list[str],
    *,
    pi_root: Path,
    cpp_runner: Path,
    output_dir: Path,
    fixture_dir: Path | None,
) -> None:
    server = ScenarioServer(chunks)
    worker = threading.Thread(target=server.serve_forever, daemon=True)
    worker.start()

    try:
        host, port = server.server_address
        base_url = f"http://{host}:{port}/v1"

        pi_env = os.environ.copy()
        pi_env["PI_REFERENCE_ROOT"] = str(pi_root)
        pi_trace = run_command(
            [str(pi_root / "node_modules" / ".bin" / "tsx"), str(PI_RUNNER), base_url],
            cwd=pi_root,
            env=pi_env,
        )
        cpp_trace = run_command([str(cpp_runner), base_url], cwd=REPO_ROOT)

        write_trace(output_dir / f"{name}.pi.jsonl", pi_trace)
        write_trace(output_dir / f"{name}.cpp.jsonl", cpp_trace)

        compare_traces(pi_trace, cpp_trace, f"pi-v0.80.0/{name}", f"pi-cpp/{name}")

        if fixture_dir is not None:
            fixture = read_trace(fixture_dir / f"{name}.jsonl")
            compare_traces(fixture, pi_trace, f"fixture/{name}", f"pi-v0.80.0/{name}")

        print(f"PASS {name}: {len(pi_trace)} events")
    finally:
        server.shutdown()
        server.server_close()
        worker.join(timeout=5)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pi-root", required=True, type=Path)
    parser.add_argument("--cpp-runner", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--fixture-dir", type=Path)
    parser.add_argument("--scenario", action="append", dest="scenarios")
    args = parser.parse_args()

    pi_root = args.pi_root.resolve()
    cpp_runner = args.cpp_runner.resolve()
    output_dir = args.output_dir.resolve()
    fixture_dir = args.fixture_dir.resolve() if args.fixture_dir else None

    verify_reference_checkout(pi_root)
    if not cpp_runner.exists():
        raise RuntimeError(f"pi-cpp trace runner not found: {cpp_runner}")

    document = json.loads(SCENARIO_FILE.read_text(encoding="utf-8"))
    if document.get("schema") != 1 or not isinstance(document.get("scenarios"), dict):
        raise RuntimeError(f"invalid scenario document: {SCENARIO_FILE}")

    scenarios: dict[str, Any] = document["scenarios"]
    selected = args.scenarios or list(scenarios.keys())
    for name in selected:
        if name not in scenarios:
            raise RuntimeError(f"unknown scenario: {name}")
        chunks = scenarios[name].get("chunks")
        if not isinstance(chunks, list) or not all(isinstance(chunk, str) for chunk in chunks):
            raise RuntimeError(f"scenario {name!r} has invalid chunks")
        run_scenario(
            name,
            chunks,
            pi_root=pi_root,
            cpp_runner=cpp_runner,
            output_dir=output_dir,
            fixture_dir=fixture_dir,
        )

    print(
        f"Differential gate passed against {PI_REPOSITORY} {PI_TAG} ({PI_COMMIT})."
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:  # keep CI failure output compact and actionable
        print(f"ERROR: {error}", file=sys.stderr)
        raise SystemExit(1)
