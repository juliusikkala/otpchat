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
#include "message.h"

void message_create(struct message* msg, uint32_t id)
{
    msg->id=id;
    msg->timestamp=time(NULL);
    msg->text.size=0;
    msg->text.data=NULL;
}
void message_create_from_str(
    struct message* msg,
    uint32_t id,
    const char* str
){
    msg->id=id;
    msg->timestamp=time(NULL);
    block_create_from_str(&msg->text, str);
}
void message_create_from_block(
    struct message* msg,
    uint32_t id,
    const struct block* text
){
    msg->id=id;
    msg->timestamp=time(NULL);
    block_clone(&msg->text, text);
}
void free_message(struct message* msg)
{
    free_block(&msg->text);
    msg->timestamp=0;
    msg->id=0;
}
