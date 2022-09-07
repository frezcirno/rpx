#ifndef __PROXYHANDLER_HPP__
#define __PROXYHANDLER_HPP__

#include <string>
#include <functional>
#include <sstream>
#include <iostream>
#include "HttpRouter.hpp"
#include "HttpContext.hpp"
#include "HttpClient.hpp"

class ProxyHandler
{
  typedef std::shared_ptr<HttpClient> HttpClientPtr;
  typedef std::shared_ptr<HttpRequest> HttpRequestPtr;
  typedef std::shared_ptr<HttpResponse> HttpResponsePtr;
  typedef std::shared_ptr<HttpContext<HttpRequest>> HttpContextPtr;

public:
  ProxyHandler(const std::string& host, uint16_t port)
    : _host(host)
    , _port(port)
  {
    if (port == 80 || port == 443)
      _hostPort = host;
    else
      _hostPort = host + ":" + std::to_string(port);
  }
  ~ProxyHandler() {}

  void operator()(int prefixLen, HttpContextPtr ctx, HttpServer* server)
  {
    InetAddress _upAddr;
    if (!_upAddr.parseHost(_host.c_str(), _port)) {
      ctx->send(
        "HTTP/1.1 502 Bad Gateway\r\nContent-Type: text/plain\r\nContent-Length: 0\r\n\r\n");
      return;
    }
    HttpRequestPtr msg = ctx->getMessage();
    std::string upPath = msg->path.substr(prefixLen);
    if (upPath.empty())
      upPath.assign("/");
    HttpClientPtr client = std::make_shared<HttpClient>(ctx->getLoop(), _upAddr);
    msg->headers.insert_or_assign("Host", _hostPort);
    msg->headers.insert_or_assign("X-Forwarded-For", ctx->getConn()->getPeerAddr().toIpPort());
    client->setConnectCallback([msg, upPath](auto upCtx) {
      upCtx->startRequest(msg->method, upPath);
      for (auto& [key, value] : msg->headers)
        upCtx->sendHeader(key, value);
      upCtx->endHeaders();
      upCtx->send(msg->body);
    });
    client->setResponseCallback([ctx](auto upCtx) {
      HttpResponsePtr msg = upCtx->getMessage();
      ctx->startResponse(msg->status_code, msg->status_message);
      for (auto& [key, value] : msg->headers)
        ctx->sendHeader(key, value);
      ctx->endHeaders();
      ctx->send(msg->body);
    });
    client->setCloseCallback([ctx](auto upCtx) { ctx->shutdown(); });
    ctx->setUserData(client);
    ctx->setCloseCallback([ctx]() {
      HttpClientPtr upClient = std::any_cast<HttpClientPtr>(ctx->getUserData());
      upClient->setConnectCallback(nullptr);
      upClient->setResponseCallback(nullptr);
      upClient->setWriteCompleteCallback(nullptr);
      upClient->setCloseCallback([upClient](auto upCtx) {
        // Hack: Hold a reference of itself, and release it after closeCallback()
        upClient->setCloseCallback(nullptr);
      });
      upClient->shutdown();
    });
    client->start();
  }

private:
  std::string _host;
  uint16_t _port;
  std::string _hostPort;
};

#endif
