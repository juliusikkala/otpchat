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
#include "key.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <endian.h>
#define KEY_MAGIC "OTPCHAT0"
#define KEY_HEAD_OFFSET 8
#define KEY_ID_OFFSET 16
#define KEY_DATA_OFFSET 32
#define BUFFER_SIZE 4096

unsigned open_key(const char* path, struct key* k)
{
    k->stream=fopen(path, "rb+");
    if(k->stream==NULL)
    {
        return 1;
    }

    char magic_check[8]={0};
    size_t sz=2;
    if(fread(magic_check, 1, 8, k->stream)!=8||
       strncmp(magic_check, KEY_MAGIC, 8)!=0||
       (sz=fread(&k->head, 1, sizeof(k->head), k->stream))!=sizeof(k->head)||
       fread(&k->id, 1, sizeof(k->id), k->stream)!=sizeof(k->id))
    {
        fclose(k->stream);
        return 1;
    }
    k->head=le64toh(k->head);
    //Get key size
    fseek(k->stream, 0, SEEK_END);
    k->size=ftell(k->stream)-KEY_DATA_OFFSET;
    if(fseek(k->stream, k->head+KEY_DATA_OFFSET, SEEK_SET)!=0)
    {
        fclose(k->stream);
        return 1;
    }
    return 0;
}
unsigned create_key(const char* path, struct key* k, size_t sz)
{
    k->stream=fopen(path, "wb+");
    if(k->stream==NULL)
    {
        return 1;
    }
    //Write magic sequence
    fwrite(KEY_MAGIC, 1, strlen(KEY_MAGIC), k->stream);
    //Write head address
    k->head=0;
    fwrite(&k->head, sizeof(k->head), 1, k->stream);

    int urandom=open("/dev/urandom", O_RDONLY);
    //Generate random id
    read(urandom, &k->id, sizeof(k->id));
    //Write id
    fwrite(&k->id, 1, sizeof(k->id), k->stream);

    //Write key data
    uint8_t* buffer=(uint8_t*)malloc(BUFFER_SIZE);
    size_t written=0;
    while(written<sz)
    {
        size_t bsize=sz-written;
        bsize=BUFFER_SIZE<bsize?BUFFER_SIZE:bsize;
        bsize=read(urandom, buffer, bsize);
        if(bsize<=0||fwrite(buffer, 1, bsize, k->stream)!=bsize)
        {
            free(buffer);
            fclose(k->stream);
            return 1;
        }
        written+=bsize;
    }
    free(buffer);
    fseek(k->stream, KEY_DATA_OFFSET, SEEK_SET);
    k->size=sz;
    return 0;
}
void close_key(struct key* k)
{
    //Save head index
    size_t head_le=htole64(k->head);
    fseek(k->stream, KEY_HEAD_OFFSET, SEEK_SET);
    fwrite(&head_le, sizeof(&head_le), 1, k->stream);
    //Close stream
    fclose(k->stream);
    k->stream=NULL;
}
void seek_key(struct key* k, uint64_t new_head)
{
    fseek(k->stream, new_head+KEY_DATA_OFFSET, SEEK_SET);
    k->head=new_head;
}

void create_block_from_str(const char* str, struct block* b)
{
    b->size=strlen(str);
    b->data=(uint8_t*)malloc(b->size);
    memcpy(b->data, str, b->size);
}
void create_block(size_t size, struct block* b)
{
    b->size=size;
    b->data=(uint8_t*)calloc(b->size, 1);
}
void free_block(struct block* b)
{
    if(b->data!=NULL)
    {
        free(b->data);
        b->data=NULL;
        b->size=0;
    }
}
static unsigned get_key_block(
    struct key* k,
    struct block* key_block,
    uint64_t bytes
){
    key_block->size=bytes;
    key_block->data=(uint8_t*)malloc(key_block->size);
    size_t read_bytes=fread(key_block->data, 1, bytes, k->stream);
    key_block->size=read_bytes;
    k->head+=read_bytes;

    if(read_bytes!=bytes)
    {
        return 1;
    }
    return 0;
}
unsigned encrypt(
    struct key* k,
    struct block* message
){
    struct block key_block;
    if(get_key_block(k, &key_block, message->size))
    {//There's not enough key data
        return 1;
    }
    for(uint64_t i=0;i<message->size;++i)
    {
        message->data[i]=key_block.data[i]^message->data[i];
    }
    free_block(&key_block);
    return 0;
}
unsigned decrypt(
    struct key* k,
    struct block* message
){
    return encrypt(k, message);
}
