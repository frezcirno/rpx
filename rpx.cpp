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

int main(int argc, char const* argv[])
{
  EventLoop loop;
  HttpServer server(&loop, InetAddress(8080), true, 1);
  HttpRouter router;
  router.addRoute("\\/etc", new StaticHandler("/etc", "/etc", true));
  router.addRoute("\\/", new StaticHandler("/", "."));
  server.setRequestCallback(
    std::bind(&HttpRouter::handleRequest, &router, std::placeholders::_1, std::placeholders::_2));
  server.start();
  loop.loop();
  return 0;
}
