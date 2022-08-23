#include <atomic>
#include <functional>
#include <iostream>
#include <thread>
#include <vector>

#include "CountDownLatch.hpp"
#include "ThreadPool.hpp"
#include "EventLoop.hpp"
#include "HttpServer.hpp"
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
  server.start();
  loop.loop();
  return 0;
}
