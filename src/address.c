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
#include "address.h"
#include <stdlib.h>
#include <string.h>

unsigned parse_address(struct address* addr, const char* text)
{
    //Find last ':'
    const char* separator=strrchr(text, ':');
    if(separator==NULL)
    {
        //No separator, so port is unspecified.
        size_t len=strlen(text);
        addr->node=(char*)malloc(len+1);
        memcpy(addr->node, text, len);
        addr->port=0;
        return 0;
    }
    //Read node name
    size_t len=separator-text;
    if(len==0)
    {
        return 1;
    }
    addr->node=(char*)malloc(len+1);
    memcpy(addr->node, text, len);
    addr->node[separator-text]=0;
    //Read port number
    char* port_end=NULL;
    long int port=strtol(separator+1, &port_end, 0);
    if(port<=0||port>=(1<<16)||*port_end!=0)
    {
        free(addr->node);
        return 1;
    }
    addr->port=port;
    return 0;
}
void free_address(struct address* addr)
{
    if(addr->node!=NULL)
    {
        free(addr->node);
        addr->node=NULL;
    }
    addr->port=0;
}
