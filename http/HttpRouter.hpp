#ifndef __ROUTER_HPP__
#define __ROUTER_HPP__

#define PCRE2_CODE_UNIT_WIDTH 8

#include <memory>
#include <string>
#include <functional>
#include <unordered_map>
#include <pcre2.h>
#include "HttpParser.hpp"
#include "TcpConnection.hpp"
#include "HttpServer.hpp"

class HttpHandler : noncopyable
{
public:
  virtual ~HttpHandler() {}
  virtual void handleRequest(const HttpParser& request, TcpConnectionPtr& conn) = 0;
};

class HttpRouter : noncopyable
{
private:
  class Route
  {
  public:
    Route(const std::string& pattern, HttpHandler* handler)
      : _handler(handler)
    {
      int errcode;
      size_t erroffset;
      _pcre = pcre2_compile_8(
        (PCRE2_SPTR8)pattern.c_str(), pattern.size(), 0, &errcode, &erroffset, NULL);
      if (_pcre == NULL) {
        PCRE2_UCHAR buffer[256];
        pcre2_get_error_message(errcode, buffer, sizeof(buffer));
        abort();
      }
      _match_data = pcre2_match_data_create_from_pattern(_pcre, NULL);
    }
    ~Route()
    {
      pcre2_match_data_free(_match_data);
      pcre2_code_free_8(_pcre);
    }
    Route(Route&&) = default;
    Route& operator=(Route&&) = default;

    bool match(const std::string& url) const
    {
      PCRE2_SPTR8 url_ptr = (PCRE2_SPTR8)url.c_str();
      int rc = pcre2_match_8(_pcre, url_ptr, url.size(), 0, PCRE2_ANCHORED, _match_data, NULL);
      if (rc < 0) {
        if (rc == PCRE2_ERROR_NOMATCH)
          return false;
        printf("pcre match error: %d\n", rc);
        abort();
      }
      return rc;
    }
    void handleRequest(const HttpParser& request, TcpConnectionPtr& conn)
    {
      _handler->handleRequest(request, conn);
    }

  private:
    pcre2_code_8* _pcre;
    pcre2_match_data_8* _match_data;
    std::unique_ptr<HttpHandler> _handler;
  };

public:
  HttpRouter() {}
  ~HttpRouter() {}

  void addRoute(const std::string& pattern, HttpHandler* handler)
  {
    _routes.emplace_back(pattern, handler);
  }

  void handleRequest(const HttpParser& request, TcpConnectionPtr& conn)
  {
    for (auto& route : _routes) {
      if (route.match(request.getPath())) {
        route.handleRequest(request, conn);
        return;
      }
    }
    conn->write("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n");
  }

private:
  std::vector<Route> _routes;
};

#endif
