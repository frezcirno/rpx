#ifndef __PROXYHANDLER_HPP__
#define __PROXYHANDLER_HPP__

#include <string>
#include <functional>
#include <sstream>
#include <iostream>
#include "HttpRouter.hpp"
#include "HttpContext.hpp"
#include "HttpClient.hpp"

class ProxyHandler : public HttpHandler
{
  typedef std::shared_ptr<HttpClient> HttpClientPtr;
  typedef std::shared_ptr<HttpRequest> HttpRequestPtr;
  typedef std::shared_ptr<HttpResponse> HttpResponsePtr;

public:
  ProxyHandler(const std::string& host, uint16_t port)
    : _host(host)
    , _port(port)
    , _upAddr(host.c_str(), port)
  {}
  ~ProxyHandler() {}

  void handleRequest(int prefixLen, HttpContext& ctx, HttpServer* server)
  {
    HttpRequestPtr msg = ctx.parser.getMessage();
    dzlog_info("ProxyHandler: %.*s", (int)msg->path.size(), msg->path.c_str());
    std::string upPath = msg->path.substr(prefixLen);
    if (upPath.empty())
      upPath.assign("/");
    HttpClientPtr client = std::make_shared<HttpClient>(ctx.getLoop(), _upAddr);
    msg->headers.insert_or_assign("Host", _upAddr.toIpPort());
    msg->headers.insert_or_assign("X-Forwarded-For", ctx.getConn()->getPeerAddr().toIpPort());
    client->setConnectCallback([msg, upPath](auto& upCtx) {
      upCtx.startRequest(msg->method, upPath);
      for (auto& [key, value] : msg->headers)
        upCtx.sendHeader(key, value);
      upCtx.endHeaders();
      upCtx.send(msg->body);
    });
    client->setResponseCallback([&](auto& upCtx) {
      HttpResponsePtr msg = upCtx.parser.getMessage();
      ctx.startResponse(msg->status_code, msg->status_message);
      for (auto& [key, value] : msg->headers)
        ctx.sendHeader(key, value);
      ctx.endHeaders();
      ctx.send(msg->body);
    });
    client->setCloseCallback([&](auto& upCtx) { ctx.shutdown(); });
    ctx.setUserData(client);
    ctx.setDestroyCallback([](HttpContext& ctx) {
      HttpClientPtr upClient = std::any_cast<HttpClientPtr>(ctx.getUserData());
      upClient->setConnectCallback(NULL);
      upClient->setResponseCallback(NULL);
      upClient->setCloseCallback([upClient](auto& upCtx) {
        // Hack: Hold a reference of itself, and release it after closeCallback()
        upClient->setCloseCallback(NULL);
      });
      upClient->shutdown();
    });
    client->start();
  }

private:
  std::string _host;
  uint16_t _port;
  InetAddress _upAddr;
};

#endif
