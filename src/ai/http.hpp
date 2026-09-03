#pragma once

#include <pi/ai/cancellation.hpp>

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace pi::ai::detail {

enum class HttpErrorKind {
    None,
    Cancelled,
    Transport,
    ConsumerAborted,
};

struct HttpRequest {
    std::string url;
    std::map<std::string, std::string> headers;
    std::string body;
    std::chrono::milliseconds connectTimeout{10'000};
    std::optional<std::chrono::milliseconds> timeout;
    std::int32_t lowSpeedLimitBytesPerSecond = 1;
    std::chrono::seconds lowSpeedTime{30};
    std::shared_ptr<CancellationToken> cancellation;
};

struct HttpResponse {
    long status = 0;
    std::map<std::string, std::string> headers;
    HttpErrorKind errorKind = HttpErrorKind::None;
    std::string errorMessage;
    std::string errorBody;
};

using HttpChunkHandler = std::function<bool(std::string_view)>;

class HttpTransport {
public:
    virtual ~HttpTransport() = default;

    virtual HttpResponse postStream(
        const HttpRequest& request,
        HttpChunkHandler onChunk) = 0;
};

std::shared_ptr<HttpTransport> makeCprHttpTransport();

} // namespace pi::ai::detail
