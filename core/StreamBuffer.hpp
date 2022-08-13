#ifndef __STREAMBUFFER_HPP__
#define __STREAMBUFFER_HPP__

#include <assert.h>
#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>
#include <vector>
#include <iostream>
#include <string>
#include <string_view>

class StreamBuffer
{
public:
  StreamBuffer(size_t n, size_t headRoom = 0)
    : _data(new char[n])
    , _head(_data + headRoom)
    , _tail(_data + headRoom)
    , _end(_data + n)
    , _headRoom(headRoom)
  {}
  ~StreamBuffer()
  {
    delete[] _data;
  }
  size_t capacity() const
  {
    return _end - _data;
  }
  size_t headCapacity() const
  {
    return _head - _data;
  }
  size_t endCapacity() const
  {
    return _end - _tail;
  }
  const char* data() const
  {
    return _head;
  }
  size_t size() const
  {
    return _tail - _head;
  }
  bool empty() const
  {
    return _head == _tail;
  }

  int upResize(size_t n)
  {
    // n *= 2;   // reserve more space
    char* new_data = new char[n];
    char* new_head = new_data + headCapacity();
    _tail = std::copy(_head, _tail, new_head);
    delete[] _data;
    _head = new_head;
    _data = new_data;
    _end = _data + n;
    return 0;
  }

  ssize_t readFd(int fd)
  {
    char extraBuf[65536];
    struct iovec vec[2];
    int room = _end - _tail;
    vec[0].iov_base = _tail;
    vec[0].iov_len = room;
    vec[1].iov_base = extraBuf;
    vec[1].iov_len = 65536;
    ssize_t n = ::readv(fd, vec, 2);
    if (n < 0)
      return n;
    int extraLen = n - room;
    if (extraLen > 0) {
      _tail += room;
      upResize(capacity() + extraLen);
      std::copy(extraBuf, extraBuf + extraLen, _tail);
      _tail += extraLen;
    } else {
      _tail += n;
    }
    return n;
  }

  void append(const char* data, size_t len)
  {
    int endRoom = endCapacity();
    int headRoom = headCapacity();
    if (headRoom > _headRoom) {
      if (len > headRoom - _headRoom + endRoom)
        upResize(_headRoom + size() + len);
      else
        alignLeft();
    } else if (len > endRoom) {
      upResize(_headRoom + size() + len);
    }
    _tail = std::copy(data, data + len, _tail);
  }

  void popFront(size_t n)
  {
    int len = size();
    assert(n <= len);
    if (n == len) {
      _head = _tail = _data + _headRoom;
    } else {
      _head += n;
    }
  }

  void popFront()
  {
    _head = _tail = _data + _headRoom;
  }

private:
  char* _data;
  char* _head;
  char* _tail;
  char* _end;
  int _headRoom;

  void alignLeft()
  {
    int len = size();
    char* new_head = _data + _headRoom;
    _tail = std::copy(_head, _tail, new_head);
    _head = new_head;
  }
};

#endif
