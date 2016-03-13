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
#ifndef OTPCHAT_CHAT_H_
#define OTPCHAT_CHAT_H_
    #include "args.h"
    #include "key.h"
    #include "user.h"
    #include "message.h"
    #include "block.h"
    #include "address.h"
    #include <stdlib.h>

    struct chat_state
    {
        struct key_store keys;
        struct user local, remote;

        struct message* history;
        size_t history_size;
        size_t history_line;
        int history_width, history_height;//width and height of the history box

        struct block input;
        size_t cursor_index;

        struct block receiving;
        size_t received_size;

        struct block sending;
        size_t sent_size;

        unsigned running;
    };

    const char* chat_id_name(struct chat_state* state, uint32_t id);
    void chat_push_message(
        struct chat_state* state,
        const struct message* msg
    );
    void chat_push_status(
        struct chat_state* state,
        const char* format,
        ...
    );
    unsigned chat_begin_connect(struct chat_state* state, struct address* addr);
    unsigned chat_begin_listen(struct chat_state* state, uint16_t port);
    void chat_end_listen(struct chat_state* state);
    void chat_disconnect(struct chat_state* state, uint32_t id);

    unsigned chat_begin_send(struct chat_state* state, struct block* b);
    void chat(struct chat_args* a);
#endif
