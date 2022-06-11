cmake_minimum_required(VERSION 3.0)

project(sparrow)

# add_compile_options(-g -lm -Wall -W -Wstrict-prototypes -Wmissing-prototypes -Wsystem-headers)

add_compile_options(-g)

set(CMAKE_C_STANDARD 99)

include_directories(include vm compiler parser cli gc object)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

add_executable(spr cli/cli.c vm/vm.c vm/core.c parser/parser.c include/unicodeUtf8.c include/utils.c
               object/obj_string.c object/header_obj.c)

add_definitions(-DDEBUG)  # 宏定义 DEBUG