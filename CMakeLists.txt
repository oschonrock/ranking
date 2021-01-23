cmake_minimum_required(VERSION 3.16)

set(CMAKE_C_COMPILER clang)

project(projects)
set(CMAKE_C_STANDARD 11) # C11 standard


# defaults from cmake
# -DCMAKE_BUILD_TYPE=Debug              adds           -g
# -DCMAKE_BUILD_TYPE=MinSizeRel         adds       -Os    -DNDEBUG
# -DCMAKE_BUILD_TYPE=RelWithDebInfo     adds       -O2 -g -DNDEBUG
# -DCMAKE_BUILD_TYPE=Release            adds       -O3    -DNDEBUG

# santiser
set(CMAKE_C_FLAGS_DEBUG            -fsanitize=address,undefined)
# set(CMAKE_C_FLAGS_RELWITHDEBINFO   -fsanitize=address,undefined) // for use with valgrind instead

add_compile_options(-Wall -Wextra -Wpedantic -Werror)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin) 

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/modules")

find_package(LibXml2 REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PCRE2 REQUIRED)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

include_directories(include)

add_executable(ranking  apps/ranking.c src/sailor.c src/regatta.c src/curl.c src/easylib.c)
target_link_libraries (ranking
  CURL::libcurl
  OpenSSL::SSL OpenSSL::Crypto
  Threads::Threads
  LibXml2::LibXml2 
  PCRE2::PCRE2)

