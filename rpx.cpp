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
  router.addSimpleRoute("/static", new StaticHandler("."));
  router.addSimpleRoute("/big", new StaticHandler("./static/big_file", true));
  router.addSimpleRoute("/a", new StaticHandler("./static/a", true));
  router.addSimpleRoute("/b", new StaticHandler("./static/b", true));
  router.addSimpleRoute("/baidu", new ProxyHandler("www.baidu.com", 80));
  router.addSimpleRoute("/self", new ProxyHandler("127.0.0.1", 8080));
  router.addSimpleRoute("/other", new ProxyHandler("127.0.0.1", 8081));
  server.setRequestCallback([&router](auto& ctx) { router.handleRequest(ctx); });
  server.start();

  loop.loop();
  zlog_fini();
  return 0;
}
