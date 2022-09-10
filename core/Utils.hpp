#ifndef __UTILS_HPP__
#define __UTILS_HPP__

#include <stddef.h>
#include <stdint.h>

template<class T, class M>
static inline constexpr ptrdiff_t offset_of(const M T::*member)
{
  return reinterpret_cast<ptrdiff_t>(&(reinterpret_cast<T*>(0)->*member));
}

template<class T, class M>
static inline constexpr T* container_of(M* ptr, const M T::*member)
{
  return reinterpret_cast<T*>(reinterpret_cast<intptr_t>(ptr) - offset_of(member));
}

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