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
#ifndef OTPCHAT_KEY_H_
#define OTPCHAT_KEY_H_
    #include <stdint.h>
    #include <stdio.h>
    //Treat this struct as read-only when accessing directly
    struct key
    {
        FILE* stream;
        size_t size;
        uint8_t id[16];
        uint64_t head;
    };
    unsigned key_open(struct key* k, const char* path);
    unsigned key_create(struct key* k, const char* path, size_t sz);
    void key_close(struct key* k);
    void key_seek(struct key* k, uint64_t new_head);

    struct key_store
    {
        struct key local;
        struct key* remotes;
        size_t remotes_size;
    };
    void key_store_init(struct key_store* store);
    void key_store_close(struct key_store* store);
    unsigned key_store_open_local(
        struct key_store* store,
        const char* local_path
    );
    unsigned key_store_open_remote(
        struct key_store* store,
        const char* remote_path
    );
    struct key* key_store_find(struct key_store* store, uint8_t* id);

    struct block;
    unsigned encrypt(
        struct key* k,
        struct block* message
    );
    unsigned decrypt(
        struct key* k,
        struct block* message
    );
#endif
