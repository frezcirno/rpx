#ifndef __HTTPPARSER_HPP__
#define __HTTPPARSER_HPP__

#include <string>
#include <sstream>
#include <unordered_map>
#include <functional>
#include "llhttp.h"
#include "Utils.hpp"
#include "TcpConnection.hpp"

template<typename T>
class HttpParser;

struct HttpRequest
{
  llhttp_method_t method;
  std::string path;
  uint8_t major, minor;
  std::unordered_map<std::string, std::string> headers;
  std::string body;

  std::string serialize() const
  {
    std::stringstream ss;
    ss << llhttp_method_name(method) << " " << path << " HTTP/" << static_cast<int>(major) << "."
       << static_cast<int>(minor) << "\r\n";
    for (auto& kv : headers) {
      ss << kv.first << ": " << kv.second << "\r\n";
    }
    ss << "\r\n";
    ss << body;
    return ss.str();
  }
};

struct HttpResponse
{
  uint8_t major, minor;
  uint16_t status_code;
  std::string status_message;
  std::unordered_map<std::string, std::string> headers;
  std::string body;

  std::string serialize() const
  {
    std::stringstream ss;
    ss << "HTTP/" << static_cast<int>(major) << "." << static_cast<int>(minor) << " " << status_code
       << " " << status_message << "\r\n";
    for (auto& kv : headers) {
      ss << kv.first << ": " << kv.second << "\r\n";
    }
    ss << "\r\n";
    ss << body;
    return ss.str();
  }
};

template<typename T>
class HttpParser : noncopyable
{
public:
  typedef std::function<void(const HttpParser&)> ParseCallback;

  HttpParser();
  ~HttpParser() {}

  llhttp_errno_t advance(const char* data, size_t len)
  {
    return llhttp_execute(&_parser, data, len);
  }
  llhttp_errno_t advance(const std::string& data)
  {
    return advance(data.data(), data.size());
  }
  void resume()
  {
    llhttp_resume(&_parser);
  }
  llhttp_errno_t finish()
  {
    return llhttp_finish(&_parser);
  }

  std::shared_ptr<T> getMessage() const
  {
    return _data;
  }

  void reset()
  {
    std::shared_ptr<T> new_data = std::make_shared<T>();
    _data.swap(new_data);
  }

  // For parsing response
  uint16_t getStatusCode() const;
  const std::string& getStatusMessage() const;

  // For parsing request
  llhttp_method_t getMethod() const;
  const char* getMethodStr() const;
  const std::string& getPath() const;

  int getHttpMajor() const
  {
    return _data->major;
  }

  int getHttpMinor() const
  {
    return _data->minor;
  }

  const std::unordered_map<std::string, std::string>& getHeaders() const
  {
    return _data->headers;
  }

  const std::string& getBody() const
  {
    return _data->body;
  }

  void setHeaderCallback(ParseCallback cb)
  {
    _headerCallback = std::move(cb);
  }

  void setMessageCallback(ParseCallback cb)
  {
    _messageCallback = std::move(cb);
  }

private:
  llhttp_t _parser;
  llhttp_settings_t _settings;
  std::shared_ptr<T> _data;
  std::string _currentBuffer;
  std::string _currentBuffer1;

  ParseCallback _headerCallback;
  ParseCallback _messageCallback;

  static int on_message_begin(llhttp_t* parser)
  {
    HttpParser* that = container_of(parser, &HttpParser::_parser);
    that->reset();
    return 0;
  }

  static int on_url(llhttp_t* parser, const char* at, size_t length)
  {
    HttpParser* that = container_of(parser, &HttpParser::_parser);
    that->_currentBuffer.append(at, length);
    return 0;
  }

  static int on_url_complete(llhttp_t* parser)
  {
    HttpParser* that = container_of(parser, &HttpParser::_parser);
    that->_data->method = static_cast<llhttp_method_t>(llhttp_get_method(parser));
    that->_data->path.swap(that->_currentBuffer);
    return 0;
  }

  static int on_status(llhttp_t* parser, const char* at, size_t length)
  {
    HttpParser* that = container_of(parser, &HttpParser::_parser);
    that->_currentBuffer.append(at, length);
    return 0;
  }

