import path from "node:path";
import { pathToFileURL } from "node:url";

function normalizeThoughtSignature(value: unknown): unknown {
  if (typeof value !== "string") return value;
  try {
    return JSON.parse(value);
  } catch {
    return value;
  }
}

function normalizeContent(block: any): Record<string, unknown> {
  if (block?.type === "text") {
    const result: Record<string, unknown> = { type: "text", text: block.text ?? "" };
    if (block.textSignature !== undefined) result.textSignature = block.textSignature;
    return result;
  }
  if (block?.type === "thinking") {
    const result: Record<string, unknown> = {
      type: "thinking",
      thinking: block.thinking ?? "",
    };
    if (block.thinkingSignature !== undefined) {
      result.thinkingSignature = block.thinkingSignature;
    }
    if (block.redacted === true) result.redacted = true;
    return result;
  }
  if (block?.type === "toolCall") {
    const result: Record<string, unknown> = {
      type: "toolCall",
      id: block.id ?? "",
      name: block.name ?? "",
      arguments: block.arguments ?? {},
    };
    if (block.thoughtSignature !== undefined) {
      result.thoughtSignature = normalizeThoughtSignature(block.thoughtSignature);
    }
    return result;
  }
  if (block?.type === "image") {
    return { type: "image", data: block.data ?? "", mimeType: block.mimeType ?? "" };
  }
  return { type: String(block?.type ?? "unknown") };
}

function normalizeUsage(usage: any): Record<string, number> {
  return {
    input: Number(usage?.input ?? 0),
    output: Number(usage?.output ?? 0),
    cacheRead: Number(usage?.cacheRead ?? 0),
    cacheWrite: Number(usage?.cacheWrite ?? 0),
    totalTokens: Number(usage?.totalTokens ?? 0),
  };
}

function normalizeMessage(message: any): Record<string, unknown> {
  const result: Record<string, unknown> = {
    api: message?.api ?? "",
    provider: message?.provider ?? "",
    model: message?.model ?? "",
    content: Array.isArray(message?.content) ? message.content.map(normalizeContent) : [],
    usage: normalizeUsage(message?.usage),
    stopReason: message?.stopReason ?? "stop",
  };
  if (message?.responseId !== undefined) result.responseId = message.responseId;
  if (message?.responseModel !== undefined) result.responseModel = message.responseModel;
  if (message?.errorMessage !== undefined) result.errorMessage = message.errorMessage;
  return result;
}

function normalizeEvent(event: any): Record<string, unknown> {
  const result: Record<string, unknown> = {
    kind: "event",
    type: event.type,
  };

  if (event.contentIndex !== undefined) result.contentIndex = event.contentIndex;
  if (event.delta !== undefined) result.delta = event.delta;
  if (event.content !== undefined) result.content = event.content;
  if (event.partial !== undefined) result.partial = normalizeMessage(event.partial);
  if (event.toolCall !== undefined) result.toolCall = normalizeContent(event.toolCall);

  if (event.type === "done") {
    result.reason = event.reason;
    result.message = normalizeMessage(event.message);
  } else if (event.type === "error") {
    result.reason = event.reason;
    result.error = normalizeMessage(event.error);
  }

  return result;
}

async function main(): Promise<void> {
  const referenceRoot = process.env.PI_REFERENCE_ROOT;
  const baseUrl = process.argv[2];

  if (!referenceRoot) {
    throw new Error("PI_REFERENCE_ROOT is required");
  }
  if (!baseUrl) {
    throw new Error("usage: runner.ts <base-url>");
  }

  const moduleUrl = pathToFileURL(
    path.join(referenceRoot, "packages", "ai", "src", "api", "openai-completions.ts"),
  ).href;
  const { stream } = await import(moduleUrl);

  const model: any = {
    id: "gpt-ref",
    name: "GPT Reference",
    api: "openai-completions",
    provider: "openai",
    baseUrl,
    reasoning: false,
    input: ["text"],
    cost: { input: 0, output: 0, cacheRead: 0, cacheWrite: 0 },
    contextWindow: 128000,
    maxTokens: 4096,
  };

  const context: any = {
    systemPrompt: "You are concise.",
    messages: [
      {
        role: "user",
        content: "hello",
        timestamp: 1,
      },
    ],
    tools: [
      {
        name: "read",
        description: "Read a file",
        parameters: {
          type: "object",
          properties: { path: { type: "string" } },
          required: ["path"],
        },
      },
    ],
  };

  const events = stream(model, context, { apiKey: "test-key", maxRetries: 0 });
  for await (const event of events) {
    process.stdout.write(`${JSON.stringify(normalizeEvent(event))}\n`);
  }
}

void main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
