#ifndef __HTTPPARSER_HPP__
#define __HTTPPARSER_HPP__

#include <string>
#include <sstream>
#include <unordered_map>
#include <functional>
#include "Utils.hpp"
#include "TcpConnection.hpp"
#include "llhttp.h"

class HttpParser;

class HttpRequest
{
public:
  HttpRequest(const std::string& url)
    : _url(url)
  {}

  ~HttpRequest() {}

  void setMethod(llhttp_method_t method)
  {
    _method = method;
  }

  void setPath(const std::string& url)
  {
    _url = url;
  }

  void setVersion(const std::string& version)
  {
    _version = version;
  }

  void setHeader(const std::string& key, const std::string& value)
  {
    _headers[key] = value;
  }

  void setHeaders(const std::unordered_map<std::string, std::string>& headers)
  {
    _headers = headers;
  }

  void setBody(const std::string& body)
  {
    _body = body;
  }

  std::string serialize() const
  {
    std::stringstream ss;
    ss << _method << " " << _url << " " << _version << "\r\n";
    for (auto& kv : _headers) {
      ss << kv.first << ": " << kv.second << "\r\n";
    }
    ss << "\r\n";
    ss << _body;
    return ss.str();
  }

private:
  llhttp_method_t _method;
  std::string _url;
  std::string _version;
  std::unordered_map<std::string, std::string> _headers;
  std::string _body;
};

class HttpParser : noncopyable
{
public:
  typedef std::function<void(const HttpParser&)> MessageCallback;

  HttpParser(llhttp_type mode)
    : _headerComplete(0)
    , _messageSeq(-1)
    , _mode(mode)
  {
    llhttp_settings_init(&_settings);
    _settings.on_message_begin = HttpParser::on_message_begin;
    _settings.on_url = HttpParser::on_url;
    _settings.on_header_field = HttpParser::on_header_field;
    _settings.on_header_value = HttpParser::on_header_value;
    _settings.on_headers_complete = HttpParser::on_headers_complete;
    _settings.on_chunk_header = HttpParser::on_chunk_header;
    _settings.on_chunk_complete = HttpParser::on_chunk_complete;
    _settings.on_body = HttpParser::on_body;
    _settings.on_message_complete = HttpParser::on_message_complete;

    llhttp_init(&_parser, mode, &_settings);
  }
  ~HttpParser() {}

  llhttp_errno_t advance(const char* data, size_t len)
  {
    return llhttp_execute(&_parser, data, len);
  }
  llhttp_errno_t advance(const std::string& data)
  {
    return advance(data.data(), data.size());
  }
  void next()
  {
    llhttp_resume(&_parser);
  }
  llhttp_errno_t finish()
  {
    return llhttp_finish(&_parser);
  }
  void reset()
  {
    _headerComplete = 0;
    _path.clear();
    _headers.clear();
    _body.clear();
    _currentHeader.clear();
  }

  int getRequestSeqno() const
  {
    return _messageSeq;
  }
  llhttp_method_t getMethod() const
  {
    return static_cast<llhttp_method_t>(_parser.method);
  }
  const char* getMethodStr() const
  {
    return llhttp_method_name(getMethod());
  }
  const std::string& getPath() const
  {
    assert(_path.size());
    return _path;
  }
  int getHttpMajor() const
  {
    return llhttp_get_http_major(const_cast<llhttp_t*>(&_parser));
  }

  int getHttpMinor() const
  {
    return llhttp_get_http_minor(const_cast<llhttp_t*>(&_parser));
  }
  int headerComplete() const
  {
    return _headerComplete;
  }
  const std::unordered_map<std::string, std::string>& getHeaders() const
  {
    return _headers;
  }
  const std::string& getBody() const
  {
    return _body;
  }

  void setMessageCallback(MessageCallback cb)
  {
    _messageCallback = std::move(cb);
  }

private:
  llhttp_t _parser;
  llhttp_settings_t _settings;
  llhttp_type _mode;
  std::unordered_map<std::string, std::string> _headers;
  std::string _path;
  std::string _currentHeader;
  std::string _body;
  int _headerComplete;
  int _messageSeq;

  MessageCallback _messageCallback;

  static int on_message_begin(llhttp_t* parser)
  {
    HttpParser* httpParser = container_of(parser, &HttpParser::_parser);
    httpParser->reset();
    httpParser->_messageSeq++;
    return 0;
  }
  static int on_url(llhttp_t* parser, const char* at, size_t length)
  {
    HttpParser* httpParser = container_of(parser, &HttpParser::_parser);
    httpParser->_path.assign(at, length);
    return 0;
  }
  static int on_header_field(llhttp_t* parser, const char* at, size_t length)
  {
    HttpParser* httpParser = container_of(parser, &HttpParser::_parser);
    httpParser->_currentHeader.assign(at, length);
    if (httpParser->_headers.count(httpParser->_currentHeader))
      return -1;
    return 0;
  }
  static int on_header_value(llhttp_t* parser, const char* at, size_t length)
  {
    HttpParser* httpParser = container_of(parser, &HttpParser::_parser);
    httpParser->_headers[httpParser->_currentHeader].assign(at, length);
    return 0;
  }
  static int on_headers_complete(llhttp_t* parser)
  {
    HttpParser* httpParser = container_of(parser, &HttpParser::_parser);
    httpParser->_headerComplete = 1;
    return 0;
  }
  static int on_chunk_header(llhttp_t* parser)
  {
    // HttpParser* httpParser = container_of(parser, &HttpParser::_parser);
    return 0;
  }
  static int on_chunk_complete(llhttp_t* parser)
  {
    // HttpParser* httpParser = container_of(parser, &HttpParser::_parser);
    return 0;
  }
  static int on_body(llhttp_t* parser, const char* at, size_t length)
  {
    HttpParser* httpParser = container_of(parser, &HttpParser::_parser);
    httpParser->_body.assign(at, length);
    return 0;
  }
  static int on_message_complete(llhttp_t* parser)
  {
    HttpParser* httpParser = container_of(parser, &HttpParser::_parser);
    httpParser->_messageCallback(*httpParser);
    return 0;
  }
};

#endif
