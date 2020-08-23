ARK_BENCHMARK_FLAGS="-Wall -Wextra -O3 -fno-exceptions -fno-rtti -march=native -std=c++2a -Wno-narrowing -Werror -Wno-unused-variable -pedantic -I../../include -I../ -Wno-implicit-fallthrough -pthread"

g++ simple.cpp $ARK_BENCHMARK_FLAGS -o simple
