#pragma once

#include <string>
#include <utility>
#include <vector>

namespace LuaHttpClient {

using HeaderList = std::vector<std::pair<std::string, std::string>>;

/**
 * Perform an HTTP/HTTPS request with an arbitrary method, custom headers, and
 * optional request body. Returns true if a valid HTTP response was received
 * (with outStatus and outBody populated), or false on transport/URL error.
 */
bool request(const std::string& method, const std::string& url, const HeaderList& headers, const std::string& body,
             int& outStatus, std::string& outBody);

}  // namespace LuaHttpClient
