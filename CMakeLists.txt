#The MIT License (MIT)
#
#Copyright (c) 2016 Julius Ikkala
#
#Permission is hereby granted, free of charge, to any person obtaining a copy
#of this software and associated documentation files (the "Software"), to deal
#in the Software without restriction, including without limitation the rights
#to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#copies of the Software, and to permit persons to whom the Software is
#furnished to do so, subject to the following conditions:
#
#The above copyright notice and this permission notice shall be included in all
#copies or substantial portions of the Software.
#
#THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
#SOFTWARE.
cmake_minimum_required(VERSION 2.8)
project(OTPCHAT C)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/build")
set(CMAKE_MODULE_PATH "${PROJECT_BINARY_DIR}/cmake/" ${CMAKE_MODULE_PATH})

set(INSTALL_BIN_DIR bin CACHE PATH "Where to install binaries")

#Fix problems with libraries installed in /usr/lib/x86_64-linux-gnu on Ubuntu
set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} /usr/lib/x86_64-linux-gnu)

option(DEBUG "Compile in debug mode" ON)

if(DEBUG)
    set(CMAKE_C_FLAGS "-Wall -Wextra -Wpedantic -O3 -g3 -std=c99 -fprofile-arcs -ftest-coverage")
else(DEBUG)
    set(CMAKE_C_FLAGS "-Wall -Wextra -Wpedantic -O3 -std=c99 -DNDEBUG")
endif(DEBUG)

if(MINGW)
    set(CMAKE_C_FLAGS "-mwindows ${CMAKE_CXX_FLAGS}")
endif(MINGW)

set(CURSES_NEED_NCURSES TRUE)
set(CURSES_WANT_NCURSESW TRUE)
find_package(NCursesw REQUIRED)

include_directories(
    src
    ${CURSES_INCLUDE_DIR}
)

file(GLOB SRC_C 
    src/address.c
    src/args.c
    src/block.c
    src/chat.c
    src/command.c
    src/key.c
    src/main.c
    src/message.c
    src/node.c
    src/ui.c
    src/user.c
)

add_executable(otpchat ${SRC_C})
target_link_libraries(otpchat ${CURSES_LIBRARIES})

install(
    TARGETS otpchat
    RUNTIME DESTINATION ${INSTALL_BIN_DIR}
)
