cmake_minimum_required(VERSION 3.10)
project(malloc_lab_driver C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Optimize for performance but keep symbols if needed
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/vendor/malloclab-handout)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(code
  vendor/malloclab-handout/mdriver.c
  vendor/malloclab-handout/memlib.c
  vendor/malloclab-handout/fsecs.c
  vendor/malloclab-handout/fcyc.c
  vendor/malloclab-handout/ftimer.c
  vendor/malloclab-handout/clock.c
  src/mm.c
)

target_compile_definitions(code PRIVATE NDEBUG)

