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
#include "node.h"
#include "user.h"
#include "ui.h"
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <endian.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <locale.h>
#define MESSAGE_HEADER_SIZE 12
#define MESSAGE_SIZE_OFFSET 0
#define MESSAGE_HEAD_OFFSET 4

const char* chat_id_name(struct chat_state* state, uint32_t id)
{
    switch(id)
    {
    case ID_STATUS:
        return "Status message";
    case ID_LOCAL:
        return state->local.name;
    case ID_REMOTE:
        return state->remote.name;
    default:
        return "Unknown";
    }
}
void chat_push_message(
    struct chat_state* state,
    const struct message* msg
){
    state->history=(struct message*)realloc(
        state->history,
        (++state->history_size)*sizeof(struct message)
    );
    struct message* new_msg=&state->history[state->history_size-1];
    new_msg->id=msg->id;
    new_msg->timestamp=msg->timestamp;
    new_msg->text.size=msg->text.size;
    new_msg->text.data=(uint8_t*)malloc(msg->text.size);
    memcpy(new_msg->text.data, msg->text.data, msg->text.size);

    if(state->history_line!=0)
    {
        state->history_line+=ui_message_lines(new_msg, state->history_width);
    }

    ui_update(state);
}
void chat_push_status(
    struct chat_state* state,
    const char* format,
    ...
){
    va_list args, args_copy;
    va_start(args, format);
    va_copy(args_copy, args);

    struct message status;
    status.id=ID_STATUS;
    status.timestamp=time(NULL);
    status.text.size=vsnprintf(NULL, 0, format, args_copy);
    status.text.data=(uint8_t*)malloc(status.text.size+1);
    vsprintf((char*)status.text.data, format, args);
    chat_push_message(state, &status);
    free_message(&status);
    va_end(args);
    va_end(args_copy);
}
unsigned chat_begin_connect(struct chat_state* state, struct address* addr)
{
    if(user_begin_connect(&state->remote, addr))
    {
        chat_push_status(
            state,
            "Connecting to %s:%d failed",
            addr->node,
            addr->port
        );
        return 1;
    }
    else
    {
        chat_push_status(state, "Connecting to %s:%d", addr->node, addr->port);
    }
    return 0;
}
unsigned chat_begin_listen(struct chat_state* state, uint16_t port)
{
    node_close(&state->local.node);
    if(node_listen(&state->local.node, port))
    {
        chat_push_status(state, "Listening on port %d failed", port);
        return 1;
    }
    else
    {
        chat_push_status(state, "Listening on port %d", port);
    }
    return 0;
}
void chat_disconnect(struct chat_state* state, uint32_t id)
{
    struct user* u=NULL;
    switch(id)
    {
    case ID_REMOTE:
        u=&state->remote;
        break;
    default:
        break;
    }
    if(u!=NULL&&u->state!=NOT_CONNECTED)
    {
        user_disconnect(&state->remote);
        chat_push_status(state, "Disconnected");
    }
}
void chat_end_listen(struct chat_state* state)
{
    if(state->local.node.socket!=-1)
    {
        node_close(&state->local.node);
        chat_push_status(state, "Stopped listening for connections");
    }
}
static unsigned chat_init(struct chat_args* a, struct chat_state* state)
{
    key_store_init(&state->keys);
    if(key_store_open_local(&state->keys, a->local_key_path))
    {
        fprintf(stderr, "Unable to open \"%s\"\n", a->local_key_path);
        goto fail;
    }
    if(key_store_open_remote(&state->keys, a->remote_key_path))
    {
        fprintf(stderr, "Unable to open \"%s\"\n", a->remote_key_path);
        goto fail;
    }
    user_init(&state->local, ID_LOCAL);
    user_init(&state->remote, ID_REMOTE);
    user_set_name(&state->local, "Local");
    user_set_name(&state->remote, "Remote");
    state->local.key=&state->keys.local;

    state->receiving.data=NULL;
    state->receiving.size=0;
    state->received_size=0;
    state->sending.data=NULL;
    state->sending.size=0;
    state->sent_size=0;
    state->history=NULL;
    state->history_size=0;
    state->history_line=0;
    state->input.data=NULL;
    state->input.size=0;
    state->cursor_index=0;
    state->running=1;

    ui_init(state);

    if(a->wait_for_remote)
    {
        chat_begin_listen(state, a->addr.port);
    }
    else
    {
        chat_begin_connect(state, &a->addr);
    }
    return 0;
fail:
    key_store_close(&state->keys);
    return 1;
}
static void chat_end(struct chat_state* state)
{
    ui_end(state);
    user_close(&state->local);
    user_close(&state->remote);
    key_store_close(&state->keys);
    free_block(&state->receiving);
    free_block(&state->sending);
    free_block(&state->input);
    if(state->history!=NULL)
    {
        for(size_t i=0;i<state->history_size;++i)
        {
            free_message(&state->history[i]);
        }
        free(state->history);
    }
}
unsigned chat_begin_send(struct chat_state* state, struct block* b)
{
    free_block(&state->sending);
    state->sent_size=0;

    state->sending.size=b->size+MESSAGE_HEADER_SIZE;
    state->sending.data=(uint8_t*)malloc(state->sending.size);

    uint32_t size=htobe32((uint32_t)state->sending.size-MESSAGE_HEADER_SIZE);
    uint64_t head=htobe64(state->local.key->head);
    memcpy(state->sending.data+MESSAGE_SIZE_OFFSET, &size, sizeof(size));
    memcpy(state->sending.data+MESSAGE_HEAD_OFFSET, &head, sizeof(head));
    memcpy(
        state->sending.data+MESSAGE_HEADER_SIZE,
        b->data,
        b->size
    );
    struct block content;
    content.data=state->sending.data+MESSAGE_HEADER_SIZE;
    content.size=state->sending.size-MESSAGE_HEADER_SIZE;
    if(encrypt(state->local.key, &content))
    {
        chat_push_status(state, "Out of local key data!");
        return 1;
    }
    return 0;
}
static unsigned chat_handle_message(struct chat_state* state)
{
    struct message new_message;
    new_message.id=ID_REMOTE;
    new_message.timestamp=time(NULL);
    new_message.text.data=state->receiving.data+MESSAGE_HEADER_SIZE;
    new_message.text.size=state->receiving.size-MESSAGE_HEADER_SIZE;
    chat_push_message(state, &new_message);
    free_block(&state->receiving);
    return 0;
}
static unsigned chat_handle_recv(struct chat_state* state)
{
    if(state->receiving.size==0)
    {
        block_create(&state->receiving, MESSAGE_HEADER_SIZE);
        state->received_size=0;
    }
    size_t received=node_recv(
        &state->remote.node,
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
            key_seek(state->remote.key, head);
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
            if(decrypt(state->remote.key, &content))
            {
                chat_push_status(state, "Out of remote key data!");
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
        &state->remote.node,
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
    while(state.running)
    {
        fd_set read_ready, write_ready;
        FD_ZERO(&read_ready);
        FD_ZERO(&write_ready);
        FD_SET(STDIN_FILENO, &read_ready);

        int biggest=STDIN_FILENO;

        if(state.remote.state==CONNECTED)
        {
            FD_SET(state.remote.node.socket, &read_ready);
            biggest=state.remote.node.socket>biggest?
                    state.remote.node.socket:biggest;
        }
        if(state.remote.state==CONNECTING||
           (state.remote.state==CONNECTED&&
            state.sending.size!=state.sent_size&&state.sending.size!=0))
        {
            //There's a message to send or the socket is connecting
            FD_SET(state.remote.node.socket, &write_ready);
            biggest=state.remote.node.socket>biggest?
                    state.remote.node.socket:biggest;
        }
        if(a->wait_for_remote&&state.remote.state==NOT_CONNECTED)
        {
            FD_SET(state.local.node.socket, &read_ready);
            biggest=state.local.node.socket>biggest?
                    state.local.node.socket:biggest;
        }
        if(
            select(
                biggest+1,
                &read_ready,
                &write_ready,
                NULL,
                NULL
            )==-1
        ){
            //Resizing the terminal causes select to fail with "Interrupted
            //system call", so we just update the ui and carry on.
            if(errno==EINTR)
            {
                ui_update(&state);
                continue;
            }
            break;
        }
        if(FD_ISSET(state.remote.node.socket, &read_ready))
        {
            chat_handle_recv(&state);
        }
        if(FD_ISSET(state.remote.node.socket, &write_ready))
        {
            if(state.remote.state==CONNECTING)
            {
                if(user_finish_connect(&state.remote, &state.keys))
                {
                    user_disconnect(&state.remote);
                    state.remote.key=NULL;
                    chat_push_status(&state, "Connection failed");
                }
                else
                {
                    chat_push_status(&state, "Connected!");
                }
            }
            else
            {
                chat_handle_send(&state);
            }
        }
        if(FD_ISSET(state.local.node.socket, &read_ready))
        {
            if(user_accept(&state.remote, &state.local.node, &state.keys))
            {
                chat_push_status(&state, "Incoming connection failed");
            }
            else
            {
                chat_push_status(&state, "Connected!");
            }
        }
        if(FD_ISSET(STDIN_FILENO, &read_ready))
        {
            ui_handle_input(&state);
        }
        if(state.remote.state==CONNECTED&&node_error(&state.remote.node))
        {
            user_disconnect(&state.remote);
            state.remote.key=NULL;
            chat_push_status(&state, "Remote disconnected");
        }
    }
    chat_end(&state);
    return;
}
