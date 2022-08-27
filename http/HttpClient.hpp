#ifndef __HTTPCLIENT_HPP__
#define __HTTPCLIENT_HPP__

#include "TcpClient.hpp"
#include "HttpParser.hpp"
#include "HttpContext.hpp"

class HttpClient
{
public:
  HttpClient(EventLoop* loop, const InetAddress& connAddr)
    : _client(loop, connAddr)
    , parser(HTTP_RESPONSE)
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

  void setConnectCallback(HttpCallback cb)
  {
    _connectCallback = std::move(cb);
  }

  void setResponseCallback(HttpCallback cb)
  {
    _responseCallback = std::move(cb);
  }

private:
  TcpClient _client;
  HttpCallback _responseCallback;
  HttpCallback _connectCallback;

  void initConnection(const TcpConnectionPtr& conn)
  {
    HttpContext* ctx = new HttpContext(conn, HTTP_RESPONSE);
    ctx->parser.setMessageCallback([this, ctx](const HttpParser&) { _responseCallback(*ctx); });
    conn->setUserData(ctx);
    if (_connectCallback)
      _connectCallback(*ctx);
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

public:
  HttpParser parser;
};

#endif
