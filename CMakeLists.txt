cmake_minimum_required(VERSION 3.10)

project(ark_bench DESCRIPTION "benchmarks for the ark entity component system framework"
                  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB ARK_BENCHMARKS ${CMAKE_SOURCE_DIR}/benchmarks/ark/*.cpp)

foreach(bench_file ${ARK_BENCHMARKS})
    # I used a simple string replace, to cut off .cpp.
    get_filename_component(bench_name ${bench_file} NAME_WLE)
    add_executable( ${bench_name} ${bench_file} )
    target_include_directories(${bench_name} PUBLIC ${CMAKE_SOURCE_DIR}/benchmarks/)
    target_include_directories(${bench_name} PUBLIC ${CMAKE_SOURCE_DIR}/include/)
    if (MSVC)
        target_compile_options(${bench_name} PUBLIC /W4)
    else()
        target_compile_options(${bench_name} PUBLIC -Wall -Wextra -pedantic -Werror)
    endif()
endforeach(bench_file ${ARK_BENCHMARKS})

# turn on most warnings and treat all warnings as errors