  static int on_status_complete(llhttp_t* parser)
  {
    HttpParser* that = container_of(parser, &HttpParser::_parser);
    that->_data->status_code = llhttp_get_status_code(parser);
    that->_data->status_message.swap(that->_currentBuffer);
    return 0;
  }

  static int on_header_field(llhttp_t* parser, const char* at, size_t length)
  {
    HttpParser* that = container_of(parser, &HttpParser::_parser);
    that->_currentBuffer.append(at, length);
    return 0;
  }

  static int on_header_field_complete(llhttp_t* parser)
  {
    HttpParser* that = container_of(parser, &HttpParser::_parser);
    return 0;
  }

  static int on_header_value(llhttp_t* parser, const char* at, size_t length)
  {
    HttpParser* that = container_of(parser, &HttpParser::_parser);
    that->_currentBuffer1.append(at, length);
    return 0;
  }

  static int on_header_value_complete(llhttp_t* parser)
  {
    HttpParser* that = container_of(parser, &HttpParser::_parser);
    that->_data->headers[that->_currentBuffer].swap(that->_currentBuffer1);
    that->_currentBuffer.clear();
    // Now _currentBuffer and _currentBuffer1 are all clear.
    return 0;
  }

  static int on_headers_complete(llhttp_t* parser)
  {
    HttpParser* that = container_of(parser, &HttpParser::_parser);
    that->_data->major = llhttp_get_http_major(&that->_parser);
    that->_data->minor = llhttp_get_http_minor(&that->_parser);
    if (that->_headerCallback)
      that->_headerCallback(*that);
    return 0;
  }

  static int on_chunk_header(llhttp_t* parser)
  {
    // HttpParser* that = container_of(parser, &HttpParser::_parser);
    return 0;
  }

  static int on_chunk_complete(llhttp_t* parser)
  {
    // HttpParser* that = container_of(parser, &HttpParser::_parser);
    return 0;
  }

  static int on_body(llhttp_t* parser, const char* at, size_t length)
  {
    HttpParser* that = container_of(parser, &HttpParser::_parser);
    that->_currentBuffer.append(at, length);
    return 0;
  }

  static int on_message_complete(llhttp_t* parser)
  {
    HttpParser* that = container_of(parser, &HttpParser::_parser);
    that->_data->body.swap(that->_currentBuffer);
    if (that->_messageCallback)
      that->_messageCallback(*that);
    return 0;
  }
};

template<>
HttpParser<HttpRequest>::HttpParser()
{

  llhttp_settings_init(&_settings);
  _settings.on_message_begin = HttpParser::on_message_begin;
  _settings.on_url = HttpParser::on_url;
  _settings.on_url_complete = HttpParser::on_url_complete;
  _settings.on_header_field = HttpParser::on_header_field;
  _settings.on_header_field_complete = HttpParser::on_header_field_complete;
  _settings.on_header_value = HttpParser::on_header_value;
  _settings.on_header_value_complete = HttpParser::on_header_value_complete;
  _settings.on_headers_complete = HttpParser::on_headers_complete;
  _settings.on_chunk_header = HttpParser::on_chunk_header;
  _settings.on_chunk_complete = HttpParser::on_chunk_complete;
  _settings.on_body = HttpParser::on_body;
  _settings.on_message_complete = HttpParser::on_message_complete;

  llhttp_init(&_parser, HTTP_REQUEST, &_settings);
}

template<>
HttpParser<HttpResponse>::HttpParser()
{

  llhttp_settings_init(&_settings);
  _settings.on_message_begin = HttpParser::on_message_begin;
  _settings.on_status = HttpParser::on_status;
  _settings.on_status_complete = HttpParser::on_status_complete;
  _settings.on_header_field = HttpParser::on_header_field;
  _settings.on_header_field_complete = HttpParser::on_header_field_complete;
  _settings.on_header_value = HttpParser::on_header_value;
  _settings.on_header_value_complete = HttpParser::on_header_value_complete;
  _settings.on_headers_complete = HttpParser::on_headers_complete;
  _settings.on_chunk_header = HttpParser::on_chunk_header;
  _settings.on_chunk_complete = HttpParser::on_chunk_complete;
  _settings.on_body = HttpParser::on_body;
  _settings.on_message_complete = HttpParser::on_message_complete;

  llhttp_init(&_parser, HTTP_RESPONSE, &_settings);
}

#endif
