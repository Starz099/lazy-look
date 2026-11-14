#include <cstdio>

#include "lazyfinder.hh"

int main()
{
  std::printf("lazyfinderd %.*s\n",
              static_cast<int>(lazyfinder::version.size()),
              lazyfinder::version.data());
  return 0;
}
