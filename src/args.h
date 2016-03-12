/*
The MIT License (MIT)

Copyright (c) 2016 Julius Ikkala

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#ifndef OTPCHAT_ARGS_H_
#define OTPCHAT_ARGS_H_
    #include <stddef.h>
    #include <stdint.h>
    #include "address.h"

    struct generate_args
    {
        size_t key_size;
        char* key_path;
    };
    void free_generate_args(struct generate_args* a);
    struct chat_args
    {
        char* local_key_path;
        char* remote_key_path;

        unsigned wait_for_remote;
        struct address addr;
    };
    void free_chat_args(struct chat_args* a);
    struct args
    {
        enum
        {
            MODE_INVALID=0,
            MODE_CHAT,
            MODE_GENERATE
        } mode;
        union
        {
            struct chat_args chat;
            struct generate_args generate;
        } mode_args;
    };
    unsigned parse_args(struct args* a, int argc, char** argv);
    void free_args(struct args* a);
#endif
