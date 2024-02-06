#ifndef __TYPES_H__
#define __TYPES_H__

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#define SERVER_NAME "SPARK_DPU"

#define MAX_CC_MSG_SIZE 4000
#define MAX_NUM_BLOCK   1024
#define MAX_BLOCK_SIZE  MAX_DMA_BUF_SIZE
#define MAX_KV_PER_BLOCK      ((MAX_BLOCK_SIZE - 16) / 8)

#define SLEEP_TIME  1000

#define MSG_TYPE_INIT 1000
#define MSG_TYPE_WRITE 1001
#define MSG_TYPE_READ 1002
#define MSG_TYPE_WAIT 1003

struct kv_pair_t {
    int key;
    int val;
};

struct msg_init_t {
    int type;
    int sid;
    int num_mapper;
    int num_reducer;
    void* write_desc;
    void* read_desc;
    void* read_addr;
    size_t read_size;
};

struct msg_write_t {
    int type;
    int msg_id;
    int mid;
    int num_task;
};

struct write_task_t {
    void *addr;
    int rid;
    int num_kv;
};


struct msg_read_t {
    int type;
    uint32_t dst;
    int msg_id;
    int num_task;
};

struct read_task_t {
    int mid;
    int rid;
};

struct msg_wait_t {
    int type;
    int msg_id;
    void *addr;
    size_t size;
};

#endif
