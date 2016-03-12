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
#include "args.h"
#include <string.h>
#include <stdlib.h>
#define DEFAULT_PORT 14137

void free_generate_args(struct generate_args* a)
{
    a->key_size=0;
    if(a->key_path!=NULL)
    {
        free(a->key_path);
        a->key_path=NULL;
    }
}
void free_chat_args(struct chat_args* a)
{
    if(a->local_key_path!=NULL)
    {
        free(a->local_key_path);
        a->local_key_path=NULL;
    }
    if(a->remote_key_path!=NULL)
    {
        free(a->remote_key_path);
        a->remote_key_path=NULL;
    }
    free_address(&a->addr);
}

static char* copy_string(const char* str)
{
    char* res=(char*)malloc(strlen(str)+1);
    strcpy(res, str);
    return res;
}
static unsigned parse_generate_args(
    int argc,
    char** argv,
    struct generate_args* a
){
    if(argc!=2)
    {
        return 1;
    }
    char* endptr=NULL;
    a->key_size=strtoll(argv[0], &endptr, 0);
    if(endptr!=argv[0]+strlen(argv[0]))
    {
        return 1;
    }
    a->key_path=copy_string(argv[1]);
    return 0;
}
static unsigned parse_chat_args(
    int argc,
    char** argv,
    struct chat_args* a
){
    if(argc<2||argc>3)
    {
        return 1;
    }
    if(argc==3)
    {
        //It is possible that only a port was given.
        char* endptr=NULL;
        a->addr.port=strtol(argv[2], &endptr, 0);
        if(*endptr!='\0')
        {
            //Not a port, is it an address?
            if(parse_address(&a->addr, argv[2]))
            {
                return 1;
            }
            if(a->addr.port==0)
            {
                a->addr.port=DEFAULT_PORT;
            }
            a->wait_for_remote=0;
        }
        else
        {
            a->wait_for_remote=1;
            a->addr.node=NULL;
        }
    }
    else
    {
        a->wait_for_remote=1;
        a->addr.node=NULL;
        a->addr.port=DEFAULT_PORT;
    }
    a->local_key_path=copy_string(argv[0]);
    a->remote_key_path=copy_string(argv[1]);
    return 0;
}
unsigned parse_args(struct args* a, int argc, char** argv)
{
    if(argc>=2&&strcmp(argv[1], "--generate")==0)
    {
        a->mode=MODE_GENERATE;
        if(parse_generate_args(argc-2, argv+2, &a->mode_args.generate))
        {
            return 1;
        }
    }
    else
    {
        a->mode=MODE_CHAT;
        if(parse_chat_args(argc-1, argv+1, &a->mode_args.chat))
        {
            return 1;
        }
    }
    return 0;
}
void free_args(struct args* a)
{
    switch(a->mode)
    {
    case MODE_CHAT:
        free_chat_args(&a->mode_args.chat);
        break;
    case MODE_GENERATE:
        free_generate_args(&a->mode_args.generate);
        break;
    default:
        break;
    }
    a->mode=MODE_INVALID;
}
