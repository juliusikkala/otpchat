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
#include "user.h"
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
#include <locale.h>
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
    struct key_store keys;
    struct user local, remote;

    struct message* history;
    size_t history_size;
    struct message current_message;
    size_t cursor_index;

    struct block receiving;
    size_t received_size;

    struct block sending;
    size_t sent_size;
};

static const char* chat_id_name(struct chat_state* state, uint32_t id)
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
static int next_char(
    const char* mbs_begin,
    size_t cursor_pos,
    size_t mbs_len
){
    mblen(NULL, 0);
    size_t i=0;
    while(i<=cursor_pos&&i<mbs_len)
    {
        int len=mblen(mbs_begin+i, mbs_len-i);
        if(len<1) break;
        i+=len;
    }
    return (ssize_t)i-(ssize_t)cursor_pos;
}
static int prev_char(
    const char* mbs_begin,
    size_t cursor_pos,
    size_t mbs_len
){
    mblen(NULL, 0);
    size_t prev=0;
    size_t i=0;
    while(i<cursor_pos&&i<mbs_len)
    {
        int len=mblen(mbs_begin+i, mbs_len-i);
        if(len<1) break;
        prev=i;
        i+=len;
    }
    return (ssize_t)prev-(ssize_t)cursor_pos;
}
static size_t count_chars(const char* mbs_begin, size_t mbs_len)
{
    mblen(NULL, 0);
    size_t chars=0;
    size_t i=0;
    while(i<mbs_len)
    {
        int len=mblen(mbs_begin+i, mbs_len-i);
        if(len<1) break;
        i+=len;
        chars++;
    }
    return chars;
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
        state->local.key,
        0,
        height-1,
        20
    );
    if(state->remote.key!=NULL)
    {
        chat_draw_key_usage(
            "Remote: ",
            state->remote.key,
            local_key_usage_len+1,
            height-1,
            20
        );
    }
    chat_draw_message_text(&state->current_message, 0, input_line+1, width);
    //Move cursor to the correct position
    size_t cursor_pos=count_chars(
        (char*)state->current_message.text.data,
        state->cursor_index
    );
    move(input_line+1+cursor_pos/width, cursor_pos%width);
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

    chat_update_ui(state);
}
static void chat_push_status(
    struct chat_state* state,
    const char* format,
    ...
){
    va_list args, args_copy;
    va_start(args, format);
    va_copy(args_copy, args);

    state->history=(struct message*)realloc(
        state->history,
        (++state->history_size)*sizeof(struct message)
    );

    struct message* new_msg=&state->history[state->history_size-1];
    new_msg->id=ID_STATUS;
    new_msg->timestamp=time(NULL);
    new_msg->text.size=vsnprintf(NULL, 0, format, args_copy);
    new_msg->text.data=(uint8_t*)malloc(new_msg->text.size+1);
    vsprintf((char*)new_msg->text.data, format, args);

    va_end(args);
    va_end(args_copy);

    chat_update_ui(state);
}
static unsigned chat_init(struct chat_args* a, struct chat_state* state)
{
    init_key_store(&state->keys);
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
    state->current_message.id=ID_LOCAL;
    state->current_message.text.data=NULL;
    state->current_message.text.size=0;
    state->cursor_index=0;

    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    start_color();
    init_pair(COLOR_KEY_USED, COLOR_WHITE, COLOR_RED);
    init_pair(COLOR_KEY_LEFT, COLOR_WHITE, COLOR_GREEN);
    init_pair(ID_STATUS+COLOR_ID_OFFSET, COLOR_WHITE, COLOR_BLACK);
    init_pair(ID_LOCAL+COLOR_ID_OFFSET, COLOR_BLACK, COLOR_WHITE);
    init_pair(ID_REMOTE+COLOR_ID_OFFSET, COLOR_WHITE, COLOR_CYAN);
    chat_update_ui(state);

    if(a->wait_for_remote)
    {
        if(node_listen(&state->local.node, a->addr.port))
        {
            chat_push_status(
                state,
                "Listening on port %d failed",
                a->addr.port
            );
        }
        else
        {
            chat_push_status(state, "Listening on port %d", a->addr.port);
        }
    }
    else
    {
        if(user_begin_connect(&state->remote, &a->addr))
        {
            chat_push_status(
                state,
                "Connecting to %s:%d failed",
                a->addr.node,
                a->addr.port
            );
        }
        else
        {
            chat_push_status(
                state,
                "Connecting to %s:%d",
                a->addr.node,
                a->addr.port
            );
        }
    }
    return 0;
fail:
    close_key_store(&state->keys);
    return 1;
}
static void chat_end(struct chat_state* state)
{
    endwin();
    user_close(&state->local);
    user_close(&state->remote);
    close_key_store(&state->keys);
    free_block(&state->receiving);
    free_block(&state->sending);
    free_message(&state->current_message);
    for(size_t i=0;i<state->history_size;++i)
    {
        free_message(&state->history[i]);
    }
    free(state->history);
}
static unsigned chat_send_block(struct chat_state* state, struct block* b)
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
static unsigned chat_handle_input(struct chat_state* state)
{
    int c=getch();
    if(c=='\n')
    {//Newline sends the message.
        if(state->current_message.text.size==0
         ||state->remote.state==NOT_CONNECTED)
        {
            return 0;
        }
        unsigned fail=chat_send_block(state, &state->current_message.text);
        state->current_message.timestamp=time(NULL);
        chat_push_message(state, &state->current_message);

        free_message(&state->current_message);
        state->current_message.id=ID_LOCAL;
        state->cursor_index=0;
        if(fail)
        {
            return 1;
        }
    }
    else if(c<256)
    {//Not newline, message continues.
        state->current_message.text.data=(uint8_t*)realloc(
            state->current_message.text.data,
            state->current_message.text.size+1
        );
        memmove(
            state->current_message.text.data+state->cursor_index+1,
            state->current_message.text.data+state->cursor_index,
            state->current_message.text.size-state->cursor_index
        );
        state->current_message.text.data[
            state->cursor_index
        ]=(uint8_t)c;
        state->current_message.text.size++;
        state->cursor_index++;
    }
    else if(c==KEY_BACKSPACE)
    {//Backspace, remove preceding character
        int offset=prev_char(
            (char*)state->current_message.text.data,
            state->cursor_index,
            state->current_message.text.size
        );
        if(offset!=0)
        {
            memmove(
                state->current_message.text.data+state->cursor_index+offset,
                state->current_message.text.data+state->cursor_index,
                state->current_message.text.size-state->cursor_index
            );
            state->cursor_index+=offset;
            state->current_message.text.size+=offset;
        }
    }
    else if(c==KEY_LEFT)
    {//Move one character back
        int offset=prev_char(
            (char*)state->current_message.text.data,
            state->cursor_index,
            state->current_message.text.size
        );
        state->cursor_index+=offset;
    }
    else if(c==KEY_RIGHT)
    {//Move one character forwards
        int offset=next_char(
            (char*)state->current_message.text.data,
            state->cursor_index,
            state->current_message.text.size
        );
        state->cursor_index+=offset;
    }
    else if(c==KEY_HOME)
    {
        state->cursor_index=0;
    }
    else if(c==KEY_END)
    {
        state->cursor_index=state->current_message.text.size;
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
            seek_key(state->remote.key, head);
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
    while(1)
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
                chat_update_ui(&state);
                continue;
            }
            break;
        }
        if(FD_ISSET(STDIN_FILENO, &read_ready))
        {
            if(chat_handle_input(&state))
            {
                break;
            }
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
            if(user_accept(&state.local.node, &state.remote, &state.keys))
            {
                chat_push_status(&state, "Incoming connection failed");
            }
            else
            {
                chat_push_status(&state, "Connected!");
            }
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
