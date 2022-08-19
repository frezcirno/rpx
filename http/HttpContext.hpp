#ifndef __HTTPCONTEXT_HPP__
#define __HTTPCONTEXT_HPP__

#include <unordered_map>
#include "TcpConnection.hpp"
#include "HttpDefinition.hpp"
#include "HttpParser.hpp"

class HttpServer;

class HttpContext;
typedef std::function<void(HttpContext&)> HttpWriteCompleteCallback;

class HttpContext
{
  friend HttpServer;

private:
  HttpContext(TcpConnectionPtr conn)
    : _conn(conn)
  {
    _conn->setWriteCompleteCallback([this](const TcpConnectionPtr& conn) {
      HttpContext* ctx = std::any_cast<HttpContext*>(conn->getUserData());
      if (writeCompleteCallback)
        writeCompleteCallback(*ctx);
    });
  }
  ~HttpContext() {}

public:
  TcpConnectionPtr& getConn()
  {
    return _conn;
  }

  void startResponse(int code, const std::string& message)
  {
    _conn->write("HTTP/1.1 ");
    _conn->write(std::to_string(code));
    _conn->write(" ");
    _conn->write(message);
    _conn->write("\r\n");
  }

  void startResponse(int code)
  {
    startResponse(code, HttpDefinition::getMessage(code));
  }

  void sendHeader(const std::string& key, const std::string& value)
  {
    _conn->write(key);
    _conn->write(": ", 2);
    _conn->write(value);
    _conn->write("\r\n", 2);
  }

  void endHeaders()
  {
    _conn->write("\r\n", 2);
  }

  void setWriteCompleteCallback(const HttpWriteCompleteCallback& cb)
  {
    writeCompleteCallback = cb;
  }

  void send(const std::string& contents)
  {
    _conn->write(contents);
  }

  void send(const char* contents, size_t len)
  {
    _conn->write(contents, len);
  }

  void shutdown()
  {
    _conn->shutdown();
  }

  void sendError(int code, const std::string& message)
  {
    std::string body;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    body += "<p>" + message + "</p>";
    body += "<hr><em>TinyWebServer</em></body></html>";

    startResponse(code, message);
    sendHeader("Content-length", std::to_string(body.size()));
    endHeaders();
    send(body);
    shutdown();
  }

  void sendError(int code)
  {
    sendError(code, HttpDefinition::getMessage(code));
  }

private:
  TcpConnectionPtr _conn;
  HttpWriteCompleteCallback writeCompleteCallback;

public:
  HttpParser parser;
};

#endif