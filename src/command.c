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
#include "command.h"
#include "chat.h"
#include <string.h>
#include <stdlib.h>
#define ARG_SEPARATORS " \t"

static char** str_to_argv(const char* str, int* argc)
{
    char** argv=NULL;
    *argc=0;
    while(str!=NULL)
    {
        const char* end=strpbrk(str, ARG_SEPARATORS);
        size_t len=0;
        if(end!=NULL)
        {
            len=end-str;
        }
        else
        {
            len=strlen(str);
        }
        if(len!=0)
        {
            char* arg=(char*)malloc(len+1);
            memcpy(arg, str, len);
            arg[len]=0;

            (*argc)++;
            argv=(char**)realloc(argv, sizeof(char*)*(*argc));
            argv[*argc-1]=arg;
        }
        if(end==NULL)
        {
            break;
        }
        str=end+1;
    }
    return argv;
}
static void free_argv(int argc, char** argv)
{
    if(argv!=NULL)
    {
        for(int i=0;i<argc;++i)
        {
            free(argv[i]);
        }
        free(argv);
    }
}
static unsigned command_connect(struct chat_state* state, int argc, char** argv)
{
    if(argc!=1)
    {
        return 2;
    }
    struct address addr;
    if(parse_address(&addr, argv[0]))
    {
        return 2;
    }
    if(chat_begin_connect(state, &addr))
    {
        return 1;
    }
    free_address(&addr);
    return 0;
}
static unsigned command_disconnect(
    struct chat_state* state,
    int argc, char** argv
){
    (void)argv;
    if(argc!=0)
    {
        return 2;
    }
    chat_disconnect(state, ID_REMOTE);
    return 0;
}
static unsigned command_listen(
    struct chat_state* state,
    int argc, char** argv
){
    unsigned port=DEFAULT_PORT;
    if(argc==1)
    {
        char* port_end=NULL;
        port=strtol(argv[0], &port_end, 0);   
        if(port<=0||port>=(1<<16)||*port_end!=0)
        {
            return 2;
        }
    }
    else if(argc!=0)
    {
        return 2;
    }
    if(chat_begin_listen(state, port))
    {
        return 1;
    }
    return 0;
}
static unsigned command_endlisten(
    struct chat_state* state,
    int argc, char** argv
){
    (void)argv;
    if(argc!=0)
    {
        return 2;
    }
    chat_end_listen(state);
    return 0;
}

static unsigned command_quit(struct chat_state* state, int argc, char** argv)
{
    (void)argv;
    if(argc!=0)
    {
        return 2;
    }
    state->running=0;
    return 0;
}
typedef unsigned (*command_function)(
    struct chat_state* state, int argc, char** argv
);
struct command_table_entry
{
    const char* command;
    command_function fn;
};
static const struct command_table_entry command_table[]={
    {"connect", command_connect},
    {"disconnect", command_disconnect},
    {"listen", command_listen},
    {"endlisten", command_endlisten},
    {"quit", command_quit}
};
unsigned command_handle(struct chat_state* state, const char* command_str)
{
    int argc=0;
    char** argv=str_to_argv(command_str, &argc);
    unsigned ret=0;
    unsigned found=0;
    if(argc<=0)
    {
        return 0;
    }
    for(size_t i=0;
        i<sizeof(command_table)/sizeof(struct command_table_entry);
        ++i
    ){
        if(strcmp(argv[0], command_table[i].command)==0)
        {
            ret=command_table[i].fn(state, argc-1, argv+1);
            found=1;
            break;
        }
    }
    if(ret==2)
    {
        chat_push_status(state, "Malformed command \"%s\"", command_str);
    }
    else if(!found)
    {
        chat_push_status(state, "Unrecognized command \"%s\"", argv[0]);
        ret=1;
    }
    free_argv(argc, argv);
    return ret;
}
