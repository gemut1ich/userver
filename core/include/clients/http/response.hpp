#pragma once

#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>

#include <curl-ev/easy.hpp>
#include <curl-ev/local_stats.hpp>

#include <utils/str_icase.hpp>

#include "error.hpp"
#include "wrapper.hpp"

namespace clients::http {

/// https://en.wikipedia.org/wiki/List_of_HTTP_status_codes
enum Status : uint16_t {
  OK = 200,
  Created = 201,
  NoContent = 204,
  BadRequest = 400,
  NotFound = 404,
  Conflict = 409,
  InternalServerError = 500,
  BadGateway = 502,
  ServiceUnavailable = 503,
  GatewayTimeout = 504,
  InsufficientStorage = 507,
  BandwithLimitExceeded = 509,
  WebServerIsDown = 520,
  ConnectionTimedOut = 522,
  OriginIsUnreachable = 523,
  TimeoutOccured = 524,
  SslHandshakeFailed = 525,
  InvalidSslCertificate = 526,
};

std::ostream& operator<<(std::ostream& os, Status s);

/// Headers class
using Headers = std::unordered_map<std::string, std::string,
                                   utils::StrIcaseHash, utils::StrIcaseEqual>;

/// Class that will be returned for successful request
class Response final {
 public:
  Response(std::shared_ptr<EasyWrapper> easy) : easy_(std::move(easy)) {}

  /// response stream
  std::ostringstream& sink_stream() { return response_stream_; }

  /// body as string
  std::string body() const { return response_stream_.str(); }
  /// return referece to headers
  const Headers& headers() const { return headers_; }
  Headers& headers() { return headers_; }

  /// status_code
  Status status_code() const;
  /// check status code
  bool IsOk() const { return status_code() == Status::OK; }

  static void RaiseForStatus(long code);

  void raise_for_status() const;

  curl::easy& easy();
  const curl::easy& easy() const;

  /// returns statistics on request execution like const of opened sockets,
  /// connect time...
  curl::LocalStats GetStats() const;

 private:
  Headers headers_;
  std::ostringstream response_stream_;
  std::shared_ptr<EasyWrapper> easy_;
};

}  // namespace clients::http

namespace curl {
std::basic_ostream<char, std::char_traits<char>>& operator<<(
    std::ostream& stream, const curl::easy& ceasy);
}  // namespace curl

std::ostream& operator<<(std::ostream& stream,
                         const clients::http::Response& response);
