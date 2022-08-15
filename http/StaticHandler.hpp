#ifndef __STATICHANDLER_HPP__
#define __STATICHANDLER_HPP__

#include <fstream>
#include <string>
#include "HttpServer.hpp"
#include "HttpRouter.hpp"

class StaticHandler : public HttpHandler
{
public:
  StaticHandler(const std::string& location, const std::string& rootPath, bool alias = false)
    : _location(location)
    , _rootPath(rootPath)
    , _alias(alias)
  {}
  ~StaticHandler() {}

  void handleRequest(const HttpParser& request, TcpConnectionPtr& conn)
  {
    std::string filePath;
    if (_alias) {
      std::string path = request.getPath();
      if (path.find(_location) == 0) {
        path = path.substr(_location.size());
      }
      if (path.empty() || path[0] != '/') {
        path = "/" + path;
      }
      filePath = _rootPath + path;
    } else {
      filePath = _rootPath + request.getPath();
    }
    std::cout << "filePath: " << filePath << std::endl;
    if (!std::ifstream(filePath).good()) {
      conn->write("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n");
      return;
    }
    std::ifstream ifs(filePath, std::ios::binary);
    if (!ifs) {
      conn->write("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n");
      return;
    }
    std::string content(std::istreambuf_iterator<char>(ifs), {});
    conn->write("HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(content.size()) +
                "\r\n\r\n" + content);
    ifs.close();
  }

private:
  std::string _location;
  std::string _rootPath;
  bool _alias;
};

#endif
