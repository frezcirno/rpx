#ifndef __ROUTER_HPP__
#define __ROUTER_HPP__

#include <memory>
#include <string>
#include <functional>
#include <unordered_map>
#include <pcre.h>
#include "HttpParser.hpp"
#include "TcpConnection.hpp"
#include "HttpServer.hpp"

class HttpHandler : noncopyable
{
public:
  virtual ~HttpHandler() {}
  virtual void handleRequest(HttpContext& ctx) = 0;
};

class HttpRouter : noncopyable
{
private:
  // Route: move-only type
  class Route
  {
  public:
    Route(std::string&& pattern, HttpHandler* handler)
      : _handler(handler)
    {
      const char* errptr;
      int erroffset;
      _pcre = pcre_compile(pattern.c_str(), 0, &errptr, &erroffset, NULL);
      if (_pcre == NULL) {
        std::cout << "pcre_compile error: " << errptr << std::endl;
        abort();
      }
    }
    ~Route()
    {
      if (_pcre)
        pcre_free(_pcre);
    }
    Route(Route&& other)
      : _handler(other._handler)
      , _pcre(other._pcre)
    {
      other._pcre = NULL;
    }
    Route& operator=(Route&& other)
    {
      _handler = other._handler;
      _pcre = other._pcre;
      other._pcre = NULL;
      return *this;
    }
    bool match(const std::string& url) const
    {
      int ovector[3];
      int rc = pcre_exec(_pcre, NULL, url.c_str(), url.size(), 0, 0, ovector, 3);
      if (rc < 0) {
        if (rc == PCRE_ERROR_NOMATCH)
          return false;
        printf("pcre match error: %d\n", rc);
        abort();
      }
      return true;
    }
    void handleRequest(HttpContext& ctx)
    {
      _handler->handleRequest(ctx);
    }

  private:
    pcre* _pcre;
    std::shared_ptr<HttpHandler> _handler;
  };

public:
  HttpRouter() {}
  ~HttpRouter() {}

  void addRoute(std::string&& pattern, HttpHandler* handler)
  {
    _routes.emplace_back(std::move(pattern), handler);
  }

  void handleRequest(HttpContext& ctx)
  {
    const auto& path = ctx.parser.getPath();
    for (auto& route : _routes) {
      if (route.match(path)) {
        route.handleRequest(ctx);
        return;
      }
    }
    ctx.sendError(404);
  }

private:
  std::vector<Route> _routes;
};

#endif
