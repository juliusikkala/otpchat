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
#include "net.h"
#include "key.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <endian.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>

unsigned parse_address(const char* text, struct address* addr)
{
    //Find last ':'
    const char* separator=strrchr(text, ':');
    if(separator==NULL)
    {
        //No separator, so port is unspecified.
        size_t len=strlen(text);
        addr->node=(char*)malloc(len+1);
        memcpy(addr->node, text, len);
        addr->port=0;
        return 0;
    }
    //Read node name
    size_t len=separator-text;
    if(len==0)
    {
        return 1;
    }
    addr->node=(char*)malloc(len+1);
    memcpy(addr->node, text, len);
    addr->node[separator-text]=0;
    //Read port number
    char* port_end=NULL;
    long int port=strtol(separator+1, &port_end, 0);
    if(port<=0||port>=(1<<16)||*port_end!=0)
    {
        free(addr->node);
        return 1;
    }
    addr->port=port;
    return 0;
}
void free_address(struct address* addr)
{
    if(addr->node!=NULL)
    {
        free(addr->node);
        addr->node=NULL;
    }
    addr->port=0;
}
void node_close(struct node* n)
{
    if(n->socket>0)
    {
        close(n->socket);
        n->socket=-1;
    }
    if(n->info!=NULL)
    {
        freeaddrinfo(n->info);
        n->info=NULL;
    }
}
unsigned node_error(struct node* n)
{
    if(n->socket==-1)
    {
        return ENOTSOCK;
    }
    int err=0;
    socklen_t sz=sizeof(err);
    if(getsockopt(n->socket, SOL_SOCKET, SO_ERROR, &err, &sz)==-1)
    {
        return errno;
    }
    if(err!=0&&err!=EINPROGRESS)
    {
        return err;
    }
    return 0;
}
void node_get_address(struct node* n, struct address* addr)
{
    addr->node=(char*)malloc(INET6_ADDRSTRLEN);

    if(n->info->ai_addr->sa_family==AF_INET)
    {
        struct sockaddr_in* sa=(struct sockaddr_in*)n->info->ai_addr;
        inet_ntop(
            n->info->ai_family,
            &(sa->sin_addr),
            addr->node,
            INET6_ADDRSTRLEN
        );
        addr->port=ntohs(sa->sin_port);
    }
    else
    {
        struct sockaddr_in6* sa=(struct sockaddr_in6*)n->info->ai_addr;
        inet_ntop(
            n->info->ai_family,
            &(sa->sin6_addr),
            addr->node,
            INET6_ADDRSTRLEN
        );
        addr->port=ntohs(sa->sin6_port);
    }
}
unsigned node_connect(
    struct node* remote,
    const struct address* addr
){
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family=AF_UNSPEC;
    hints.ai_socktype=SOCK_STREAM;
    
    char port_str[6]={0};
    snprintf(port_str, sizeof(port_str), "%d", addr->port);

    remote->info=NULL;
    remote->socket=-1;
    
    if(getaddrinfo(addr->node, port_str, &hints, &remote->info))
    {
        return 1;
    }
    remote->socket=socket(
        remote->info->ai_family,
        remote->info->ai_socktype,
        remote->info->ai_protocol
    );
    if(remote->socket==-1)
    {
        freeaddrinfo(remote->info);
        return 1;
    }
    //Set socket non-blocking
    fcntl(remote->socket, F_SETFL, O_NONBLOCK);
    if(
        connect(
            remote->socket,
            remote->info->ai_addr,
            remote->info->ai_addrlen
        )!=0&&errno!=EINPROGRESS
    ){
        close(remote->socket);
        freeaddrinfo(remote->info);
        return 1;
    }
    return 0;
}
unsigned node_listen(struct node* local, uint16_t port)
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family=AF_UNSPEC;
    hints.ai_socktype=SOCK_STREAM;
    hints.ai_flags=AI_PASSIVE;
    
    char port_str[6]={0};
    snprintf(port_str, sizeof(port_str), "%d", port);

    local->info=NULL;
    local->socket=-1;
    
    if(getaddrinfo(NULL, port_str, &hints, &local->info))
    {
        return 1;
    }
    local->socket=socket(
        local->info->ai_family,
        local->info->ai_socktype,
        local->info->ai_protocol
    );
    if(local->socket==-1)
    {
        freeaddrinfo(local->info);
        return 1;
    }
    int arg=1;
    //Enable socket reuse.
    setsockopt(local->socket, SOL_SOCKET, SO_REUSEADDR, &arg, sizeof(arg));
    if(bind(local->socket, local->info->ai_addr, local->info->ai_addrlen)==-1||
       listen(local->socket, 5)==-1)
    {
        close(local->socket);
        freeaddrinfo(local->info);
        return 1;
    }
    return 0;
}
unsigned node_accept(
    struct node* local,
    struct node* remote
){
    remote->info=(struct addrinfo*)malloc(sizeof(struct addrinfo));
    memset(remote->info, 0, sizeof(struct addrinfo));
    remote->info->ai_addrlen=sizeof(struct sockaddr_storage);
    remote->info->ai_addr=(struct sockaddr*)malloc(remote->info->ai_addrlen);
    remote->socket=accept(
        local->socket,
        remote->info->ai_addr,
        &remote->info->ai_addrlen
    );
    if(remote->socket==-1)
    {
        freeaddrinfo(remote->info);
        remote->info=NULL;
        return 1;
    }
    //Set socket non-blocking
    fcntl(remote->socket, F_SETFL, O_NONBLOCK);
    return 0;
}

