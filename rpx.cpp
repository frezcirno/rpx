#include <atomic>
#include <functional>
#include <iostream>
#include <thread>
#include <vector>

#include "CountDownLatch.hpp"
#include "ThreadPool.hpp"
#include "EventLoop.hpp"
#include "HttpServer.hpp"

void handleRequest(const HttpParser& request, TcpConnectionPtr& conn)
{
  std::cout << "[" << conn->getPeerAddr().toIpPort() << "] " << request.getMethodStr() << " "
            << request.getUrl() << std::endl;
  for (auto&& [k, v] : request.getHeaders())
    std::cout << "< " << k << ": " << v << std::endl;
  std::cout << request.getBody() << std::endl;
  conn->write("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
}

int main(int argc, char const* argv[])
{
  EventLoop loop;
  HttpServer server(&loop, InetAddress(8080), true, 1);
  server.setRequestCallback(handleRequest);
  server.start();
  loop.loop();
  return 0;
}
