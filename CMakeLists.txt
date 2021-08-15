cmake_minimum_required(VERSION 3.20)
project(gpt2_codec C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer")
add_subdirectory(vendor/cJSON)
add_subdirectory(vendor/utf8proc)

set(BUILD_SHARED_AND_STATIC_LIBS on)

LIST(APPEND VENDOR_INCLUDES
        vendor
        vendor/libc_hsearch_r/include
        vendor/uthash/include
        vendor/utfproc)
include_directories(${VENDOR_INCLUDES})

add_library(gpt2_codec
        library.c
        rdtsc.c
        rdtsc.h
        vendor/libc_hsearch_r/search_hsearch_r.c
        vendor/utf8proc/utf8proc.c)
add_dependencies(gpt2_codec cjson)
target_link_libraries(gpt2_codec cjson)
add_executable(gpt2_codec_test main.c)
add_dependencies(gpt2_codec_test gpt2_codec)
target_link_libraries(gpt2_codec_test gpt2_codec)
