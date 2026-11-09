#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <string_view>

namespace pi::ai::detail {

// Repair malformed JSON string literals in the same compatibility spirit as
// pi v0.80.0: raw control characters are escaped and invalid backslash escapes
// are preserved as literal backslashes.
std::string repairJson(std::string_view json);

// Parse complete or incomplete streaming JSON. The function is intentionally
// private to the OpenAI-compatible implementation and always returns an object
// on unrecoverable input, matching pi's parseStreamingJson fallback contract.
nlohmann::json parseStreamingJson(std::string_view partialJson);

} // namespace pi::ai::detail
