#pragma once

#include <pi/ai/cancellation.hpp>
#include <pi/ai/event_stream.hpp>
#include <pi/ai/message.hpp>

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace pi::ai {

enum class ModelInput {
    Text,
    Image,
};

struct ModelCost {
    double input = 0;
    double output = 0;
    double cacheRead = 0;
    double cacheWrite = 0;
};

struct Model {
    std::string id;
    std::string name;
    std::string api;
    std::string provider;
    std::string baseUrl;
    bool reasoning = false;
    std::vector<ModelInput> input{ModelInput::Text};
    ModelCost cost;
    std::int64_t contextWindow = 0;
    std::int64_t maxTokens = 0;
    std::map<std::string, std::string> headers;
};

struct ToolDefinition {
    std::string name;
    std::string description;
    nlohmann::json parameters = nlohmann::json::object();
};

struct Context {
    std::optional<std::string> systemPrompt;
    std::vector<Message> messages;
    std::vector<ToolDefinition> tools;
};

// A null header value suppresses a provider/model default header with the same
// case-insensitive name. Concrete providers perform the actual merge.
using ProviderHeaders = std::map<std::string, std::optional<std::string>>;

struct StreamOptions {
    std::optional<double> temperature;
    std::optional<std::int64_t> maxTokens;
    std::shared_ptr<CancellationToken> cancellation;
    std::optional<std::string> apiKey;
    ProviderHeaders headers;
    std::optional<std::chrono::milliseconds> timeout;
    // Number of retry attempts after the initial request. The OpenAI-compatible
    // provider defaults to zero, matching pi v0.80.0's OpenAI path.
    std::optional<std::size_t> maxRetries;
    // Cap for a server-requested Retry-After delay. Default is 60 seconds.
    // Zero disables the cap.
    std::optional<std::chrono::milliseconds> maxRetryDelay;
};

class Provider {
public:
    virtual ~Provider() = default;

    Provider(const Provider&) = delete;
    Provider& operator=(const Provider&) = delete;
    Provider(Provider&&) = delete;
    Provider& operator=(Provider&&) = delete;

    // Implementations should return a stream and encode request/runtime failures
    // as terminal EvError events rather than allowing worker-thread exceptions to
    // escape or terminate the process.
    virtual AssistantMessageEventStream stream(
        const Model& model,
        const Context& context,
        const StreamOptions& options) = 0;

protected:
    Provider() = default;
};

} // namespace pi::ai
