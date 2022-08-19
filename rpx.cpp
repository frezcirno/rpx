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
  EventLoop loop;
  HttpServer server(&loop, InetAddress(8080), true, 24);
  HttpRouter router;
  router.addRoute("\\/etc", new StaticHandler("/etc", "/etc", true));
  router.addRoute("\\/", new StaticHandler("/", "."));
  server.setRequestCallback([&router](HttpContext& ctx) { router.handleRequest(ctx); });
  server.start();
  loop.loop();
  return 0;
}
