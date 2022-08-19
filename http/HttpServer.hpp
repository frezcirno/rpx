#ifndef __HTTPSERVER_HPP__
#define __HTTPSERVER_HPP__

#include "TcpConnection.hpp"
#include "TcpServer.hpp"
#include "HttpContext.hpp"

typedef std::function<void(HttpContext&)> HttpRequestCallback;

class HttpServer
{
public:
  HttpServer(EventLoop* loop, const InetAddress& listenAddr, bool reusePort, int threadNum)
    : _server(loop, listenAddr, reusePort, threadNum)
  {
    _server.setConnectCallback([this](const TcpConnectionPtr& conn) { initConnection(conn); });
    _server.setCloseCallback([this](const TcpConnectionPtr& conn) { deleteConnection(conn); });
    _server.setMessageCallback(
      [this](const TcpConnectionPtr& conn, StreamBuffer* buffer) { handleMessage(conn, buffer); });
    _server.setWriteCompleteCallback(
      [this](const TcpConnectionPtr& conn) { writeCompleteCallback(conn); });
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

  void setRequestCallback(HttpRequestCallback cb)
  {
    _requestCallback = cb;
  }

private:
  TcpServer _server;
  HttpRequestCallback _requestCallback;

  void initConnection(const TcpConnectionPtr& conn)
  {
    HttpContext* ctx = new HttpContext(conn);
    ctx->parser.setRequestCallback([this, ctx](const HttpParser&) { _requestCallback(*ctx); });
    conn->setUserData(ctx);
  }

  void deleteConnection(const TcpConnectionPtr& conn)
  {
    HttpContext* ctx = std::any_cast<HttpContext*>(conn->getUserData());
    delete ctx;
  }

  void handleMessage(const TcpConnectionPtr& conn, StreamBuffer* buffer)
  {
    HttpContext* ctx = std::any_cast<HttpContext*>(conn->getUserData());
    ctx->parser.advance(buffer->data(), buffer->size());
    buffer->popFront();
  }

  void writeCompleteCallback(const TcpConnectionPtr& conn) {}
};


#endif
