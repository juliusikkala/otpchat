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
        uint8_t id[16];
        uint64_t head;
    };
    unsigned open_key(const char* path, struct key* k);
    unsigned create_key(const char* path, struct key* k, size_t sz);
    void close_key(struct key* k);

    struct block
    {
        uint8_t* data;
        uint64_t sz;
    };
    void create_block_from_str(const char* str, struct block* b);
    void free_block(struct block* b);
    unsigned get_key_block(
        struct key* k,
        struct block* key_block,
        uint64_t bytes
    );
    unsigned encrypt(
        const struct block* key,
        const struct block* input,
        struct block* output
    );
    unsigned decrypt(
        const struct block* key,
        const struct block* input,
        struct block* output
    );
#endif
