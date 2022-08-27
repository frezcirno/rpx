#ifndef __HTTPCONTEXT_HPP__
#define __HTTPCONTEXT_HPP__

#include <unordered_map>
#include "TcpConnection.hpp"
#include "HttpDefinition.hpp"
#include "HttpParser.hpp"

class HttpContext;
typedef std::function<void(HttpContext&)> HttpCallback;

class HttpContext
{
  friend class HttpServer;
  friend class HttpClient;

private:
  HttpContext(TcpConnectionPtr conn, llhttp_type_t mode)
    : _conn(conn)
    , parser(mode)
  {
    _conn->setWriteCompleteCallback([&](const TcpConnectionPtr& conn) {
      HttpContext* ctx = std::any_cast<HttpContext*>(conn->getUserData());
      if (_writeCompleteCallback)
        _writeCompleteCallback(*ctx);
    });
  }
  ~HttpContext() {}

public:
  TcpConnectionPtr& getConn()
  {
    return _conn;
  }

  void setMessageCallback(const HttpParser::MessageCallback& cb)
  {
    parser.setMessageCallback(cb);
  }

  void startRequest(llhttp_method_t method, const std::string& url)
  {
    _conn->write(llhttp_method_name(method));
    _conn->write(" ");
    _conn->write(url);
    _conn->write(" HTTP/1.1\r\n");
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

  int endHeaders()
  {
    return _conn->write("\r\n", 2);
  }

  void setWriteCompleteCallback(HttpCallback cb)
  {
    _writeCompleteCallback = std::move(cb);
  }

  int send(const std::string& contents)
  {
    return _conn->write(contents);
  }

  int send(const char* contents, size_t len)
  {
    return _conn->write(contents, len);
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
  HttpCallback _writeCompleteCallback;

public:
  HttpParser parser;
};

#endif