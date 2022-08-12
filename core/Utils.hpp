#ifndef __UTILS_HPP__
#define __UTILS_HPP__

class noncopyable
{
public:
  noncopyable(const noncopyable&) = delete;
  noncopyable& operator=(const noncopyable&) = delete;

protected:
  noncopyable() = default;
  ~noncopyable() = default;
};

#endif