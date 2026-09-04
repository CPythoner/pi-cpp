#include "ai/streaming_json.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>

namespace pi::ai::detail {

namespace {

bool isControlCharacter(unsigned char ch) noexcept {
    return ch <= 0x1f;
}

bool isHexDigit(char ch) noexcept {
    return (ch >= '0' && ch <= '9') ||
           (ch >= 'a' && ch <= 'f') ||
           (ch >= 'A' && ch <= 'F');
}

std::string escapeControlCharacter(unsigned char ch) {
    switch (ch) {
        case '\b': return "\\b";
        case '\f': return "\\f";
        case '\n': return "\\n";
        case '\r': return "\\r";
        case '\t': return "\\t";
        default: {
            static constexpr char hex[] = "0123456789abcdef";
            std::string escaped = "\\u00";
            escaped.push_back(hex[(ch >> 4) & 0x0f]);
            escaped.push_back(hex[ch & 0x0f]);
            return escaped;
        }
    }
}

nlohmann::json parseComplete(std::string_view input) {
    if (input.empty()) return nlohmann::json();
    return nlohmann::json::parse(input.begin(), input.end(), nullptr, false);
}

class PartialParser {
public:
    explicit PartialParser(std::string_view input) : input_(input) {}

    nlohmann::json parse() {
        skipWhitespace();
        if (index_ >= input_.size()) throw std::runtime_error("empty partial json");
        return parseValue();
    }

private:
    nlohmann::json parseValue() {
        skipWhitespace();
        if (index_ >= input_.size()) throw std::runtime_error("incomplete json value");

        const char ch = input_[index_];
        if (ch == '"') return parseString();
        if (ch == '{') return parseObject();
        if (ch == '[') return parseArray();
        if (matchesLiteral("null")) return nullptr;
        if (matchesLiteral("true")) return true;
        if (matchesLiteral("false")) return false;
        return parseNumber();
    }

    bool matchesLiteral(std::string_view literal) {
        const auto remaining = input_.substr(index_);
        const auto compareLength = std::min(remaining.size(), literal.size());
        if (remaining.substr(0, compareLength) != literal.substr(0, compareLength)) return false;
        if (remaining.size() < literal.size()) {
            index_ = input_.size();
            return true;
        }
        if (remaining.substr(0, literal.size()) == literal) {
            index_ += literal.size();
            return true;
        }
        return false;
    }

    nlohmann::json parseString() {
        const auto start = index_;
        ++index_;
        bool escaped = false;

        while (index_ < input_.size()) {
            const char ch = input_[index_];
            if (escaped) {
                escaped = false;
                ++index_;
                continue;
            }
            if (ch == '\\') {
                escaped = true;
                ++index_;
                continue;
            }
            if (ch == '"') {
                ++index_;
                const auto parsed = parseComplete(input_.substr(start, index_ - start));
                if (parsed.is_discarded() || !parsed.is_string()) {
                    throw std::runtime_error("malformed json string");
                }
                return parsed;
            }
            ++index_;
        }

        std::string completed(input_.substr(start));
        completed.push_back('"');
        const auto parsed = parseComplete(completed);
        if (parsed.is_discarded() || !parsed.is_string()) {
            throw std::runtime_error("malformed partial json string");
        }
        return parsed;
    }

    nlohmann::json parseObject() {
        ++index_;
        nlohmann::json object = nlohmann::json::object();

        for (;;) {
            skipWhitespace();
            if (index_ >= input_.size()) return object;
            if (input_[index_] == '}') {
                ++index_;
                return object;
            }
            if (input_[index_] != '"') return object;

            nlohmann::json keyJson;
            try {
                keyJson = parseString();
            } catch (...) {
                return object;
            }
            if (!keyJson.is_string()) return object;
            const auto key = keyJson.get<std::string>();

            skipWhitespace();
            if (index_ >= input_.size() || input_[index_] != ':') return object;
            ++index_;
            skipWhitespace();
            if (index_ >= input_.size()) return object;

            try {
                object[key] = parseValue();
            } catch (...) {
                return object;
            }

            skipWhitespace();
            if (index_ >= input_.size()) return object;
            if (input_[index_] == ',') {
                ++index_;
                continue;
            }
            if (input_[index_] == '}') {
                ++index_;
                return object;
            }
            return object;
        }
    }

