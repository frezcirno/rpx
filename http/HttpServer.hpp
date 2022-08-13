#ifndef __HTTPSERVER_HPP__
#define __HTTPSERVER_HPP__

#include "TcpConnection.hpp"
#include "TcpServer.hpp"
#include "HttpParser.hpp"

class HttpServer
{
  typedef std::function<void(const HttpParser&, TcpConnectionPtr&)> HttpRequestCallback;

public:
  HttpServer(EventLoop* loop, const InetAddress& listenAddr, bool reusePort, int threadNum)
    : _server(loop, listenAddr, reusePort, threadNum)
  {
    _server.setConnectCallback(std::bind(&HttpServer::initConnection, this, std::placeholders::_1));
    _server.setCloseCallback(std::bind(&HttpServer::deleteConnection, this, std::placeholders::_1));
    _server.setMessageCallback(
      std::bind(&HttpServer::handleMessage, this, std::placeholders::_1, std::placeholders::_2));
    _server.setWriteCompleteCallback(
      std::bind(&HttpServer::writeCompleteCallback, this, std::placeholders::_1));
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
    parser->setRequestCallback(std::bind(_requestCallback, std::placeholders::_1, conn));
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
