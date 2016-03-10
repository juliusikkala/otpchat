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
#include <math.h>
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
#define COLOR_SCROLLBAR (COLOR_ID_OFFSET+ID_LOCAL)

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
    size_t history_line;
    int history_width, history_height;//width and height of the history box

    struct block input;
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
static size_t count_bytes(
    const char* mbs_begin,
    size_t mbs_len,
    size_t begin_index,
    size_t chars
){
    mblen(NULL, 0);
    size_t i=0, j=0;
    while(i<mbs_len&&j<chars)
    {
        int len=mblen(mbs_begin+i, mbs_len-i);
        if(len<1) break;
        if(i>=begin_index)
        {
            j++;
        }
        i+=len;
    }
    return i-begin_index;
}
static unsigned string_lines(const char* str, size_t strlen, unsigned width)
{
    size_t chars=count_chars(str, strlen);
    return chars==0?0:(chars-1)/width+1;
}
static unsigned chat_message_lines(struct message* msg, unsigned width)
{
    return 1+string_lines((char*)msg->text.data, msg->text.size, width);
}
static unsigned count_history_lines(struct chat_state* state)
{
    unsigned lines=0;
    size_t i=0;
    for(i=0;i<state->history_size;++i)
    {
        lines+=chat_message_lines(state->history+i, state->history_width);
    }
    return lines;
}
static void draw_rect(int x, int y, unsigned w, unsigned h)
{
    for(unsigned i=0;i<h;++i)
    {
        if(y+(int)i<0) continue;
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
    if(y<0) return;
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
static void draw_text_rect(
    const char* str, size_t strlen,
    int color_id,
    int x, int y, unsigned width
){
    unsigned line=0;
    unsigned lines=string_lines(str, strlen, width);
    size_t str_offset=0;
    attron(COLOR_PAIR(color_id));
    draw_rect(x, y+line, width, lines-line==0?1:lines-line);
    //Move cursor to the wanted position even if the loop below does not
    //execute.
    move(y, x);
    for(;line<lines;++line)
    {
        size_t len=count_bytes(str, strlen, str_offset, width);
        if(y+(int)line>=0)
        {
            mvprintw(
                y+line,
                x,
                "%.*s",
                len,
                str+str_offset
            );
        }
        str_offset+=len;
    }
    attroff(COLOR_PAIR(color_id));
}
static void chat_draw_history(
    struct chat_state* state,
    int x, int y
){
    int line=y+state->history_height+state->history_line;
    for(int i=state->history_size-1;i>=0&&line>=0;--i)
    {
        unsigned msg_lines=chat_message_lines(
            state->history+i,
            state->history_width
        );
        line-=msg_lines;
        if(line>y+state->history_height)
        {
            continue;
        }
        chat_draw_message_header(
            state,
            &state->history[i],
            x,
            line,
            state->history_width
        );
        draw_text_rect(
            (char*)state->history[i].text.data,
            state->history[i].text.size,
            state->history[i].id+COLOR_ID_OFFSET,
            x, line+1, state->history_width
        );
    }
}
static void chat_draw_scrollbar(
    int x, int y,
    unsigned content_h,
    unsigned content_offset,
    unsigned displayed_h
){
    float bar_ratio_height=displayed_h/(float)content_h;
    float bar_ratio_top=((int)content_offset-displayed_h)/(float)content_h;
    int bar_height=(int)(displayed_h*bar_ratio_height)+1;
    int bar_top=displayed_h*bar_ratio_top;
    attron(COLOR_PAIR(COLOR_SCROLLBAR));
    draw_rect(x, y+bar_top, 1, bar_height);
    attroff(COLOR_PAIR(COLOR_SCROLLBAR));
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

    int lines=string_lines((char*)state->input.data, state->input.size, width);
    if(lines==0)
    {
        lines=1;
    }
    input_line-=lines+2;

    state->history_width=width;
    state->history_height=height-lines-2;
    unsigned history_lines=count_history_lines(state);
    if(history_lines>(unsigned)state->history_height)
    {
        state->history_width-=1;//Make room for scrollbar
        history_lines=count_history_lines(state);
        //Draw scrollbar
        chat_draw_scrollbar(
            width-1, 0,
            history_lines,
            history_lines-state->history_line,
            state->history_height
        );
    }
    chat_draw_history(state, 0, 0);
    //Print margins around input box
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
    //Print input box
    draw_text_rect(
        (char*)state->input.data,
        state->input.size,
        COLOR_ID_OFFSET+ID_LOCAL,
        0, input_line+1, width
    );
    //Move cursor to the correct position
    size_t cursor_pos=count_chars(
        (char*)state->input.data,
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

    if(state->history_line!=0)
    {
        int width, height;
        getmaxyx(stdscr, height, width);
        (void)height;//Stop the compiler from complaining
        state->history_line+=chat_message_lines(new_msg, width);
    }

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
    state->history_line=0;
    state->input.data=NULL;
    state->input.size=0;
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
    free_block(&state->input);
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
        if(state->input.size==0||state->remote.state==NOT_CONNECTED)
        {
            return 0;
        }
        struct message msg;
        msg.text.data=state->input.data;
        msg.text.size=state->input.size;
        msg.id=ID_LOCAL;
        msg.timestamp=time(NULL);

        chat_push_message(state, &msg);
        unsigned fail=chat_send_block(state, &msg.text);

        free_block(&state->input);
        state->cursor_index=0;
        if(fail)
        {
            return 1;
        }
    }
    else if(c<256)
    {//Not newline, message continues.
        state->input.data=(uint8_t*)realloc(
            state->input.data,
            state->input.size+1
        );
        memmove(
            state->input.data+state->cursor_index+1,
            state->input.data+state->cursor_index,
            state->input.size-state->cursor_index
        );
        state->input.data[state->cursor_index]=(uint8_t)c;
        state->input.size++;
        state->cursor_index++;
    }
    else if(c==KEY_BACKSPACE)
    {//Backspace, remove preceding character
        int offset=prev_char(
            (char*)state->input.data,
            state->cursor_index,
            state->input.size
        );
        if(offset!=0)
        {
            memmove(
                state->input.data+state->cursor_index+offset,
                state->input.data+state->cursor_index,
                state->input.size-state->cursor_index
            );
            state->cursor_index+=offset;
            state->input.size+=offset;
        }
    }
    else if(c==KEY_LEFT)
    {//Move one character back
        int offset=prev_char(
            (char*)state->input.data,
            state->cursor_index,
            state->input.size
        );
        state->cursor_index+=offset;
    }
    else if(c==KEY_RIGHT)
    {//Move one character forwards
        int offset=next_char(
            (char*)state->input.data,
            state->cursor_index,
            state->input.size
        );
        state->cursor_index+=offset;
    }
    else if(c==KEY_UP)
    {
        unsigned lines=count_history_lines(state);
        if(state->history_line+state->history_height<lines)
        {
            state->history_line++;
        }
    }
    else if(c==KEY_DOWN)
    {
        if(state->history_line>0)
        {
            state->history_line--;
        }
    }
    else if(c==KEY_HOME)
    {
        state->cursor_index=0;
    }
    else if(c==KEY_END)
    {
        state->cursor_index=state->input.size;
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
