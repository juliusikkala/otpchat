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
#define _DEFAULT_SOURCE
#include "chat.h"
#include "key.h"
#include "args.h"
#include "net.h"
#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>
#include <errno.h>
#include <endian.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#define TIMEOUT_MS 2000
#define MESSAGE_HEADER_SIZE 12
#define MESSAGE_SIZE_OFFSET 0
#define MESSAGE_HEAD_OFFSET 4

struct chat_state
{
    struct key local_key, remote_key;
    struct node remote;

    struct block receiving;
    size_t received_size;

    struct block sending;
    size_t sent_size;
};

static void print_user_prompt(struct key* k, const char* name)
{
    printf("%s (%lu/%lu): ", name, k->head, k->size);
    fflush(stdout);
}

static unsigned passive_connect(
    struct address* addr,
    struct chat_state* state
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
           node_accept(&local, &state->remote))
        {
            continue;
        }
        struct key* accepted_key=NULL;
        if( node_handshake(
                &state->remote,
                &state->local_key,
                &state->remote_key,
                1,
                &accepted_key,
                TIMEOUT_MS
            )
        ){
            struct address remote_addr;
            node_get_address(&state->remote, &remote_addr);
            fprintf(
                stderr,
                "Failed handshake with %s:%d\n",
                remote_addr.node, remote_addr.port
            );
            free_address(&remote_addr);
            node_close(&state->remote);
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
static unsigned active_connect(
    struct address* addr,
    struct chat_state* state
){
    //"Client" mode, try to connect to the remote
    fd_set writefds;
    if(node_connect(&state->remote, addr))
    {
        fprintf(
            stderr,
            "Connecting to %s:%d\n failed\n",
            addr->node, addr->port
        );
        return 1;
    }
    FD_ZERO(&writefds);
    FD_SET(state->remote.socket, &writefds);
    if(select(state->remote.socket+1, NULL, &writefds, NULL, NULL)==-1)
    {
        fprintf(stderr, "select() failed: %s\n", strerror(errno));
        return 1;
    }
    struct key* accepted_key=NULL;
    if( node_handshake(
            &state->remote,
            &state->local_key,
            &state->remote_key,
            1,
            &accepted_key,
            TIMEOUT_MS
        )
    ){
        struct address remote_addr;
        node_get_address(&state->remote, &remote_addr);
        fprintf(
            stderr,
            "Failed handshake with %s:%d\n",
            remote_addr.node, remote_addr.port
        );
        free_address(&remote_addr);
        node_close(&state->remote);
        return 1;
    }
    //Handshake succeeded!
    return 0;
}
static unsigned chat_init(struct chat_args* a, struct chat_state* state)
{
    if(open_key(a->local_key_path, &state->local_key))
    {
        fprintf(stderr, "Unable to open \"%s\"\n", a->local_key_path);
        return 1;
    }
    if(open_key(a->remote_key_path, &state->remote_key))
    {
        fprintf(stderr, "Unable to open \"%s\"\n", a->remote_key_path);
        close_key(&state->local_key);
        return 1;
    }
    if(a->wait_for_remote)
    {
        printf("Listening for connection on port %d\n", a->addr.port);
        if(passive_connect(&a->addr, state))
        {
            goto end;
        }
    }
    else
    {
        printf("Connecting to %s:%d\n", a->addr.node, a->addr.port);
        if(active_connect(&a->addr, state))
        {
            goto end;
        }
    }
    state->receiving.data=NULL;
    state->receiving.size=0;
    state->received_size=0;
    state->sending.data=NULL;
    state->sending.size=0;
    state->sent_size=0;
    return 0;
end:
    close_key(&state->local_key);
    close_key(&state->remote_key);
    return 1;
}
static void chat_end(struct chat_state* state)
{
    node_close(&state->remote);
    close_key(&state->local_key);
    close_key(&state->remote_key);
    free_block(&state->receiving);
    free_block(&state->sending);
}
static unsigned chat_begin_send(struct chat_state* state, struct block* message)
{
    free_block(&state->sending);
    state->sent_size=0;

    state->sending.size=message->size+MESSAGE_HEADER_SIZE;
    state->sending.data=(uint8_t*)malloc(state->sending.size);

    uint32_t size=htobe32((uint32_t)state->sending.size-MESSAGE_HEADER_SIZE);
    uint64_t head=htobe64(state->local_key.head);
    memcpy(state->sending.data+MESSAGE_SIZE_OFFSET, &size, sizeof(size));
    memcpy(state->sending.data+MESSAGE_HEAD_OFFSET, &head, sizeof(head));
    memcpy(
        state->sending.data+MESSAGE_HEADER_SIZE,
        message->data,
        message->size
    );
    struct block content;
    content.data=state->sending.data+MESSAGE_HEADER_SIZE;
    content.size=state->sending.size-MESSAGE_HEADER_SIZE;
    if(encrypt(&state->local_key, &content))
    {
        fprintf(stderr, "Out of local key data!\n");
        return 1;
    }
    return 0;
}
static unsigned chat_handle_input(struct chat_state* state)
{
    struct block message;
    size_t alloc=0;
    getline((char**)&message.data, &alloc, stdin);
    message.size=strlen((char*)message.data);
    if(message.size==0)
    {
        return 0;
    }
    message.size--;//Don't count newline
    if(chat_begin_send(state, &message))
    {
        free_block(&message);
        return 1;
    }
    free_block(&message);
    print_user_prompt(&state->local_key, "Local");
    return 0;
}
static unsigned chat_handle_message(struct chat_state* state)
{
    print_user_prompt(&state->remote_key, "\nRemote");
    printf(
        "%.*s\n",
        (int)state->receiving.size-MESSAGE_HEADER_SIZE,
        state->receiving.data+MESSAGE_HEADER_SIZE
    );
    free_block(&state->receiving);
    state->received_size=0;
    return 0;
}
static unsigned chat_handle_recv(struct chat_state* state)
{
    if(state->receiving.size==0)
    {
        create_block(MESSAGE_HEADER_SIZE, &state->receiving);
        state->received_size=0;
    }
    size_t received=node_recv(
        &state->remote,
        state->receiving.data+state->received_size,
        state->receiving.size-state->received_size
    );
    state->received_size+=received;
    if(received==0)
    {
        return 1;
    }
    if(state->received_size==state->receiving.size)
    {
        if(state->receiving.size==MESSAGE_HEADER_SIZE)
        {
            uint32_t size=0;
            uint64_t head=0;
            memcpy(
                &size,
                state->receiving.data+MESSAGE_SIZE_OFFSET,
                sizeof(size)
            );
            memcpy(
                &head,
                state->receiving.data+MESSAGE_HEAD_OFFSET,
                sizeof(head)
            );
            size=be32toh(size);
            head=be64toh(head);
            seek_key(&state->remote_key, head);
            state->receiving.size+=size;
            state->receiving.data=(uint8_t*)realloc(
                state->receiving.data,
                state->receiving.size
            );
        }
        else
        {
            //Decrypt message content
            struct block content;
            content.data=state->receiving.data+MESSAGE_HEADER_SIZE;
            content.size=state->receiving.size-MESSAGE_HEADER_SIZE;
            if(decrypt(&state->remote_key, &content))
            {
                fprintf(stderr, "Out of remote key data!\n");
                return 1;
            }
            return chat_handle_message(state);
        }
    }
    return 0;
}
static unsigned chat_handle_send(struct chat_state* state)
{
    if(state->sending.size==0)
    {
        return 0;
    }
    size_t sent=node_send(
        &state->remote,
        state->sending.data+state->sent_size,
        state->sending.size-state->sent_size
    );
    state->sent_size+=sent;
    if(state->sent_size==state->sending.size)
    {
        free_block(&state->sending);
        state->sent_size=0;
    }
    return 0;
}
void chat(struct chat_args* a)
{
    struct chat_state state;
    if(chat_init(a, &state))
    {
        return;
    }
    printf("Connected!\n");
    print_user_prompt(&state.local_key, "Local");
    while(state.remote.socket!=-1)
    {
        fd_set read_ready, write_ready;
        FD_ZERO(&read_ready);
        FD_SET(state.remote.socket, &read_ready);

        FD_ZERO(&write_ready);
        if(state.sending.size!=state.sent_size&&state.sending.size!=0)
        {
            //There's a message to send
            FD_SET(state.remote.socket, &write_ready);
        }
        else
        {
            //No message to send, read one.
            FD_SET(STDIN_FILENO, &read_ready);
        }
        if(
            select(
                state.remote.socket+1,
                &read_ready,
                &write_ready,
                NULL,
                NULL
            )==-1
        ){
            fprintf(stderr, "select() failed: %s\n", strerror(errno));
        }
        if(FD_ISSET(STDIN_FILENO, &read_ready))
        {
            if(chat_handle_input(&state))
            {
                break;
            }
        }
        if(FD_ISSET(state.remote.socket, &read_ready))
        {
            if(chat_handle_recv(&state))
            {
                break;
            }
        }
        if(FD_ISSET(state.remote.socket, &write_ready))
        {
            if(chat_handle_send(&state))
            {
                break;
            }
        }
    }
    chat_end(&state);
    return;
}
