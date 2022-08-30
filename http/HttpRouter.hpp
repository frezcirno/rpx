#ifndef __ROUTER_HPP__
#define __ROUTER_HPP__

#include <memory>
#include <string>
#include <functional>
#include <unordered_map>
#include <zlog.h>
#include <pcre.h>
#include "HttpParser.hpp"
#include "TcpConnection.hpp"
#include "HttpServer.hpp"

class HttpHandler : noncopyable
{
protected:
  typedef class HttpContext<HttpRequest> HttpContext;

public:
  virtual ~HttpHandler() {}
  virtual void handleRequest(int prefixLen, HttpContext& ctx, HttpServer* server) = 0;
};

class HttpRouter : noncopyable
{
  typedef class HttpContext<HttpRequest> HttpContext;

private:
  class Route : noncopyable
  {
  public:
    virtual ~Route() {}
    virtual int match(const std::string& url) const = 0;
    virtual void handleRequest(int prefixLen, HttpContext& ctx, HttpServer* server) = 0;
  };

  class SimpleRoute : public Route
  {
  public:
    SimpleRoute(const std::string& url, HttpHandler* handler)
      : _url(url)
      , _handler(handler)
    {}
    ~SimpleRoute() {}
    int match(const std::string& url) const override
    {
      if (url.size() < _url.size())
        return 0;
      if (strncmp(url.c_str(), _url.c_str(), _url.size()) != 0)
        return 0;
      if (url.size() > _url.size() && !ispunct(url[_url.size()]))
        return 0;
      return _url.size();
    }
    void handleRequest(int prefixLen, HttpContext& ctx, HttpServer* server) override
    {
      _handler->handleRequest(prefixLen, ctx, server);
    }

  private:
    std::string _url;
    std::shared_ptr<HttpHandler> _handler;
  };

  class RegexRoute : public Route
  {
  public:
    RegexRoute(const std::string& pattern, HttpHandler* handler)
      : _handler(handler)
    {
      const char* errptr;
      int erroffset;
      _pcre = pcre_compile(pattern.c_str(), PCRE_CASELESS, &errptr, &erroffset, NULL);
      if (_pcre == NULL) {
        std::cout << "pcre_compile error: " << errptr << std::endl;
        abort();
      }
    }
    ~RegexRoute()
    {
      if (_pcre)
        pcre_free(_pcre);
    }
    int match(const std::string& url) const
    {
      int ovector[3];
      int rc = pcre_exec(_pcre, NULL, url.c_str(), url.size(), 0, PCRE_ANCHORED, ovector, 3);
      if (rc < 0) {
        if (rc == PCRE_ERROR_NOMATCH)
          return 0;
        printf("pcre match error: %d\n", rc);
        abort();
      }
      assert(rc == 1);
      return ovector[1];
    }
    void handleRequest(int prefixLen, HttpContext& ctx, HttpServer* server)
    {
      _handler->handleRequest(prefixLen, ctx, server);
    }

  private:
    pcre* _pcre;
    std::shared_ptr<HttpHandler> _handler;
  };

public:
  HttpRouter(HttpServer* server)
    : _server(server)
    , _zc(zlog_get_category("HttpRouter"))
  {}
  ~HttpRouter() {}

  void addSimpleRoute(const std::string& pattern, HttpHandler* handler)
  {
    _routes.push_back(std::make_unique<SimpleRoute>(pattern, handler));
  }

  void addRegexRoute(const std::string& pattern, HttpHandler* handler)
  {
    _routes.push_back(std::make_unique<RegexRoute>(pattern, handler));
  }

  void handleRequest(HttpContext& ctx)
  {
    const auto& path = ctx.parser.getMessage()->path;
    for (auto& route : _routes) {
      int len = route->match(path);
      if (len) {
        route->handleRequest(len, ctx, _server);
        return;
      }
    }
    ctx.sendError(404);
  }

private:
  typedef std::unique_ptr<Route> RoutePtr;
  std::vector<RoutePtr> _routes;
  HttpServer* _server;
  zlog_category_t* _zc;
};

#endif
