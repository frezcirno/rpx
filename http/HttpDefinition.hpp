#ifndef __HTTPDEFINITION_HPP__
#define __HTTPDEFINITION_HPP__

#include <unordered_map>

#define HTTPLIST(expander)                                                                         \
  expand(CONTINUE, 100, "Continue", "Request has been received and being processed")               \
  expand(SWITCHING_PROTOCOLS, 101, "Switching Protocols", "Switching to new protocol.")            \
  expand(OK, 200, "OK", "Request fulfilled, document follows.")                                    \
  expand(CREATED, 201, "Created", "Document created, URL follows.")                                \
  expand(ACCEPTED, 202, "Accepted", "Request accepted, processing continues off-line.")            \
  expand(NON_AUTHORITATIVE_INFORMATION,                                                            \
         203,                                                                                      \
         "Non-Authoritative Information",                                                          \
         "Request fulfilled from cache.")                                                          \
  expand(NO_CONTENT, 204, "No Content", "Request fulfilled, nothing follows.")                     \
  expand(RESET_CONTENT, 205, "Reset Content", "Clear input form for further input.")               \
  expand(PARTIAL_CONTENT, 206, "Partial Content", "Partial content follows.")                      \
  expand(                                                                                          \
    MULTIPLE_CHOICES, 300, "Multiple Choices", "Object has several resources -- see URI list.")    \
  expand(                                                                                          \
    MOVED_PERMANENTLY, 301, "Moved Permanently", "Object moved permanently -- see URI list.")      \
  expand(FOUND, 302, "Found", "Object moved temporarily -- see URI list.")                         \
  expand(SEE_OTHER, 303, "See Other", "Object moved -- see Method and URL list.")                  \
  expand(NOT_MODIFIED, 304, "Not Modified", "Document has not changed since given time.")          \
  expand(USE_PROXY, 305, "Use Proxy", "You must use proxy specified in Location.")                 \
  expand(                                                                                          \
    TEMPORARY_REDIRECT, 307, "Temporary Redirect", "Object moved temporarily -- see URI list.")    \
  expand(                                                                                          \
    PERMANENT_REDIRECT, 308, "Permanent Redirect", "Object moved permanently -- see URI list.")    \
  expand(BAD_REQUEST, 400, "Bad Request", "Bad request syntax or unsupported method.")             \
  expand(UNAUTHORIZED, 401, "Unauthorized", "No permission -- see authorization schemes.")         \
  expand(PAYMENT_REQUIRED, 402, "Payment Required", "No payment -- see charging schemes.")         \
  expand(FORBIDDEN, 403, "Forbidden", "Request forbidden -- authorization will not help.")         \
  expand(NOT_FOUND, 404, "Not Found", "Document not found.")                                       \
  expand(METHOD_NOT_ALLOWED, 405, "Method Not Allowed", "Method not allowed for this resource.")   \
  expand(NOT_ACCEPTABLE,                                                                           \
         406,                                                                                      \
         "Not Acceptable",                                                                         \
         "Cannot generate response -- client may not support media type.")                         \
  expand(PROXY_AUTHENTICATION_REQUIRED,                                                            \
         407,                                                                                      \
         "Proxy Authentication Required",                                                          \
         "You must authenticate with this proxy.")                                                 \
  expand(REQUEST_TIMEOUT, 408, "Request Timeout", "Request timed out; try again later.")           \
  expand(CONFLICT, 409, "Conflict", "Request conflict.")                                           \
  expand(GONE, 410, "Gone", "URI no longer exists and has been permanently removed.")              \
  expand(LENGTH_REQUIRED, 411, "Length Required", "Client must specify Content-Length.")           \
  expand(PRECONDITION_FAILED, 412, "Precondition Failed", "Precondition in headers is false.")     \
  expand(REQUEST_ENTITY_TOO_LARGE, 413, "Request Entity Too Large", "Entity is too large.")        \
  expand(REQUEST_URI_TOO_LONG, 414, "Request-URI Too Long", "URI is too long.")                    \
  expand(                                                                                          \
    UNSUPPORTED_MEDIA_TYPE, 415, "Unsupported Media Type", "Entity body in unsupported format.")   \
  expand(REQUESTED_RANGE_NOT_SATISFIABLE,                                                          \
         416,                                                                                      \
         "Requested Range Not Satisfiable",                                                        \
         "Cannot satisfy request range.")                                                          \
  expand(                                                                                          \
    EXPECTATION_FAILED, 417, "Expectation Failed", "Expect condition could not be satisfied.")     \
  expand(UNPROCESSABLE_ENTITY, 422, "Unprocessable Entity", "Unprocessable entity.")               \
  expand(LOCKED, 423, "Locked", "Locked.")                                                         \
  expand(FAILED_DEPENDENCY, 424, "Failed Dependency", "Failed dependency.")                        \
  expand(UPGRADE_REQUIRED, 426, "Upgrade Required", "Client should upgrade to use new protocol.")  \
  expand(                                                                                          \
    PRECONDITION_REQUIRED, 428, "Precondition Required", "Precondition in headers is false.")      \
  expand(TOO_MANY_REQUESTS, 429, "Too Many Requests", "Too many requests.")                        \
  expand(REQUEST_HEADER_FIELDS_TOO_LARGE,                                                          \
         431,                                                                                      \
         "Request Header Fields Too Large",                                                        \
         "Request header fields too large.")                                                       \
  expand(INTERNAL_SERVER_ERROR, 500, "Internal Server Error", "Server got itself in trouble.")     \
  expand(NOT_IMPLEMENTED, 501, "Not Implemented", "Server does not support this operation.")       \
  expand(BAD_GATEWAY, 502, "Bad Gateway", "Invalid responses from another server/proxy.")          \
  expand(SERVICE_UNAVAILABLE,                                                                      \
         503,                                                                                      \
         "Service Unavailable",                                                                    \
         "The server cannot process the request due to a high load.")                              \
  expand(GATEWAY_TIMEOUT,                                                                          \
         504,                                                                                      \
         "Gateway Timeout",                                                                        \
         "The gateway server did not receive a timely response.")                                  \
  expand(HTTP_VERSION_NOT_SUPPORTED, 505, "HTTP Version Not Supported", "Cannot fulfill request.")

enum HttpStatus
{
#define expand(name, code, __, ___) name = code,
  HTTPLIST(expand)
#undef expand
};

namespace HttpDefinition
{

struct HttpStatusMessage
{
  const int code;
  const char* message;
  const char* description;
};

const std::unordered_map<int, HttpStatusMessage> httpStatusMessage{
#define expand(name, code, message, description) {code, {code, message, description}},
  HTTPLIST(expand)
#undef expand
};

const char* getMessage(int code)
{
  if (code < 0 || code >= 600 || !httpStatusMessage.count(code)) {
    return nullptr;
  }
  std::unordered_map<int, int> m;
  return httpStatusMessage.at(code).message;
}
};   // namespace HttpDefinition

#endif