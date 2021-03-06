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
#include <stdio.h>
#include "key.h"
#include "args.h"
#include "chat.h"

void print_usage(const char* name)
{
    fprintf(
        stderr,
        "Usage: %s <local-key> <remote-key> [<address>[:<port>]]\n"
        "       %s --generate <size> <new-key-file>\n",
        name, name
    );
}
void generate(struct generate_args* a)
{
    struct key new_key;
    key_create(&new_key, a->key_path, a->key_size);
    key_close(&new_key);
}
int main(int argc, char** argv)
{
    struct args args;
    if(parse_args(&args, argc, argv))
    {
        print_usage(argv[0]);
        return 1;
    }
    switch(args.mode)
    {
    case MODE_CHAT:
        chat(&args.mode_args.chat);
        break;
    case MODE_GENERATE:
        generate(&args.mode_args.generate);
        break;
    default:
        break;
    }
    free_args(&args);
    return 0;
}
