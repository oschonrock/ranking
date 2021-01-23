cmake_minimum_required(VERSION 3.16)

if (NOT WIN32)
  set(CMAKE_C_COMPILER clang)
endif()

project(ranking)
set(CMAKE_C_STANDARD 11) # C11 standard


# defaults from cmake
# -DCMAKE_BUILD_TYPE=Debug              adds           -g
# -DCMAKE_BUILD_TYPE=MinSizeRel         adds       -Os    -DNDEBUG
# -DCMAKE_BUILD_TYPE=RelWithDebInfo     adds       -O2 -g -DNDEBUG
# -DCMAKE_BUILD_TYPE=Release            adds       -O3    -DNDEBUG

# santizer
if (NOT "${CMAKE_SYSTEM_NAME}" STREQUAL "FreeBSD") # FreeBSD hangs on curl_init with sanitizer
  set(CMAKE_C_FLAGS_DEBUG            -fsanitize=address,undefined)
# set(CMAKE_C_FLAGS_RELWITHDEBINFO   -fsanitize=address,undefined) // for use with valgrind instead
endif()  

# be very pedantic
add_compile_options(-Wall -Wextra -Wpedantic -Werror)

# support for clangd - there is a symlink in src root to the one in build
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# put executable binaries in their own folder
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin) 

# cmake does not come with a Module FindPCRE2.cmake. Include our own. 
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/modules")

find_package(LibXml2 REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenSSL 1.1.0 REQUIRED)
find_package(PCRE2 REQUIRED)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

include_directories(include)

add_executable(ranking
  apps/ranking.c
  src/sailor.c
  src/regatta.c
  src/curl.c
  src/easylib.c)

target_link_libraries (ranking
  CURL::libcurl
  OpenSSL::SSL OpenSSL::Crypto
  Threads::Threads
  LibXml2::LibXml2 
  PCRE2::PCRE2)
