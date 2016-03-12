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

static char** str_to_argv(const char* str, int* argc)
{
    char** argv=NULL;
    *argc=0;
    while(str!=NULL&&*str!=0)
    {
        const char* end=strchr(str, ' ');
        size_t len=0;
        if(end!=NULL)
        {
            len=end-str;
        }
        else
        {
            len=strlen(str);
        }
        char* arg=(char*)malloc(len+1);
        memcpy(arg, str, len);
        arg[len]=0;

        (*argc)++;
        argv=(char**)realloc(argv, sizeof(char*)*(*argc));
        argv[*argc-1]=arg;
        str=end;
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
static unsigned command_quit(struct chat_state* state, int argc, char** argv)
{
    (void)argv;
    if(argc!=0)
    {
        return 1;
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
    //{"connect", command_connect},
    //{"disconnect", command_disconnect},
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
    for(size_t i=0;i<sizeof(command_table)/sizeof(command_table);++i)
    {
        if(strcmp(argv[0], command_table[i].command)==0)
        {
            ret=command_table[i].fn(state, argc-1, argv+1);
            found=1;
            break;
        }
    }
    if(!found)
    {
        chat_push_status(state, "Unrecognized command %s", argv[0]);
        ret=1;
    }
    free_argv(argc, argv);
    return ret;
}
