#ifndef __PROXYHANDLER_HPP__
#define __PROXYHANDLER_HPP__

#include <string>
#include <functional>
#include <sstream>
#include <iostream>
#include <memory>
#include "HttpRouter.hpp"
#include "HttpContext.hpp"
#include "TcpClient.hpp"

class ProxyHandler
{
  typedef std::shared_ptr<TcpClient> TcpClientPtr;
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
    msg->path = upPath;
    msg->headers.insert_or_assign("Host", _hostPort);
    msg->headers.insert_or_assign("X-Forwarded-For", ctx->getConn()->getPeerAddr().toIpPort());
    TcpClientPtr client = std::make_shared<TcpClient>(ctx->getLoop(), _upAddr);
    client->setConnectCallback([msg](auto upConn) { upConn->write(msg->serialize()); });
    client->setMessageCallback([ctx](const TcpConnectionPtr& upConn, StreamBuffer* buf) {
      ctx->send(buf->data(), buf->size());
      buf->popFront();
    });
    client->setCloseCallback([ctx](auto upConn) { ctx->forceClose(); });
    ctx->setUserData(client);
    ctx->setCloseCallback([ctx] {
      TcpClientPtr upClient = std::any_cast<TcpClientPtr>(ctx->getUserData());
      upClient->setConnectCallback(nullptr);
      upClient->setMessageCallback(nullptr);
      upClient->setWriteCompleteCallback(nullptr);
      upClient->setCloseCallback([upClient](auto upConn) {
        // Hack: Hold a reference of itself until closeCallback() to avoid nullptr
        upClient->setCloseCallback(nullptr);
      });
      upClient->forceClose();
    });
    client->start();
  }

private:
  std::string _host;
  uint16_t _port;
  std::string _hostPort;
};

#endif