size_t node_send(
    struct node* remote,
    const void* data,
    size_t size
){
    ssize_t sent=send(remote->socket, data, size, 0);
    if(sent==-1)
    {
        switch(errno)
        {
        default:
        case ECONNRESET:
        case ENOTCONN:
            close(remote->socket);
            //fallthrough intentional
        case EBADF:
        case ENOTSOCK:
            remote->socket=-1;
            break;
        }
        return 0;
    }
    return (size_t)sent;
}
size_t node_recv(
    struct node* remote,
    void* data,
    size_t size
){
    ssize_t received=recv(remote->socket, data, size, 0);
    if(received<=0)
    {
        close(remote->socket);
        remote->socket=-1;
        return 0;
    }
    return (size_t)received;
}
unsigned node_exchange(
    struct node* remote,
    const void* send_data,
    size_t send_size,
    void* recv_data,
    size_t recv_size,
    unsigned* timeout_ms
){
    struct timeval timeout_tv={0};
    if(timeout_ms!=NULL)
    {
        timeout_tv.tv_sec=*timeout_ms/1000;
        timeout_tv.tv_usec=(*timeout_ms%1000)*1000;
    }
    fd_set remote_fds;
    FD_ZERO(&remote_fds);
    FD_SET(remote->socket, &remote_fds);
    while((send_size>0||recv_size>0)&&remote->socket!=-1)
    {
        fd_set readfds, writefds;
        if(recv_size>0)
        {
            readfds=remote_fds;
        }
        else
        {
            FD_ZERO(&readfds);
        }
        if(send_size>0)
        {
            writefds=remote_fds;
        }
        else
        {
            FD_ZERO(&writefds);
        }
        struct timeval timeout_temp={timeout_tv.tv_sec, timeout_tv.tv_usec};
        struct timeval begin_tv={0};
        struct timeval end_tv={0};
        gettimeofday(&begin_tv, NULL);
        if( select(
                remote->socket+1,
                &readfds,
                &writefds,
                NULL,
                timeout_ms!=NULL?&timeout_temp:NULL
            )==-1
        )
        {
            if(errno==EINTR)
            {
                continue;
            }
            return 1;
        }
        gettimeofday(&end_tv, NULL);
        struct timeval diff_tv;
        timersub(&end_tv, &begin_tv, &diff_tv);
        timersub(&timeout_tv, &diff_tv, &timeout_tv);
        if(FD_ISSET(remote->socket, &readfds))
        {
            size_t read=node_recv(remote, recv_data, recv_size);
            recv_data=((char*)recv_data)+read;
            recv_size-=read;
        }
        if(FD_ISSET(remote->socket, &writefds))
        {
            size_t write=node_send(remote, send_data, send_size);
            send_data=((char*)send_data)+write;
            send_size-=write;
        }
    }
    if(send_size>0||recv_size>0)
    {
        return 1;
    }
    if(timeout_ms!=NULL)
    {
        *timeout_ms=timeout_tv.tv_sec*1000+timeout_tv.tv_usec/1000;
    }
    return 0;
}
