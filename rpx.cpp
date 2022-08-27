#include <atomic>
#include <functional>
#include <iostream>
#include <thread>
#include <vector>

#include "CountDownLatch.hpp"
#include "ThreadPool.hpp"
#include "EventLoop.hpp"
#include "HttpServer.hpp"
#include "HttpClient.hpp"
#include "HttpRouter.hpp"
#include "StaticHandler.hpp"

void handle(HttpContext& ctx)
{
  ctx.sendError(NOT_IMPLEMENTED);
}

int main(int argc, char const* argv[])
{
  int threadNum = 1;
  if (argc >= 2)
    threadNum = atoi(argv[1]);

  EventLoop loop;
  HttpServer server(&loop, InetAddress(8080), true, threadNum);
  HttpRouter router;
  router.addRoute("\\/big", new StaticHandler("/big", "./static/big_file", true));
  router.addRoute("\\/a", new StaticHandler("/a", "./static/a", true));
  router.addRoute("\\/b", new StaticHandler("/b", "./static/b", true));
  router.addRoute("\\/c", new StaticHandler("/c", "./static/c", true));
  server.setRequestCallback([&router](HttpContext& ctx) { router.handleRequest(ctx); });
  server.start();

  int i = 3;
  void* timer;
  timer = loop.runEvery(1.0, [&] {
    if (i) {
      std::cout << i-- << std::endl;
    } else {
      loop.cancel(timer);
      std::cout << "Timer!" << std::endl;
    }
  });

  InetAddress addr("110.242.68.66", 80);
  HttpClient client(&loop, addr);
  client.setConnectCallback([](HttpContext& ctx) {
    ctx.startRequest(HTTP_GET, "/");
    ctx.sendHeader("Connection", "keep-alive");
    ctx.sendHeader("User-Agent", "cpp-httplib/0.1");
    ctx.sendHeader("Accept", "*/*");
    ctx.sendHeader("Accept-Encoding", "gzip, deflate");
    ctx.sendHeader("Accept-Language", "en-US,en;q=0.8,zh-CN;q=0.6,zh;q=0.4");
    ctx.sendHeader("Host", "www.baidu.com");
    ctx.endHeaders();
    ctx.shutdown();
  });
  client.setResponseCallback(
    [](HttpContext& ctx) { std::cout << ctx.parser.getBody() << std::endl; });
  client.start();

  loop.loop();
  return 0;
}
