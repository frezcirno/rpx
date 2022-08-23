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

  void handleRequest(HttpContext& ctx)
  {
    std::string path = ctx.parser.getPath();
    if (path.find("..") != std::string::npos) {
      ctx.sendError(HttpStatus::FORBIDDEN);
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
    std::shared_ptr<std::ifstream> ifs(new std::ifstream(filePath, std::ios::binary));
    if (!ifs->good()) {
      ctx.sendError(HttpStatus::NOT_FOUND);
      return;
    }
    size_t fileSize = ifs->seekg(0, std::ios::end).tellg();
    ifs->seekg(0, std::ios::beg);
    ctx.startResponse(HttpStatus::OK);
    ctx.sendHeader("Content-Type", "text/html");
    ctx.sendHeader("Content-Length", std::to_string(fileSize));
    ctx.endHeaders();
    // char buf[1024];
    // while (ifs->good()) {
    //   ifs->read(buf, sizeof(buf));
    //   int rv = ctx.send(buf, ifs->gcount());
    //   if (rv < 0)
    //     break;
    // }
    // ctx.shutdown();
    ctx.setWriteCompleteCallback([ifs](HttpContext& ctx) { sendMore(ctx, ifs); });
  }

  static void sendMore(HttpContext& ctx, std::shared_ptr<std::ifstream> ifs)
  {
    char buf[4096];
    if (ifs->good()) {
      ifs->read(buf, sizeof buf);
      size_t readCount = ifs->gcount();
      if (readCount > 0) {
        int rv = ctx.send(buf, readCount);
        if (rv < 0)
          goto err;
      }
      return;
    }
  err:
    ctx.setWriteCompleteCallback(NULL);
    ctx.shutdown();
  }

private:
  std::string _location;
  std::string _rootPath;
  bool _alias;
};

#endif
