#ifndef __HTTPCLIENT_HPP__
#define __HTTPCLIENT_HPP__

#include "TcpClient.hpp"
#include "HttpContext.hpp"

class HttpClient
{
  typedef class HttpContext<HttpResponse> HttpContext;
  typedef typename HttpContext::HttpParser HttpParser;
  typedef typename HttpContext::HttpCallback HttpCallback;
  typedef std::shared_ptr<HttpContext> HttpContextPtr;

public:
  HttpClient(EventLoop* loop, const InetAddress& connAddr)
    : _client(loop, connAddr)
  {
    _client.setConnectCallback([&](const TcpConnectionPtr& conn) { initConnection(conn); });
    _client.setCloseCallback([&](const TcpConnectionPtr& conn) { deleteConnection(conn); });
    _client.setMessageCallback(
      [&](const TcpConnectionPtr& conn, StreamBuffer* buffer) { handleMessage(conn, buffer); });
    _client.setWriteCompleteCallback(
      [&](const TcpConnectionPtr& conn) { writeCompleteCallback(conn); });
  }
  ~HttpClient() {}

  EventLoop* getLoop() const
  {
    return _client.getLoop();
  }
  void start()
  {
    _client.start();
  }
  void stop()
  {
    _client.stop();
  }
  void shutdown()
  {
    _client.shutdown();
  }
  void forceClose()
  {
    _client.forceClose();
  }

  void setConnectCallback(HttpCallback cb)
  {
    _connectCallback = std::move(cb);
  }

  void setResponseCallback(HttpCallback cb)
  {
    _responseCallback = std::move(cb);
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
  TcpClient _client;
  HttpCallback _connectCallback;
  HttpCallback _writeCompleteCallback;
  HttpCallback _responseCallback;
  HttpCallback _closeCallback;

  void initConnection(const TcpConnectionPtr& conn)
  {
    HttpContextPtr ctx = HttpContext::create(conn);
    conn->setUserData(ctx);
    ctx->setMessageCallback([&, ctx](const HttpParser&) {
      if (_responseCallback)
        _responseCallback(ctx);
    });
    if (_connectCallback)
      _connectCallback(ctx);
  }

  void deleteConnection(const TcpConnectionPtr& conn)
  {
    HttpContextPtr ctx = std::any_cast<HttpContextPtr>(conn->getUserData());
    ctx->setHeaderCallback(nullptr);
    ctx->setMessageCallback(nullptr);
    ctx->setWriteCompleteCallback(nullptr);
    ctx->setCloseCallback(nullptr);
    if (_closeCallback)
      _closeCallback(ctx);
    conn->setUserData(nullptr);
  }

  void handleMessage(const TcpConnectionPtr& conn, StreamBuffer* buffer)
  {
    HttpContextPtr ctx = std::any_cast<HttpContextPtr>(conn->getUserData());
    ctx->advance(buffer->data(), buffer->size());
    buffer->popFront();
  }

  void writeCompleteCallback(const TcpConnectionPtr& conn)
  {
    HttpContextPtr ctx = std::any_cast<HttpContextPtr>(conn->getUserData());
    if (_writeCompleteCallback)
      _writeCompleteCallback(ctx);
  }
};

#endif
