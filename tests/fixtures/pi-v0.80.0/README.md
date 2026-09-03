# pi v0.80.0 reference fixtures

This directory is the compatibility-evidence root for pi-cpp.

## Canonical upstream baseline

```text
repository: https://github.com/earendil-works/pi
tag:        v0.80.0
commit:     f08e968c83d92bce5f5fd2f7f20ef37f8cf04a39
package:    @earendil-works/pi-ai 0.80.0
node:       >= 22.19.0
```

The exact commit is intentionally recorded in addition to the tag. The differential harness verifies `git rev-parse HEAD` and the `packages/ai/package.json` version before it accepts any reference trace.

## Compatibility rule

Behavior conflicts are resolved in this order:

```text
pi v0.80.0 observable behavior
    > explicit pi-cpp compatibility spec
    > Tau v0.4.1 implementation reference
```

Tau fixtures may be used for engineering regression only. They are not canonical compatibility evidence.

## Real differential harness

The v0.0.2 harness does not hand-write the reference event sequence. It starts a deterministic local OpenAI-compatible HTTP/SSE endpoint and sends the same scenario through both implementations:

```text
tests/reference/scenarios/openai.json
          |
          +--> local HTTP/SSE server --> pi v0.80.0 source runner --> normalized JSONL
          |
          +--> local HTTP/SSE server --> pi-cpp Provider          --> normalized JSONL
                                                                  |
                                                              strict diff
```

The upstream runner imports `packages/ai/src/api/openai-completions.ts` from the exact checkout. The pi-cpp runner consumes the public `OpenAICompatibleProvider`, so the comparison includes the real HTTP adapter, SSE parser, delta merger and `AssistantMessageEventStream` rather than calling private decoder helpers directly.

Initial deterministic scenarios are:

- `text-stop`: two text deltas, stable usage, normal stop;
- `tool-call`: tool-call arguments split across SSE deltas, including partial JSON state;
- `length-stop`: text terminated by `finish_reason=length`.

## Trace normalization

Normalization is allow-list based. The harness removes fields that are nondeterministic and not the target of this gate, currently timestamps and monetary cost calculations.

It keeps and compares:

- event type and ordering;
- `contentIndex`;
- normalized `partial` message state on every streaming event;
- text/thinking/tool-call deltas;
- aggregated text/thinking/tool-call content;
- tool-call id, name and parsed arguments, including partial arguments;
- `responseId` / `responseModel` when present;
- `stopReason`;
- stable usage fields: input, output, cacheRead, cacheWrite and totalTokens;
- error messages for terminal errors.

A JSON `thoughtSignature` is normalized to its parsed JSON value because pi v0.80.0 parses that signature again before replaying `reasoning_details`; JSON object key order is not a behavioral distinction. Non-JSON opaque signatures remain strings.

Stable business fields must not be normalized away to make a mismatch disappear.

## Reproduction

A local reproduction uses the same exact checkout as CI:

```bash
git clone https://github.com/earendil-works/pi .reference/pi
git -C .reference/pi checkout f08e968c83d92bce5f5fd2f7f20ef37f8cf04a39
npm ci --ignore-scripts --prefix .reference/pi

cmake --preset release
cmake --build --preset release --target pi_reference_trace --parallel

python3 tests/reference/run_differential.py \
  --pi-root .reference/pi \
  --cpp-runner build/release/tests/pi_reference_trace \
  --output-dir build/reference-traces
```

On Windows, invoke the corresponding `.exe`; the canonical differential CI runs on Ubuntu while the regular build matrix compiles `pi_reference_trace` on Linux, macOS and Windows.

Once canonical traces are committed under `traces/`, pass:

```text
--fixture-dir tests/fixtures/pi-v0.80.0/traces
```

to additionally verify that a newly generated real pi trace still matches the checked-in evidence.

## Directory layout

```text
pi-v0.80.0/
├── README.md
├── message/
├── events/
└── traces/
```

Each generated fixture set records or inherits from this document:

1. deterministic input;
2. upstream source path / harness entrypoint;
3. generation command;
4. exact baseline commit;
5. normalization rules;
6. expected stable fields.

## Status

The baseline metadata and real differential harness are present. The first Compatibility run generates the canonical upstream JSONL traces as a workflow artifact; after review those traces are checked in under `traces/` and become an additional fixture gate.
