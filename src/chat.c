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
#include "chat.h"
#include "key.h"
#include "args.h"
#include "net.h"
#include <stdio.h>
#include <ncurses.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#define TIMEOUT_MS 2000

unsigned passive_connect(
    struct address* addr,
    struct node* remote,
    struct key* local_key,
    struct key* remote_key
){
    //"Server" mode, attempts to handshake all connectors and continues when
    //the first handshake succeeds.
    struct node local;   
    fd_set readfds;
    if(node_listen(&local, addr->port))
    {
        fprintf(stderr, "Unable to listen port %d\n", addr->port);
        return 1;
    }
    FD_ZERO(&readfds);
    FD_SET(local.socket, &readfds);
    while(1)
    {
        fd_set read_temp=readfds;
        if(select(local.socket+1, &read_temp, NULL, NULL, NULL)==-1)
        {
            fprintf(stderr, "select() failed: %s\n", strerror(errno));
            node_close(&local);
            return 1;
        }
        if(!FD_ISSET(local.socket, &read_temp)||
           node_accept(&local, remote))
        {
            continue;
        }
        struct key* accepted_key=NULL;
        if( node_handshake(
                remote,
                local_key,
                remote_key,
                1,
                &accepted_key,
                TIMEOUT_MS
            )
        ){
            struct address remote_addr;
            node_get_address(remote, &remote_addr);
            fprintf(
                stderr,
                "Failed handshake with %s:%d\n",
                remote_addr.node, remote_addr.port
            );
            free_address(&remote_addr);
            node_close(remote);
        }
        else
        {
            //Handshake succeeded!
            break;
        }
    }
    node_close(&local);
    return 0;
}
unsigned active_connect(
    struct address* addr,
    struct node* remote,
    struct key* local_key,
    struct key* remote_key
){
    //"Client" mode, try to connect to the remote
    fd_set writefds;
    if(node_connect(remote, addr))
    {
        fprintf(
            stderr,
            "Connecting to %s:%d\n failed\n",
            addr->node, addr->port
        );
        return 1;
    }
    FD_ZERO(&writefds);
    FD_SET(remote->socket, &writefds);
    if(select(remote->socket+1, NULL, &writefds, NULL, NULL)==-1)
    {
        fprintf(stderr, "select() failed: %s\n", strerror(errno));
        return 1;
    }
    struct key* accepted_key=NULL;
    if( node_handshake(
            remote,
            local_key,
            remote_key,
            1,
            &accepted_key,
            TIMEOUT_MS
        )
    ){
        struct address remote_addr;
        node_get_address(remote, &remote_addr);
        fprintf(
            stderr,
            "Failed handshake with %s:%d\n",
            remote_addr.node, remote_addr.port
        );
        free_address(&remote_addr);
        node_close(remote);
        return 1;
    }
    //Handshake succeeded!
    return 0;
}
void chat(struct chat_args* a)
{
    struct key local_key, remote_key;
    if(open_key(a->local_key_path, &local_key))
    {
        fprintf(stderr, "Unable to open \"%s\"\n", a->local_key_path);
        return;
    }
    if(open_key(a->remote_key_path, &remote_key))
    {
        fprintf(stderr, "Unable to open \"%s\"\n", a->remote_key_path);
        close_key(&local_key);
        return;
    }
    struct node remote;
    if(a->wait_for_remote)
    {
        printf("Listening for connection on port %d\n", a->addr.port);
        if(passive_connect(&a->addr, &remote, &local_key, &remote_key))
        {
            goto end;
        }
    }
    else
    {
        printf("Connecting to %s:%d\n", a->addr.node, a->addr.port);
        if(active_connect(&a->addr, &remote, &local_key, &remote_key))
        {
            goto end;
        }
    }
    printf("Connection succeeded!\n");
    node_close(&remote);
end:
    close_key(&local_key);
    close_key(&remote_key);
    return;
}
