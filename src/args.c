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
    if(a->key_path!=NULL)
    {
        free(a->key_path);
        a->key_path=NULL;
    }
    if(a->key_directory!=NULL)
    {
        free(a->key_directory);
        a->key_directory=NULL;
    }
}

//Returns the index of the string in argv or negative for error
static int find_str_in_argv(int argc, char** argv, const char* str)
{
    int i=0;
    for(i=0;i<argc;++i)
    {
        if(strcmp(argv[i], str)==0)
        {
            return i;
        }
    }
    return -1;
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
    a->key_path=(char*)malloc(strlen(argv[1])+1);
    strcpy(a->key_path, argv[1]);
    return 0;
}
static unsigned parse_chat_args(
    int argc,
    char** argv,
    struct chat_args* a
){
    if(argc<2)
    {
        return 1;
    }
    a->key_path=NULL;
    a->key_directory=NULL;
    return 0;
}
unsigned parse_args(int argc, char** argv, struct args* a)
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
