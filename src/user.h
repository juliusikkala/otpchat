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
#ifndef OTPCHAT_USER_H_
#define OTPCHAT_USER_H_
    #include "key.h"
    #include "net.h"
    enum connection_state
    {
        NOT_CONNECTED=0,
        CONNECTING,
        CONNECTED
    };
    struct user
    {
        struct key* key;
        struct node node;
        char* name;
        enum connection_state state;
        uint32_t id;
    };
    void user_init(struct user* u, uint32_t id);
    void user_set_name(struct user* u, const char* name);
    unsigned user_begin_connect(struct user* u, struct address* addr);
    unsigned user_finish_connect(
        struct user* u,
        struct key_store* keys
    );
    unsigned user_accept(
        struct node* listen_node,
        struct user* u,
        struct key_store* keys
    );
    void user_disconnect(struct user* u);
    void user_close(struct user* u);
#endif
