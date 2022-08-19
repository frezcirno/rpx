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

  void handleRequest(const HttpParser& request, HttpContext& conn)
  {
    std::string path = request.getPath();
    if (path.find("..") != std::string::npos) {
      conn.sendError(HttpStatus::FORBIDDEN);
      return;
    }
    std::string filePath;
    if (_alias) {
      if (path.find(_location) == 0)
        path = path.substr(_location.size());
      if (path.empty() || path[0] != '/')
        path = "/" + path;
      filePath = _rootPath + path;
    } else {
      filePath = _rootPath + path;
    }
    if (filePath.back() == '/')
      filePath += "index.html";
    std::ifstream ifs(filePath, std::ios::binary);
    if (!ifs) {
      conn.sendError(HttpStatus::NOT_FOUND);
      return;
    }
    std::string content(std::istreambuf_iterator<char>(ifs), {});
    conn.startResponse(HttpStatus::OK);
    conn.sendHeader("Content-Type", "text/html");
    conn.sendHeader("Content-Length", std::to_string(content.size()));
    conn.endHeaders();
    conn.sendContent(content);
    conn.shutdown();
    ifs.close();
  }

private:
  std::string _location;
  std::string _rootPath;
  bool _alias;
};

#endif
