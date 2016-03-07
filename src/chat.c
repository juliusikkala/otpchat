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
#include <time.h>
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
#define ID_STATUS  0
#define ID_LOCAL  1
#define ID_REMOTE 2
#define COLOR_KEY_USED  1
#define COLOR_KEY_LEFT  2
#define COLOR_ID_OFFSET 3

struct message
{
    uint32_t id;
    time_t timestamp;
    struct block text;
};
static void free_message(struct message* msg)
{
    free_block(&msg->text);
    msg->id=0;
}
struct chat_state
{
    struct key local_key, remote_key;
    struct node remote;

    struct message* history;
    size_t history_size;
    struct message current_message;

    struct block receiving;
    size_t received_size;

    struct block sending;
    size_t sent_size;
};

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
static const char* chat_id_name(struct chat_state* state, uint32_t id)
{
    switch(id)
    {
    case ID_STATUS:
        return "Status message";
    case ID_LOCAL:
        return "Local";
    case ID_REMOTE:
        return "Remote";
    default:
        return "Unknown";
    }
}
static unsigned chat_message_lines(const struct message* msg, unsigned width)
{
    return msg->text.size==0?0:(msg->text.size-1)/width+1;
}
static void draw_rect(int x, int y, unsigned w, unsigned h)
{
    for(unsigned i=0;i<h;++i)
    {
        move(y+i, x);
        for(unsigned j=0;j<w;++j)
        {
            addch(' ');
        }
    }
}
static void chat_draw_message_header(
    struct chat_state* state,
    struct message* msg,
    int x, int y, int width
){
    char date[9];
    strftime(
        date,
        sizeof(date),
        "%H:%M:%S",
        localtime(&msg->timestamp)
    );
    attron(COLOR_PAIR(msg->id+COLOR_ID_OFFSET));
    draw_rect(x, y, width, 1);
    mvprintw(y, x, "%s [%s]:", chat_id_name(state, msg->id), date);
    attroff(COLOR_PAIR(msg->id+COLOR_ID_OFFSET));
}
static void chat_draw_message_text(
    struct message* msg,
    int x, int y, unsigned width
){
    unsigned line=0;
    if(y<0)
    {
        line=-y;
    }
    unsigned lines=chat_message_lines(msg, width);
    //Move cursor to the wanted position even if the loop below does not
    //execute.
    attron(COLOR_PAIR(msg->id+COLOR_ID_OFFSET));
    draw_rect(x, y+line, width, lines-line==0?1:lines-line);
    move(y, x);
    for(;line<lines;++line)
    {
        size_t characters_left=msg->text.size-width*line;
        mvprintw(
            y+line,
            x,
            "%.*s", 
            characters_left<width?characters_left:width,
            (char*)msg->text.data+width*line
        );
    }
    attroff(COLOR_PAIR(msg->id+COLOR_ID_OFFSET));
}
static unsigned chat_draw_key_usage(
    const char* info_text,
    struct key* k,
    int x, int y, unsigned min_width
){
    mvprintw(y, x, info_text);
    unsigned usage_len=snprintf(NULL, 0, "%lu/%lu", k->head, k->size);
    char* usage_str=(char*)malloc(usage_len+1);
    sprintf(usage_str, "%lu/%lu", k->head, k->size);

    unsigned width=min_width<usage_len?usage_len:min_width;
    unsigned usage_offset=(width-usage_len)/2;
    unsigned used=(k->head/(double)k->size)*width;
    unsigned left=width-used;
    unsigned i=0;
    attron(COLOR_PAIR(COLOR_KEY_USED));
    for(unsigned j=0;i<used;++j, ++i)
    {
        if(i>=usage_offset&&i-usage_offset<usage_len)
        {
            addch(usage_str[i-usage_offset]);
        }
        else
        {
            addch(' ');
        }
    }
    attroff(COLOR_PAIR(COLOR_KEY_USED));
    attron(COLOR_PAIR(COLOR_KEY_LEFT));
    for(unsigned j=0;j<left;++j, ++i)
    {
        if(i>=usage_offset&&i-usage_offset<usage_len)
        {
            addch(usage_str[i-usage_offset]);
        }
        else
        {
            addch(' ');
        }
    }
    attroff(COLOR_PAIR(COLOR_KEY_LEFT));
    free(usage_str);
    return strlen(info_text)+width;
}
static void chat_update_ui(struct chat_state* state)
{
    clear();
    int width, height;
    getmaxyx(stdscr, height, width);

    int input_line=height;

    int lines=chat_message_lines(&state->current_message, width);
    if(lines==0)
    {
        lines=1;
    }
    input_line-=lines+2;

    int history_line=input_line;
    //Print past messages
    for(int i=state->history_size-1;i>=0&&history_line>=0;--i)
    {
        lines=chat_message_lines(&state->history[i], width);
        history_line-=lines+1;
        chat_draw_message_header(
            state,
            &state->history[i],
            0,
            history_line,
            width
        );
        chat_draw_message_text(&state->history[i], 0, history_line+1, width);
    }
    //Print input box
    draw_rect(0, input_line, width, 1);
    draw_rect(0, input_line+2, width, 1);
    unsigned local_key_usage_len=chat_draw_key_usage(
        "Local:  ",
        &state->local_key,
        0,
        input_line+2,
        20
    );
    chat_draw_key_usage(
        "Remote: ",
        &state->remote_key,
        local_key_usage_len+1,
        input_line+2,
        20
    );
    chat_draw_message_text(&state->current_message, 0, input_line+1, width);
    refresh();
}
static void chat_push_message(
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
}
static void chat_push_status(
    struct chat_state* state,
    const char* status_message
){
    struct message temp;
    temp.id=ID_STATUS;
    temp.timestamp=time(NULL);
    temp.text.size=strlen(status_message);
    temp.text.data=(uint8_t*)status_message;
    chat_push_message(state, &temp);
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
    state->history=NULL;
    state->history_size=0;
    state->current_message.id=ID_LOCAL;
    state->current_message.text.data=NULL;
    state->current_message.text.size=0;

    initscr();
    cbreak();
    noecho();
    start_color();
    init_pair(COLOR_KEY_USED, COLOR_WHITE, COLOR_RED);
    init_pair(COLOR_KEY_LEFT, COLOR_WHITE, COLOR_GREEN);
    init_pair(ID_STATUS+COLOR_ID_OFFSET, COLOR_WHITE, COLOR_BLACK);
    init_pair(ID_LOCAL+COLOR_ID_OFFSET, COLOR_BLACK, COLOR_WHITE);
    init_pair(ID_REMOTE+COLOR_ID_OFFSET, COLOR_WHITE, COLOR_CYAN);
    chat_update_ui(state);
    return 0;
end:
    close_key(&state->local_key);
    close_key(&state->remote_key);
    return 1;
}
static void chat_end(struct chat_state* state)
{
    endwin();
    node_close(&state->remote);
    close_key(&state->local_key);
    close_key(&state->remote_key);
    free_block(&state->receiving);
    free_block(&state->sending);
}
static unsigned chat_send_block(struct chat_state* state, struct block* b)
{
    free_block(&state->sending);
    state->sent_size=0;

    state->sending.size=b->size+MESSAGE_HEADER_SIZE;
    state->sending.data=(uint8_t*)malloc(state->sending.size);

    uint32_t size=htobe32((uint32_t)state->sending.size-MESSAGE_HEADER_SIZE);
    uint64_t head=htobe64(state->local_key.head);
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
    if(encrypt(&state->local_key, &content))
    {
        fprintf(stderr, "Out of local key data!\n");
        return 1;
    }
    return 0;
}
static unsigned chat_handle_input(struct chat_state* state)
{
    char c=getch();
    if(c=='\n')
    {//Newline sends the message.
        if(state->current_message.text.size==0)
        {
            return 0;
        }
        unsigned fail=chat_send_block(state, &state->current_message.text);
        state->current_message.timestamp=time(NULL);
        chat_push_message(state, &state->current_message);

        free_message(&state->current_message);
        state->current_message.id=ID_LOCAL;
        if(fail)
        {
            return 1;
        }
    }
    else
    {//Not newline, message continues.
        state->current_message.text.data=(uint8_t*)realloc(
            state->current_message.text.data,
            ++state->current_message.text.size
        );
        state->current_message.text.data[
            state->current_message.text.size-1
        ]=(uint8_t)c;
    }
    chat_update_ui(state);
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
    chat_update_ui(state);
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
