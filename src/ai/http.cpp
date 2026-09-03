#include "ai/http.hpp"

#include <cpr/cpr.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace pi::ai::detail {

namespace {

long parseStatusLine(std::string_view line) {
    if (line.size() < 8 || line.substr(0, 5) != "HTTP/") return 0;

    const auto firstSpace = line.find(' ');
    if (firstSpace == std::string_view::npos || firstSpace + 3 >= line.size()) return 0;

    const auto a = line[firstSpace + 1];
    const auto b = line[firstSpace + 2];
    const auto c = line[firstSpace + 3];
    if (a < '0' || a > '9' || b < '0' || b > '9' || c < '0' || c > '9') return 0;

    return static_cast<long>((a - '0') * 100 + (b - '0') * 10 + (c - '0'));
}

class CprHttpTransport final : public HttpTransport {
public:
    HttpResponse postStream(
        const HttpRequest& request,
        HttpChunkHandler onChunk) override {
        cpr::Session session;
        session.SetUrl(cpr::Url{request.url});

        cpr::Header headers;
        for (const auto& [name, value] : request.headers) {
            headers[name] = value;
        }
        session.SetHeader(headers);
        session.SetBody(cpr::Body{request.body});
        session.SetConnectTimeout(cpr::ConnectTimeout{request.connectTimeout});
        if (request.timeout) {
            session.SetTimeout(cpr::Timeout{*request.timeout});
        }
        session.SetLowSpeed(cpr::LowSpeed{
            request.lowSpeedLimitBytesPerSecond,
            request.lowSpeedTime});

        auto cancelled = std::make_shared<std::atomic_bool>(false);
        std::uint64_t cancellationRegistration = 0;
        if (request.cancellation) {
            cancelled->store(request.cancellation->requested(), std::memory_order_release);
            cancellationRegistration = request.cancellation->registerCallback([cancelled] {
                cancelled->store(true, std::memory_order_release);
            });
            session.SetCancellationParam(cancelled);
        }

        long currentStatus = 0;
        bool consumerAborted = false;
        std::string errorBody;

        session.SetHeaderCallback(cpr::HeaderCallback{
            [&currentStatus](std::string_view line, std::intptr_t) {
                if (const auto status = parseStatusLine(line); status != 0) {
                    currentStatus = status;
                }
                return true;
            }});

        session.SetWriteCallback(cpr::WriteCallback{
            [&currentStatus, &consumerAborted, &errorBody, &onChunk](
                std::string_view data,
                std::intptr_t) {
                if (currentStatus >= 400) {
                    errorBody.append(data.data(), data.size());
                    return true;
                }

                if (onChunk && !onChunk(data)) {
                    consumerAborted = true;
                    return false;
                }
                return true;
            }});

        const auto response = session.Post();

        if (request.cancellation && cancellationRegistration != 0) {
            request.cancellation->unregisterCallback(cancellationRegistration);
        }

        HttpResponse result;
        result.status = response.status_code;
        result.errorBody = std::move(errorBody);
        for (const auto& [name, value] : response.header) {
            result.headers[name] = value;
        }

        if (response.error) {
            if (cancelled->load(std::memory_order_acquire) ||
                (request.cancellation && request.cancellation->requested())) {
                result.errorKind = HttpErrorKind::Cancelled;
            } else if (consumerAborted) {
                result.errorKind = HttpErrorKind::ConsumerAborted;
            } else {
                result.errorKind = HttpErrorKind::Transport;
            }
            result.errorMessage = response.error.message;
        }

        return result;
    }
};

} // namespace

std::shared_ptr<HttpTransport> makeCprHttpTransport() {
    return std::make_shared<CprHttpTransport>();
}

} // namespace pi::ai::detail
