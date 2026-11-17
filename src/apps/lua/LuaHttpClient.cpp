#include "LuaHttpClient.h"

#include <Logging.h>
#include <Memory.h>
#include <esp_wifi.h>

#include <cctype>
#include <string>

#if defined(FREEINK_NET_WOLFSSL)
#include <SecureHttpClient.h>
#else
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#endif

namespace {

#if !defined(FREEINK_NET_WOLFSSL)
constexpr int HTTP_RX_BUF = 2048;
constexpr int HTTP_TX_BUF = 512;
constexpr size_t READ_CHUNK = 1024;
#endif
constexpr int HTTP_TIMEOUT_MS = 60000;
constexpr int MAX_REDIRECTS = 5;

struct WifiPowerSaveGuard {
  WifiPowerSaveGuard() {
    esp_err_t err = esp_wifi_set_ps(WIFI_PS_NONE);
    if (err != ESP_OK) LOG_ERR("LUA_HTTP", "Failed to disable WiFi power-save: %d", err);
  }
  ~WifiPowerSaveGuard() {
    esp_err_t err = esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    if (err != ESP_OK) LOG_ERR("LUA_HTTP", "Failed to restore WiFi power-save: %d", err);
  }
};

bool equalsIgnoreCase(const std::string& a, const char* b) {
  size_t i = 0;
  for (; i < a.size() && b[i] != '\0'; ++i) {
    if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
      return false;
    }
  }
  return i == a.size() && b[i] == '\0';
}

#if !defined(FREEINK_NET_WOLFSSL)
bool isRedirect(int status) {
  return status == 301 || status == 302 || status == 303 || status == 307 || status == 308;
}

bool writeRequestBody(esp_http_client_handle_t client, const std::string& body) {
  size_t written = 0;
  while (written < body.size()) {
    const int w = esp_http_client_write(client, body.data() + written, body.size() - written);
    if (w <= 0) {
      LOG_ERR("LUA_HTTP", "write failed");
      return false;
    }
    written += static_cast<size_t>(w);
  }
  return true;
}
#endif

}  // namespace

namespace LuaHttpClient {

bool request(const std::string& method, const std::string& url, const HeaderList& headers, const std::string& body,
             int& outStatus, std::string& outBody) {
  outStatus = 0;
  outBody.clear();
  WifiPowerSaveGuard psGuard;

#if defined(FREEINK_NET_WOLFSSL)
  freeink::SecureHttpClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setInsecure();
  http.setFollowRedirects(MAX_REDIRECTS);
  if (!http.begin(url)) {
    LOG_ERR("LUA_HTTP", "wolfSSL bad URL: %s", url.c_str());
    return false;
  }

  bool hasUserAgent = false;
  for (const auto& kv : headers) {
    if (equalsIgnoreCase(kv.first, "User-Agent")) {
      http.setUserAgent(kv.second);
      hasUserAgent = true;
    } else {
      http.addHeader(kv.first, kv.second);
    }
  }
  if (!hasUserAgent) {
    http.setUserAgent("CrossPoint-ESP32-" CROSSPOINT_VERSION);
  }

  LOG_DBG("LUA_HTTP", "wolfSSL %s: %s", method.c_str(), url.c_str());
  const int status = http.sendRequest(
      method.c_str(), body.empty() ? nullptr : reinterpret_cast<const uint8_t*>(body.data()), body.size());
  if (status < 0) {
    LOG_ERR("LUA_HTTP", "wolfSSL request failed: %s %s", method.c_str(), url.c_str());
    return false;
  }
  outStatus = status;
  outBody = http.getString();
  return true;
#else
  esp_http_client_config_t config = {};
  config.url = url.c_str();
  config.buffer_size = HTTP_RX_BUF;
  config.buffer_size_tx = HTTP_TX_BUF;
  config.timeout_ms = HTTP_TIMEOUT_MS;
  config.crt_bundle_attach = esp_crt_bundle_attach;
  config.keep_alive_enable = true;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (!client) {
    LOG_ERR("LUA_HTTP", "client init failed");
    return false;
  }

  esp_http_client_method_t espMethod = HTTP_METHOD_GET;
  if (equalsIgnoreCase(method, "POST"))
    espMethod = HTTP_METHOD_POST;
  else if (equalsIgnoreCase(method, "PUT"))
    espMethod = HTTP_METHOD_PUT;
  else if (equalsIgnoreCase(method, "DELETE"))
    espMethod = HTTP_METHOD_DELETE;
  else if (equalsIgnoreCase(method, "PATCH"))
    espMethod = HTTP_METHOD_PATCH;
  else if (equalsIgnoreCase(method, "HEAD"))
    espMethod = HTTP_METHOD_HEAD;
  esp_http_client_set_method(client, espMethod);

  bool hasUserAgent = false;
  for (const auto& kv : headers) {
    if (equalsIgnoreCase(kv.first, "User-Agent")) hasUserAgent = true;
    esp_http_client_set_header(client, kv.first.c_str(), kv.second.c_str());
  }
  if (!hasUserAgent) {
    esp_http_client_set_header(client, "User-Agent", "CrossPoint-ESP32-" CROSSPOINT_VERSION);
  }

  esp_err_t err = esp_http_client_open(client, body.size());
  if (err != ESP_OK || (!body.empty() && !writeRequestBody(client, body))) {
    LOG_ERR("LUA_HTTP", "open/write failed: %s", esp_err_to_name(err));
    esp_http_client_cleanup(client);
    return false;
  }

  esp_http_client_fetch_headers(client);
  int status = esp_http_client_get_status_code(client);
  for (int hop = 0; isRedirect(status) && hop < MAX_REDIRECTS; ++hop) {
    if (esp_http_client_set_redirection(client) != ESP_OK) break;
    esp_http_client_close(client);
    if (status == 301 || status == 302 || status == 303) {
      esp_http_client_set_method(client, HTTP_METHOD_GET);
      err = esp_http_client_open(client, 0);
    } else {
      err = esp_http_client_open(client, body.size());
      if (err == ESP_OK && !body.empty() && !writeRequestBody(client, body)) {
        esp_http_client_cleanup(client);
        return false;
      }
    }
    if (err != ESP_OK) {
      esp_http_client_cleanup(client);
      return false;
    }
    esp_http_client_fetch_headers(client);
    status = esp_http_client_get_status_code(client);
  }

  auto buf = makeUniqueNoThrow<char[]>(READ_CHUNK);
  if (!buf) {
    esp_http_client_cleanup(client);
    return false;
  }
  while (true) {
    const int read = esp_http_client_read(client, buf.get(), READ_CHUNK);
    if (read < 0) {
      esp_http_client_cleanup(client);
      return false;
    }
    if (read == 0) break;
    outBody.append(buf.get(), static_cast<size_t>(read));
  }

  esp_http_client_cleanup(client);
  outStatus = status;
  return true;
#endif
}

}  // namespace LuaHttpClient
