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
    printf("init fail");
    return -1;
  }

  EventLoop loop;
  HttpServer server(&loop, InetAddress(8080), true, threadNum);
  HttpRouter router(&server);
  router.addSimpleRoute("/ping",
                        [](int, HttpContext<HttpRequest>::HttpContextPtr ctx, HttpServer*) {
                          ctx->startResponse(200);
                          ctx->sendHeader("Content-Type", "text/plain");
                          ctx->sendHeader("Content-Length", "4");
                          ctx->sendHeader("Connection", "close");
                          ctx->endHeaders();
                          ctx->send("pong", 4);
                          ctx->shutdown();
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
