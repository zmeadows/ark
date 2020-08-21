ARK_BENCHMARK_FLAGS="-Wall -Wextra -O3 -fno-exceptions -fno-rtti -march=native -std=c++2a -Wno-narrowing -Werror -Wno-unused-variable -pedantic -I../../include -I../ -Wno-implicit-fallthrough -pthread"

if [ $# -eq 0 ]
  then
    g++ simple_pos_vel.cpp $ARK_BENCHMARK_FLAGS -o simple_pos_vel 2>&1
    g++ simple_parallel_systems.cpp $ARK_BENCHMARK_FLAGS -o simple_parallel_systems 2>&1
    g++ create_destroy.cpp $ARK_BENCHMARK_FLAGS -o create_destroy 2>&1
    exit 0
fi

g++ $1.cpp $ARK_BENCHMARK_FLAGS -o $1 2>&1
