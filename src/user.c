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
#include "user.h"
#include <stdlib.h>
#include <string.h>
#define TIMEOUT_MS 2000
#define PROTOCOL_ID "OTPCHAT0"

void user_init(struct user* u, uint32_t id)
{
    u->key=NULL;
    u->node.socket=-1;
    u->name=NULL;
    u->state=NOT_CONNECTED;
    u->id=id;
}
void user_set_name(struct user* u, const char* name)
{
    if(u->name!=NULL)
    {
        free(u->name);
        u->name=NULL;
    }
    u->name=(char*)malloc(strlen(name)+1);
    strcpy(u->name, name);
}
unsigned user_begin_connect(struct user* u, struct address* addr)
{
    if(node_connect(&u->node, addr))
    {
        return 1;
    }
    u->state=CONNECTING;
    return 0;
}
static unsigned user_handshake(
    struct user* u,
    struct key_store* keys,
    unsigned timeout_ms
){
    uint8_t send_message[8+sizeof(keys->local.id)]={0};
    uint8_t recv_message[8+sizeof(keys->local.id)]={0};
    memcpy(send_message, PROTOCOL_ID, 8);
    memcpy(send_message+8, keys->local.id, sizeof(keys->local.id));

    if( node_exchange(
            &u->node,
            send_message,
            sizeof(send_message),
            recv_message,
            sizeof(recv_message),
            &timeout_ms
        )
    ){
        return 4;
    }
    if(memcmp(recv_message, PROTOCOL_ID, 8)!=0)
    {
        return 1;
    }
    u->key=key_store_find(keys, recv_message+8);
    uint8_t local_accept=u->key!=NULL;
    uint8_t remote_accept=0;
    if( node_exchange(
            &u->node,
            &local_accept,
            1,
            &remote_accept,
            1,
            &timeout_ms
        )
    ){
        return 4;
    }
    if(!remote_accept)
    {
        return 2;
    }
    if(!local_accept)
    {
        return 3;
    }
    return 0;
}
unsigned user_finish_connect(
    struct user* u,
    struct key_store* keys
){
    if(node_error(&u->node))
    {
        return 1;
    }
    if(user_handshake(u, keys, TIMEOUT_MS))
    {
        user_disconnect(u);
        return 1;
    }
    u->state=CONNECTED;
    return 0;
}
unsigned user_accept(
    struct node* listen_node,
    struct user* u,
    struct key_store* keys
){
    if(node_accept(listen_node, &u->node))
    {
        return 1;
    }
    return user_finish_connect(u, keys);
}
void user_disconnect(struct user* u)
{
    node_close(&u->node);
    u->state=NOT_CONNECTED;
}
void user_close(struct user* u)
{
    user_disconnect(u);
    if(u->name!=NULL)
    {
        free(u->name);
        u->name=NULL;
    }
    u->key=NULL;
}
