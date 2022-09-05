#ifndef __HTTPCONTEXT_HPP__
#define __HTTPCONTEXT_HPP__

#include <unordered_map>
#include "TcpConnection.hpp"
#include "HttpDefinition.hpp"
#include "HttpParser.hpp"

template<typename T>
class HttpContext;

/**
 * class HttpContext: An wrapper over HttpConnection
 *
 * which provides a simple interface to send and parse http request/response.
 */
template<typename T>
class HttpContext
{
  friend class HttpServer;
  friend class HttpClient;

public:
  typedef class HttpParser<T> HttpParser;
  typedef typename HttpParser::ParseCallback ParseCallback;
  typedef std::shared_ptr<HttpContext> HttpContextPtr;
  typedef typename std::function<void(HttpContextPtr)> HttpCallback;
  typedef typename std::function<void()> HttpCtxCallback;

private:
  HttpContext(TcpConnectionPtr conn)
    : _conn(conn)
  {}

public:
  template<typename... U>
  static HttpContextPtr create(U&&... args)
  {
    struct MakeSharedEnabler : public HttpContext
    {
      MakeSharedEnabler(U&&... args)
        : HttpContext(std::forward<U>(args)...)
      {}
    };

    return std::make_shared<MakeSharedEnabler>(std::forward<U>(args)...);
  }

  ~HttpContext() {}

  const TcpConnectionPtr& getConn() const
  {
    return _conn;
  }
  EventLoop* getLoop() const
  {
    return _conn->getLoop();
  }

  std::shared_ptr<T> getMessage() const
  {
    return parser.getMessage();
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

  void forceClose()
  {
    _conn->forceClose();
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

  std::any& getUserData()
  {
    return _userData;
  }
  void setUserData(const std::any& userData)
  {
    _userData = userData;
  }

  /**
   * setWriteCompleteCallback() - set write complete callback
   *
   * context-level callback, only used in server incoming connection
   */
  void setWriteCompleteCallback(HttpCtxCallback cb)
  {
    _writeCompleteCallback = std::move(cb);
  }

  /**
   * setCloseCallback() - set close callback
   *
   * context-level callback, only used in server incoming connection
   */
  void setCloseCallback(HttpCtxCallback cb)
  {
    _closeCallback = std::move(cb);
  }

private:
  TcpConnectionPtr _conn;
  std::any _userData;
  HttpParser parser;
  HttpCtxCallback _writeCompleteCallback;
  HttpCtxCallback _closeCallback;

  void setHeaderCallback(const ParseCallback& cb)
  {
    parser.setHeaderCallback(cb);
  }
  void setMessageCallback(const ParseCallback& cb)
  {
    parser.setMessageCallback(cb);
  }
  void closeCallback()
  {
    if (_closeCallback)
      _closeCallback();
  }
  void writeCompleteCallback()
  {
    if (_writeCompleteCallback)
      _writeCompleteCallback();
  }
  llhttp_errno_t advance(const char* data, size_t len)
  {
    return parser.advance(data, len);
  }
};

#endif