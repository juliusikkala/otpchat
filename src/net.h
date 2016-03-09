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
#ifndef OTPCHAT_NET_H_
#define OTPCHAT_NET_H_
    #include <stdint.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netdb.h>
    struct address
    {
        char* node;
        uint16_t port;
    };
    //Parses an address of form node:port. Returns non-zero on failure.
    //If the port is unspecified, it will be set to 0
    unsigned parse_address(
        const char* text,
        struct address* addr
    );
    void free_address(struct address* addr);

    struct key;
    struct node
    {
        struct addrinfo* info;
        int socket;
    };

    void node_close(struct node* n);
    unsigned node_error(struct node* n);

    void node_get_address(struct node* n, struct address* addr);
    //Returns non-zero on failure.
    //On success, the connection is being formed asynchronously.
    //Use select() or poll() on node->socket to wait for the connecting to
    //finish.
    unsigned node_connect(
        struct node* remote,
        const struct address* addr
    );
    //Returns non-zero on failure.
    //Creates a local node, binds its socket and starts listening on it.
    unsigned node_listen(struct node* local, uint16_t port);

    //Returns non-zero on failure.
    unsigned node_accept(
        struct node* local,
        struct node* remote
    );
    //Returns non-zero on failure.
    //1 - Protocol mismatch
    //2 - Local key not recognized by remote
    //3 - Remote key not recognized
    //4 - Timed out
    unsigned node_handshake(
        struct node* remote,
        struct key* local_key,
        struct key* remote_keys,
        size_t remote_keys_sz,
        struct key** selected_remote_key,
        unsigned timeout_ms
    );

    //Returns the number of bytes sent.
    //Does not block. Use select() on remote->socket before calling.
    size_t node_send(
        struct node* remote,
        const void* data,
        size_t size
    );
    //Returns the number of bytes received.
    //Does not block. Use select() on remote->socket before calling.
    size_t node_recv(
        struct node* remote,
        void* data,
        size_t size
    );
    //Returns non-zero on failure.
    //Sends send_data and receives into recv_data simultaneously.
    //Blocks until timeout. timeout_ms will be set to the time left on success.
    unsigned node_exchange(
        struct node* remote,
        const void* send_data,
        size_t send_size,
        void* recv_data,
        size_t recv_size,
        unsigned* timeout_ms
    );
#endif
