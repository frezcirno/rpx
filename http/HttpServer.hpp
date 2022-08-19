#ifndef __HTTPSERVER_HPP__
#define __HTTPSERVER_HPP__

#include "TcpConnection.hpp"
#include "TcpServer.hpp"
#include "HttpParser.hpp"
#include "HttpContext.hpp"

typedef std::function<void(const HttpParser&, HttpContext&)> HttpRequestCallback;

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
    HttpParser* parser = new HttpParser();
    parser->setRequestCallback([this, conn](const HttpParser& parser) {
      auto ctx = HttpContext(conn);
      _requestCallback(parser, ctx);
    });
    conn->setUserData(parser);
  }

  void deleteConnection(const TcpConnectionPtr& conn)
  {
    HttpParser* parser = std::any_cast<HttpParser*>(conn->getUserData());
    delete parser;
  }

  void handleMessage(const TcpConnectionPtr& conn, StreamBuffer* buffer)
  {
    HttpParser* parser = std::any_cast<HttpParser*>(conn->getUserData());
    parser->advance(buffer->data(), buffer->size());
    buffer->popFront();
  }

  void writeCompleteCallback(const TcpConnectionPtr& conn) {}
};


#endif
