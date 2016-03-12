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
#include "ui.h"
#include "chat.h"
#include "user.h"
#include "command.h"
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <ncurses.h>
#define COLOR_KEY_USED  1
#define COLOR_KEY_LEFT  2
#define COLOR_ID_OFFSET 3
#define COLOR_SCROLLBAR (COLOR_ID_OFFSET+ID_LOCAL)

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
unsigned ui_message_lines(struct message* msg, unsigned width)
{
    return 1+string_lines((char*)msg->text.data, msg->text.size, width);
}
unsigned ui_history_lines(struct chat_state* state)
{
    unsigned lines=0;
    size_t i=0;
    for(i=0;i<state->history_size;++i)
    {
        lines+=ui_message_lines(state->history+i, state->history_width);
    }
    return lines;
}
unsigned ui_handle_input(struct chat_state* state)
{
    int c=getch();
    if(c=='\n')
    {//Newline sends the message.
        if(state->input.size==0)
        {
            return 0;
        }
        if(state->input.data[0]=='/')
        {
            char* command_str=(char*)malloc(state->input.size);
            memcpy(command_str, state->input.data+1, state->input.size-1);
            command_str[state->input.size-1]=0;
            if(command_handle(state, command_str))
            {
                free(command_str);
                return 1;
            }
            free(command_str);
        }
        else if(state->remote.state==CONNECTED)
        {
            struct message msg;
            msg.text.data=state->input.data;
            msg.text.size=state->input.size;
            msg.id=ID_LOCAL;
            msg.timestamp=time(NULL);

            chat_push_message(state, &msg);
            chat_begin_send(state, &msg.text);
        }
        else
        {
            return 0;
        }

        free_block(&state->input);
        state->cursor_index=0;
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
        unsigned lines=ui_history_lines(state);
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
    ui_update(state);
    return 0;
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
static void draw_message_header(
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
static void draw_history(
    struct chat_state* state,
    int x, int y
){
    int line=y+state->history_height+state->history_line;
    for(int i=state->history_size-1;i>=0&&line>=0;--i)
    {
        unsigned msg_lines=ui_message_lines(
            state->history+i,
            state->history_width
        );
        line-=msg_lines;
        if(line>y+state->history_height)
        {
            continue;
        }
        draw_message_header(
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
static void draw_scrollbar(
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
static unsigned draw_key_usage(
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
void ui_update(struct chat_state* state)
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
    unsigned history_lines=ui_history_lines(state);
    if(history_lines>(unsigned)state->history_height)
    {
        state->history_width-=1;//Make room for scrollbar
        history_lines=ui_history_lines(state);
        //Draw scrollbar
        draw_scrollbar(
            width-1, 0,
            history_lines,
            history_lines-state->history_line,
            state->history_height
        );
    }
    draw_history(state, 0, 0);
    //Print margins around input box
    draw_rect(0, input_line, width, 1);
    draw_rect(0, input_line+2, width, 1);
    unsigned local_key_usage_len=draw_key_usage(
        "Local:  ",
        state->local.key,
        0,
        height-1,
        20
    );
    if(state->remote.key!=NULL)
    {
        draw_key_usage(
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
void ui_init(struct chat_state* state)
{
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
    ui_update(state);
}
void ui_end(struct chat_state* state)
{
    (void)state;
    endwin();
}