    nlohmann::json parseArray() {
        ++index_;
        nlohmann::json array = nlohmann::json::array();

        for (;;) {
            skipWhitespace();
            if (index_ >= input_.size()) return array;
            if (input_[index_] == ']') {
                ++index_;
                return array;
            }

            try {
                array.push_back(parseValue());
            } catch (...) {
                return array;
            }

            skipWhitespace();
            if (index_ >= input_.size()) return array;
            if (input_[index_] == ',') {
                ++index_;
                continue;
            }
            if (input_[index_] == ']') {
                ++index_;
                return array;
            }
            return array;
        }
    }

    nlohmann::json parseNumber() {
        const auto start = index_;
        while (index_ < input_.size()) {
            const char ch = input_[index_];
            if (ch == ',' || ch == ']' || ch == '}' ||
                ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t') {
                break;
            }
            ++index_;
        }

        if (index_ == start) throw std::runtime_error("invalid json atom");
        const auto token = input_.substr(start, index_ - start);
        auto parsed = parseComplete(token);
        if (!parsed.is_discarded() && parsed.is_number()) return parsed;

        const std::string text(token);
        char* end = nullptr;
        (void)std::strtod(text.c_str(), &end);
        if (end == text.c_str()) throw std::runtime_error("invalid partial json number");

        const auto consumed = static_cast<std::size_t>(end - text.c_str());
        parsed = parseComplete(std::string_view(text.data(), consumed));
        if (parsed.is_discarded() || !parsed.is_number()) {
            throw std::runtime_error("invalid partial json number");
        }
        return parsed;
    }

    void skipWhitespace() {
        while (index_ < input_.size()) {
            const char ch = input_[index_];
            if (ch != ' ' && ch != '\n' && ch != '\r' && ch != '\t') break;
            ++index_;
        }
    }

    std::string_view input_;
    std::size_t index_ = 0;
};

nlohmann::json partialParse(std::string_view input) {
    try {
        return PartialParser(input).parse();
    } catch (...) {
        return nlohmann::json();
    }
}

} // namespace

std::string repairJson(std::string_view json) {
    std::string repaired;
    repaired.reserve(json.size());
    bool inString = false;

    for (std::size_t index = 0; index < json.size(); ++index) {
        const char ch = json[index];

        if (!inString) {
            repaired.push_back(ch);
            if (ch == '"') inString = true;
            continue;
        }

        if (ch == '"') {
            repaired.push_back(ch);
            inString = false;
            continue;
        }

        if (ch == '\\') {
            if (index + 1 >= json.size()) {
                repaired += "\\\\";
                continue;
            }

            const char next = json[index + 1];
            if (next == 'u' && index + 5 < json.size() &&
                isHexDigit(json[index + 2]) &&
                isHexDigit(json[index + 3]) &&
                isHexDigit(json[index + 4]) &&
                isHexDigit(json[index + 5])) {
                repaired.append(json.substr(index, 6));
                index += 5;
                continue;
            }

            constexpr std::string_view validEscapes = "\"\\/bfnrt";
            if (validEscapes.find(next) != std::string_view::npos) {
                repaired.push_back('\\');
                repaired.push_back(next);
                ++index;
                continue;
            }

            repaired += "\\\\";
            continue;
        }

        const auto byte = static_cast<unsigned char>(ch);
        repaired += isControlCharacter(byte)
            ? escapeControlCharacter(byte)
            : std::string(1, ch);
    }

    return repaired;
}

nlohmann::json parseStreamingJson(std::string_view partialJson) {
    const auto first = partialJson.find_first_not_of(" \n\r\t");
    if (first == std::string_view::npos) return nlohmann::json::object();

    auto parsed = parseComplete(partialJson);
    if (!parsed.is_discarded()) return parsed;

    const auto repaired = repairJson(partialJson);
    if (repaired != partialJson) {
        parsed = parseComplete(repaired);
        if (!parsed.is_discarded()) return parsed;
    }

    parsed = partialParse(partialJson);
    if (!parsed.is_null()) return parsed;

    if (repaired != partialJson) {
        parsed = partialParse(repaired);
        if (!parsed.is_null()) return parsed;
    }

    return nlohmann::json::object();
}

} // namespace pi::ai::detail
