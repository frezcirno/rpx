#include <atomic>
#include <functional>
#include <iostream>
#include <thread>
#include <vector>

#include "CountDownLatch.hpp"
#include "ThreadPool.hpp"
#include "EventLoop.hpp"
#include "TcpServer.hpp"

void connectionCallback(const TcpConnectionPtr& conn)
{
  std::cout << "on connection" << std::endl;
}

void messageCallback(const TcpConnectionPtr& conn, StreamBuffer* buffer)
{
  std::cout << "on message" << std::endl;
  write(STDOUT_FILENO, buffer->data(), buffer->size());
  buffer->popFront(buffer->size());
  conn->write("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
}

void writeCallback(const TcpConnectionPtr& conn)
{
  std::cout << "write ok" << std::endl;
}

int main(int argc, char const* argv[])
{
  EventLoop loop;
  TcpServer server(&loop, InetAddress(8080), true);
  server.setConnectionCallback(connectionCallback);
  server.setMessageCallback(messageCallback);
  server.setWriteCompleteCallback(writeCallback);
  server.start();
  loop.loop();
  return 0;
}
