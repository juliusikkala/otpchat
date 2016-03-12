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
#include "block.h"
#include <stdlib.h>
#include <string.h>

void block_create_from_str(struct block* b, const char* str)
{
    b->size=strlen(str);
    b->data=(uint8_t*)malloc(b->size);
    memcpy(b->data, str, b->size);
}
void block_create(struct block* b, size_t size)
{
    b->size=size;
    b->data=(uint8_t*)calloc(b->size, 1);
}
void block_clone(struct block* dst, const struct block* src)
{
    dst->size=src->size;
    dst->data=(uint8_t*)malloc(dst->size);
    memcpy(dst->data, src->data, dst->size);
}
void free_block(struct block* b)
{
    if(b->data!=NULL)
    {
        free(b->data);
        b->data=NULL;
        b->size=0;
    }
}
