#include <atomic>
#include <functional>
#include <iostream>
#include <thread>
#include <vector>
#include <zlog.h>

#include "ThreadPool.hpp"
#include "EventLoop.hpp"
#include "HttpServer.hpp"
#include "HttpClient.hpp"
#include "HttpRouter.hpp"
#include "StaticHandler.hpp"
#include "ProxyHandler.hpp"

int main(int argc, char const* argv[])
{
  int threadNum = 1;
  if (argc >= 2)
    threadNum = atoi(argv[1]);

  int rc = dzlog_init("rpx.conf", "default");
  if (rc) {
    dzlog_fatal("init fail");
    return -1;
  }

  InetAddress listenAddr;
  if (!listenAddr.parseHost("127.0.0.1", 8080)) {
    dzlog_fatal("parseHost fail");
    return -2;
  }
  EventLoop loop;
  HttpServer server(&loop, listenAddr, true, threadNum);
  HttpRouter router(&server);
  router.addSimpleRoute(
    "/ping", [](int, HttpContext<HttpRequest>::HttpContextPtr ctx, HttpServer*) {
      ctx->send("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 4\r\nConnection: "
                "close\r\n\r\npong");
    });
  router.addSimpleRoute("/big", StaticHandler("./static/big_file", true));
  router.addSimpleRoute("/a", StaticHandler("./static/a", true));
  router.addSimpleRoute("/b", StaticHandler("./static/b", true));
  router.addSimpleRoute("/baidu", ProxyHandler("www.baidu.com", 80));
  router.addSimpleRoute("/self", ProxyHandler("127.0.0.1", 8080));
  router.addSimpleRoute("/other", ProxyHandler("127.0.0.1", 8081));
  server.setRequestCallback([&router](auto ctx) { router.handleRequest(ctx); });
  server.start();

  loop.loop();
  zlog_fini();
  return 0;
}
