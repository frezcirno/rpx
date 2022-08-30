#ifndef __HTTPSERVER_HPP__
#define __HTTPSERVER_HPP__

#include <zlog.h>
#include "TcpConnection.hpp"
#include "TcpServer.hpp"
#include "HttpContext.hpp"

class HttpServer
{
  typedef class HttpContext<HttpRequest> HttpContext;
  typedef typename HttpContext::HttpParser HttpParser;
  typedef typename HttpContext::HttpCallback HttpCallback;

public:
  HttpServer(EventLoop* loop, const InetAddress& listenAddr, bool reusePort, int threadNum,
             ThreadInitCallback cb = ThreadInitCallback())
    : _server(loop, listenAddr, reusePort, threadNum, cb)
    , _zc(zlog_get_category("HttpServer"))
  {
    _server.setConnectCallback([&](const TcpConnectionPtr& conn) { initConnection(conn); });
    _server.setCloseCallback([&](const TcpConnectionPtr& conn) { deleteConnection(conn); });
    _server.setMessageCallback(
      [&](const TcpConnectionPtr& conn, StreamBuffer* buffer) { handleMessage(conn, buffer); });
    _server.setWriteCompleteCallback(
      [&](const TcpConnectionPtr& conn) { writeCompleteCallback(conn); });
  }
  ~HttpServer() {}

  EventLoop* getBaseLoop() const
  {
    return _server.getBaseLoop();
  }
  void start()
  {
    _server.start();
  }

  void setConnectCallback(HttpCallback cb)
  {
    _connectCallback = std::move(cb);
  }

  void setRequestCallback(HttpCallback cb)
  {
    _requestCallback = std::move(cb);
  }

  void setWriteCompleteCallback(HttpCallback cb)
  {
    _writeCompleteCallback = std::move(cb);
  }

  void setCloseCallback(HttpCallback cb)
  {
    _closeCallback = std::move(cb);
  }

private:
  TcpServer _server;
  HttpCallback _connectCallback;
  HttpCallback _writeCompleteCallback;
  HttpCallback _requestCallback;
  HttpCallback _closeCallback;

  zlog_category_t* _zc;

  void initConnection(const TcpConnectionPtr& conn)
  {
    HttpContext* ctx = new HttpContext(conn);
    ctx->setMessageCallback([this, ctx](const HttpParser& parser) { _requestCallback(*ctx); });
    conn->setUserData(ctx);
    if (_connectCallback)
      _connectCallback(*ctx);
  }

  void deleteConnection(const TcpConnectionPtr& conn)
  {
    HttpContext* ctx = std::any_cast<HttpContext*>(conn->getUserData());
    if (_closeCallback)
      _closeCallback(*ctx);
    delete ctx;
  }

  void handleMessage(const TcpConnectionPtr& conn, StreamBuffer* buffer)
  {
    HttpContext* ctx = std::any_cast<HttpContext*>(conn->getUserData());
    ctx->parser.advance(buffer->data(), buffer->size());
    buffer->popFront();
  }

  void writeCompleteCallback(const TcpConnectionPtr& conn)
  {
    HttpContext* ctx = new HttpContext(conn);
    if (_writeCompleteCallback)
      _writeCompleteCallback(*ctx);
  }
};


#endif
