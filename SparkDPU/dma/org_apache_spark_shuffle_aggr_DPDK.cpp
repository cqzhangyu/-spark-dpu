#define _GNU_SOURCE
extern "C" {
        
    #include <fcntl.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <stdint.h>
    #include <string.h>
    #include <unistd.h>
    #include <stdbool.h>
    #include <sys/msg.h>
    #include <sys/ipc.h>
    #include <sys/stat.h>
    #include <sys/mman.h>
    #include <sys/types.h>
    #include <sys/types.h>
    #include <sys/ioctl.h>
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <pthread.h>

    #include "org_apache_spark_shuffle_aggr_DPDK.h"
    #include "pack.h"
    #include "utils.h"
    #include "common.h"
    #include "dma_copy_core.h"

    #include <doca_buf.h>
    #include <doca_buf_inventory.h>
    #include <doca_ctx.h>
    #include <doca_dev.h>
    #include <doca_dma.h>
    #include <doca_mmap.h>
};
#include <queue>
#include <vector>
#include <map>
#include <mutex>
#include <mutex>

#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)

#define SERVER_NAME "SPARK_DPU"

#define MAX_DOCA_ARGC 30
#define MAX_DOCA_LEN_ARGV 50

#define MAX_CC_MSG_SIZE 4000
#define MAX_NUM_BLOCK   1024
#define MAX_BLOCK_SIZE  MAX_DMA_BUF_SIZE
#define MAX_KV_PER_BLOCK      ((MAX_BLOCK_SIZE - 16) / 8)

#define MAX_MAPPER_PER_WORKER   16
#define MAX_REDUCER_PER_WORKER  16

#define MSG_TYPE_INIT 1000
#define MSG_TYPE_WRITE 1001
#define MSG_TYPE_READ 1002
#define MSG_TYPE_WAIT 1003

#define SLEEP_TIME  1000

DOCA_LOG_REGISTER(IPC)

struct dma_context_t {
    struct dma_copy_cfg dma_cfg;
    struct core_state core_state;
    struct doca_comm_channel_ep_t *ep;
    struct doca_comm_channel_addr_t *peer_addr;
    struct doca_dev *cc_dev;
    struct doca_dev_rep *cc_dev_rep;

    void *write_desc;
    void *read_desc;
    char *write_buf;
    char *read_buf;

    int sid;
    int num_mapper;
    int num_reducer;
};

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

struct block_pointer {
    void *addr;
    int rid;
    int num_kv;
    struct block_pointer *next;
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


#define MAX_CC_WRITE_TASK   ((MAX_CC_MSG_SIZE - sizeof(struct msg_write_t)) / sizeof(struct write_task_t))
#define MAX_CC_READ_TASK    ((MAX_CC_MSG_SIZE - sizeof(struct msg_read_t)) / sizeof(struct read_task_t))
#define MAX_CC_WAIT_MSG     (MAX_NUM_BLOCK * 2)

static struct dma_context_t dma_ctx;

static std::mutex cc_mutex;

/*********** structure for write ***********/
static std::mutex write_mutex;
// number of local mappers
static int num_local_map = 0;
// the number of used blocks
static int used_write_blocks = 0;
// the buffer of all available blocks
static struct block_pointer *avail_write_blocks = NULL;
// the header of the list of each mapper's blocks
static struct block_pointer **write_blocks = NULL;
static std::map<int, int> map_id_to_lmid;

/*********** structure for read ***********/
static std::mutex read_mutex;
// number of read msgs
static int num_read_msg = 0;
// number of write msgs
static int num_wait_msg = 0;
// number of local reducers
static int num_local_reduce = 0;
// total read msg number of a local reducer
static int *lrid_total_read;
// total wait msg number of a local reducer
static int *lrid_total_wait;
// finished read msg number of a local reducer
static int *lrid_finished_read;
// finished wait msg number of a local reducer
static int *lrid_finished_wait;
static std::map<int, int> key_to_lrid;
static std::map<int, int> msg_id_to_lrid;
// all received msg_wait
static struct msg_wait_t *wait_msgs;
// messages of a local reducer
static std::vector<struct msg_wait_t *> *lrid_waits;

/* write util functions */
static struct block_pointer *get_avail_write_block() {
    std::lock_guard<std::mutex> lock(write_mutex);
    if (unlikely(used_write_blocks == MAX_NUM_BLOCK)) {
        printf("Run out of blocks!\n");
        return NULL;
    }
    struct block_pointer *block = &avail_write_blocks[used_write_blocks ++];
    return block;
}

static inline int get_lmid_from_map_id(int map_id) {
    auto it = map_id_to_lmid.find(map_id);
    if (it == map_id_to_lmid.end()) {
        map_id_to_lmid[map_id] = num_local_map;
        return num_local_map ++;
    }
    return it->second;
}

/* read util functions */

/**
 * @brief Get a new message id for a local reducer
 * 
 * @param lrid local reducer id
 * @return int new message id
 */
static inline int get_new_msg_id(int lrid) {
    std::unique_lock<std::mutex> lock(read_mutex);
    int msg_id = num_read_msg ++;
    msg_id_to_lrid[msg_id] = lrid;
    lrid_total_read[lrid] ++;
    return msg_id;
}

static inline int get_lrid_from_key(int key) {
    std::lock_guard<std::mutex> lock(read_mutex);
    auto it = key_to_lrid.find(key);
    if (it == key_to_lrid.end()) {
        int reduce_id = num_local_reduce ++;
        key_to_lrid[key] = reduce_id;
        return reduce_id;
    }
    return it->second;
}

static inline struct msg_wait_t * get_msg_wait_from_lrid(int lrid) {
    std::lock_guard<std::mutex> lock(read_mutex);
    if (lrid_finished_wait[lrid] == lrid_total_wait[lrid]) {
        return NULL;
    }
    return lrid_waits[lrid][lrid_finished_wait[lrid]++];
}


static inline int cc_send_msg(void *msg, int size) {
    doca_error_t result;
    std::lock_guard<std::mutex> lock(cc_mutex);
    struct timespec ts = {
        .tv_nsec = SLEEP_TIME,
    };
    while ((result = doca_comm_channel_ep_sendto(
                dma_ctx.ep, 
                msg, 
                size, 
                DOCA_CC_MSG_FLAG_NONE,
                dma_ctx.peer_addr)) == DOCA_ERROR_AGAIN)
        nanosleep(&ts, &ts);

    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to send msg_write to DPU: %s", doca_get_error_string(result));
        return result;
    }
}

JNIEXPORT jint JNICALL 
Java_org_apache_spark_shuffle_aggr_DPDK_ipc_1init(
                                    JNIEnv * env, 
                                    jclass obj, 
                                    jint shuffle_id, 
                                    jint num_mapper, 
                                    jint num_reducer) {
    
    doca_error_t result;
    int argc = 0;
    char *argv[MAX_DOCA_ARGC] = {0};
    struct timespec ts = {
        .tv_nsec = SLEEP_TIME,
    };
    size_t export_desc_len;
    
    dma_ctx.dma_cfg.mode = DMA_COPY_MODE_HOST;

    /* Register a logger backend */
    result = doca_log_create_standard_backend();
    if (result != DOCA_SUCCESS)
        return EXIT_FAILURE;

    result = doca_argp_init("spark_dma", &dma_ctx.dma_cfg);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to init ARGP resources: %s", doca_get_error_string(result));
        return EXIT_FAILURE;
    }
    register_dma_copy_params();

    // prepare arg parameters
    for (int i = 0; i < MAX_DOCA_ARGC; i ++) {
        argv[i] = (char *)malloc(MAX_DOCA_LEN_ARGV);
    }
    strcpy(argv[argc ++], "./spark_dma");
    strcpy(argv[argc ++], "-p");
    strcpy(argv[argc ++], "01:00.0");

    result = doca_argp_start(argc, argv);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to parse application input: %s", doca_get_error_string(result));
        return EXIT_FAILURE;
    }
    
    result = init_cc(&dma_ctx.dma_cfg, &dma_ctx.ep, &dma_ctx.cc_dev, &dma_ctx.cc_dev_rep);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to Initiate Comm Channel");
        return EXIT_FAILURE;
    }

    /* Open DOCA dma device */
    result = open_dma_device(&dma_ctx.core_state.dev);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to open DMA device");
        return EXIT_FAILURE;
    }

    /* Create DOCA core objects */
    result = create_core_objs(&dma_ctx.core_state, dma_ctx.dma_cfg.mode);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to create DOCA core structures");
        return EXIT_FAILURE;
    }
    
    /* Init DOCA core objects */
    result = init_core_objs(&dma_ctx.core_state, &dma_ctx.dma_cfg);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to initialize DOCA core structures");
        return EXIT_FAILURE;
    }

    // result = host_start_dma_copy(&dma_ctx.dma_cfg, &dma_ctx.core_state, dma_ctx.ep, &dma_ctx.peer_addr);
    
    /* Initialize communication channel */

    result = doca_comm_channel_ep_connect(dma_ctx.ep, SERVER_NAME, &dma_ctx.peer_addr);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to establish a connection with the DPU: %s", doca_get_error_string(result));
        return result;
    }

    while ((result = doca_comm_channel_peer_addr_update_info(dma_ctx.peer_addr)) == DOCA_ERROR_CONNECTION_INPROGRESS)
        nanosleep(&ts, &ts);

    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to validate the connection with the DPU: %s", doca_get_error_string(result));
        return result;
    }

    DOCA_LOG_INFO("Connection to DPU was established successfully");
    const size_t mmap_size = MAX_NUM_BLOCK * MAX_BLOCK_SIZE;

    result = memory_alloc_and_populate(dma_ctx.core_state.write_mmap, mmap_size, DOCA_ACCESS_DPU_READ_WRITE, &dma_ctx.write_buf);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to populate write_mmap");
        return EXIT_FAILURE;
    }
    result = memory_alloc_and_populate(dma_ctx.core_state.read_mmap, mmap_size, DOCA_ACCESS_DPU_READ_WRITE, &dma_ctx.read_buf);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to populate read_mmap");
        return EXIT_FAILURE;
    }

    result = doca_mmap_export_dpu(dma_ctx.core_state.write_mmap, dma_ctx.core_state.dev, (const void **)&dma_ctx.write_desc, &export_desc_len);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to export DOCA mmap: %s", doca_get_error_string(result));
        return result;
    }
    result = doca_mmap_export_dpu(dma_ctx.core_state.read_mmap, dma_ctx.core_state.dev, (const void **)&dma_ctx.read_desc, &export_desc_len);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to export DOCA mmap: %s", doca_get_error_string(result));
        return result;
    }

    dma_ctx.sid = shuffle_id;
    dma_ctx.num_mapper = num_mapper;
    dma_ctx.num_reducer = num_reducer;

    struct msg_init_t msg = {0};
    msg.type = MSG_TYPE_INIT;
    msg.sid = shuffle_id;
    msg.num_mapper = num_mapper;
    msg.num_reducer = num_reducer;
    msg.write_desc = dma_ctx.write_desc;
    msg.read_desc = dma_ctx.read_desc;
    msg.read_addr = dma_ctx.read_buf;
    msg.read_size = mmap_size;

    /* Send the memory map export descriptor to DPU */
    cc_send_msg(&msg, sizeof(struct msg_init_t));
    // result = wait_for_successful_status_msg(ep, peer_addr);
    // if (result != DOCA_SUCCESS)
    // 	return result;
    avail_write_blocks = new struct block_pointer[MAX_NUM_BLOCK];
    for (int i = 0; i < MAX_NUM_BLOCK; i ++) {
        avail_write_blocks[i].addr = dma_ctx.write_buf + i * MAX_BLOCK_SIZE;
        avail_write_blocks[i].rid = 0;
        avail_write_blocks[i].num_kv = 0;
        avail_write_blocks[i].next = NULL;
    }

    /* init write structures */
    num_local_map = 0;
    used_write_blocks = 0;
    map_id_to_lmid.clear();

    write_blocks = new struct block_pointer*[MAX_MAPPER_PER_WORKER * num_reducer];
    for (int i = 0; i < MAX_MAPPER_PER_WORKER * num_reducer; i ++) {
        write_blocks[i] = get_avail_write_block();
        write_blocks[i]->rid = i % num_reducer;
    }

    /* init read structures */
    num_local_reduce = 0;
    num_read_msg = 0;
    num_wait_msg = 0;
    key_to_lrid.clear();
    msg_id_to_lrid.clear();
    lrid_total_read = new int[MAX_REDUCER_PER_WORKER]();
    lrid_total_wait = new int[MAX_REDUCER_PER_WORKER]();
    lrid_finished_read = new int[MAX_REDUCER_PER_WORKER]();
    lrid_finished_wait = new int[MAX_REDUCER_PER_WORKER]();
    wait_msgs = new struct msg_wait_t[MAX_NUM_BLOCK * 2]();
    lrid_waits = new std::vector<struct msg_wait_t *>[MAX_REDUCER_PER_WORKER];
    return 0;
}

JNIEXPORT jint JNICALL Java_org_apache_spark_shuffle_aggr_DPDK_ipc_1clean
  (JNIEnv *env, jclass obj) {

    /* Destroy Comm Channel */
    destroy_cc(dma_ctx.ep, dma_ctx.peer_addr, dma_ctx.cc_dev, dma_ctx.cc_dev_rep);

    /* Destroy core objects */
    destroy_core_objs(&dma_ctx.core_state, &dma_ctx.dma_cfg);

    /* ARGP destroy_resources */
    doca_argp_destroy();

    free(dma_ctx.write_buf);
    free(dma_ctx.read_buf);

    // delete all the resources allocated in ipc_init
    
    num_local_map = 0;
    used_write_blocks = 0;
    map_id_to_lmid.clear();
    
    num_local_reduce = 0;
    num_read_msg = 0;
    num_wait_msg = 0;
    key_to_lrid.clear();
    msg_id_to_lrid.clear();

    delete[] avail_write_blocks;
    delete[] write_blocks;
    delete[] lrid_total_read;
    delete[] lrid_total_wait;
    delete[] lrid_finished_read;
    delete[] lrid_finished_wait;
    delete[] wait_msgs;
    delete[] lrid_waits;
    return 0;
}


JNIEXPORT jint JNICALL Java_org_apache_spark_shuffle_aggr_DPDK_ipc_1write(JNIEnv *env, jclass obj, jint map_id, jint num_partitions, jintArray kv, jint num) {

    int lmid = get_lmid_from_map_id(map_id);

    struct kv_pair_t *kv_pairs = (struct kv_pair_t*) env->GetIntArrayElements(kv, 0);
    int kv_num = num / 2;
    int num_partition = dma_ctx.num_reducer;
    for (int i = 0; i < kv_num; i ++) {
        int partition = kv_pairs[i].key % num_partition;
        int map_block_id = partition + lmid * num_partition;
        struct block_pointer *map_block = write_blocks[map_block_id];
        *(((struct kv_pair_t *)map_block->addr) + map_block->num_kv) = kv_pairs[i];
        map_block->num_kv ++;
        if (unlikely(map_block->num_kv == MAX_KV_PER_BLOCK)) {
            // cc_send_write_msg(map_block->block, map_block->off * sizeof(struct kv_pair_t));
            struct block_pointer *new_block = get_avail_write_block();

            new_block->num_kv = 0;
            new_block->rid = partition;
            new_block->next = map_block;
            write_blocks[map_block_id] = new_block;
        }
    }

    env->ReleaseIntArrayElements(kv, (jint*) kv_pairs, 0);
    return 0;
}

JNIEXPORT jlongArray JNICALL Java_org_apache_spark_shuffle_aggr_DPDK_ipc_1lengths(JNIEnv *env, jclass obj, jint map_id, jint num_partitions) {
    
    int lmid = get_lmid_from_map_id(map_id);

    jlongArray ret = env->NewLongArray(num_partitions);
    char index_path[190];

    FILE* fp = fopen(index_path, "w+");

    jlong *partition_lengths = new jlong[num_partitions];
    
    char buf[MAX_CC_MSG_SIZE];

    struct msg_write_t *msg = (struct msg_write_t *) buf;
    struct write_task_t *tasks = (struct write_task_t *) (buf + sizeof(struct msg_write_t));
    msg->type = MSG_TYPE_WRITE;
    msg->msg_id = lmid;
    msg->mid = map_id;

    // iterate over all reducers
    for (int rid = 0; rid < num_partitions; rid ++) {
        int map_block_id = lmid * num_partitions + rid;
        struct block_pointer *map_block = write_blocks[map_block_id];
        
        partition_lengths[rid] = 0;
        msg->num_task = 0;
        // iterate over the list of all blocks for (mid, rid)
        while (map_block != NULL) {
            partition_lengths[rid] += map_block->num_kv * sizeof(struct kv_pair_t);

            if (likely(map_block->num_kv) != 0) {
                // generate a new task
                struct write_task_t *task = &tasks[msg->num_task];
                task->addr = map_block->addr;
                task->rid = map_block->rid;
                task->num_kv = map_block->num_kv;
                msg->num_task ++;

                if (unlikely(msg->num_task == MAX_CC_WRITE_TASK)) {
                    /* Send a write msg to DPU */
                    cc_send_msg(msg, sizeof(msg_write_t) + msg->num_task * sizeof(write_task_t));
                    msg->num_task = 0;
                }
            }
            map_block = map_block->next;
        }
    }

    if (msg->num_task != 0) {
        /* Send a write msg to DPU */
        cc_send_msg(msg, sizeof(msg_write_t) + msg->num_task * sizeof(write_task_t));
        msg->num_task = 0;
    }
    env->SetLongArrayRegion(ret, 0, num_partitions, partition_lengths);

    delete [] partition_lengths;

    return ret;
}

static
int combineByKey(struct kv_pair_t* kv_arr, int num) {
    int size = 0;
    for (int loop = 1; loop < num; loop++) {
        if (kv_arr[size].key == kv_arr[loop].key) {
            kv_arr[size].val = kv_arr[size].val + kv_arr[loop].val;
        } else {
            size++;
            kv_arr[size].key = kv_arr[loop].key;
            kv_arr[size].val = kv_arr[loop].val;
        }
    }
    return size + 1;
}
static 
int cmpfunc (const void * a, const void * b) {
    return (((struct kv_pair_t *)a)->key - ((struct kv_pair_t *)b)->key);
}


JNIEXPORT jint JNICALL Java_org_apache_spark_shuffle_aggr_DPDK_ipc_1fetch(JNIEnv *env, jclass obj,
                                                                         jint key,
                                                                         jstring   hostName,
                                                                         jintArray shuffle_ids,
                                                                         jintArray map_ids,
                                                                         jintArray reduce_ids) {
    const char* dst_ip = env->GetStringUTFChars(hostName, NULL);
    char buf[MAX_CC_MSG_SIZE];

    int lrid = get_lrid_from_key(key);
    struct msg_read_t *msg = (struct msg_read_t*) buf;
    struct read_task_t *tasks = (struct read_task_t*) (buf + sizeof(struct msg_read_t));
    struct in_addr dst_addr;
    inet_aton(dst_ip, &dst_addr);

    jint  num_task = env->GetArrayLength(shuffle_ids);
    jint  *ss = env->GetIntArrayElements(shuffle_ids, 0);
    jint  *ms = env->GetIntArrayElements(map_ids, 0);
    jint  *rs = env->GetIntArrayElements(reduce_ids, 0);

    msg->type = MSG_TYPE_READ;
    msg->dst  = dst_addr.s_addr;
    msg->num_task = 0;

    for (int i = 0; i < num_task; i ++) {
        // ss[i] == dma_ctx.sid
        tasks[msg->num_task].mid = ms[i];
        tasks[msg->num_task].rid = rs[i];
        msg->num_task ++;
        if (msg->num_task == MAX_CC_READ_TASK) {
            msg->msg_id = get_new_msg_id(lrid);
            cc_send_msg(msg, sizeof(struct msg_read_t) + msg->num_task * sizeof(struct read_task_t));
            msg->num_task = 0;
        }
    }
    if (msg->num_task!= 0) {
        msg->msg_id = get_new_msg_id(lrid);
        cc_send_msg(msg, sizeof(struct msg_read_t) + msg->num_task * sizeof(struct read_task_t));
    }

    env->ReleaseIntArrayElements(shuffle_ids, ss, 0);
    env->ReleaseIntArrayElements(map_ids, ms, 0);
    env->ReleaseIntArrayElements(reduce_ids, rs, 0);
    env->ReleaseStringUTFChars(hostName, dst_ip);

    return 0;
}

JNIEXPORT jbyteArray JNICALL Java_org_apache_spark_shuffle_aggr_DPDK_ipc_1read(JNIEnv *env, jclass obj, jint map_id, jint reduce_id) {
    int lmid = get_lmid_from_map_id(map_id);
    int map_block_id = lmid * dma_ctx.num_reducer + reduce_id;
    struct block_pointer *map_block = write_blocks[map_block_id];
    if (map_block == NULL) {
        return NULL;
    }

    struct kv_pair_t *kv_pairs = (struct kv_pair_t *) (map_block->addr + 8);
    int num_kv = *(size_t *)(map_block->addr) / sizeof(struct kv_pair_t);
    jbyteArray ret = env->NewByteArray(num_kv * sizeof(struct kv_pair_t));
    env->SetByteArrayRegion(ret, 0, num_kv * sizeof(struct kv_pair_t), (const jbyte*)kv_pairs);

    write_blocks[map_block_id] = map_block->next;
    return ret;
}

JNIEXPORT jbyteArray JNICALL Java_org_apache_spark_shuffle_aggr_DPDK_ipc_1wait(JNIEnv *env, jclass obj,
                                                                               jint key) {
    
    struct msg_wait_t *msg;
    doca_error_t result;
    char buf[MAX_CC_MSG_SIZE];
    struct timespec ts = {
        .tv_nsec = SLEEP_TIME,
    };

    int lrid = get_lrid_from_key(key);
    if (lrid_finished_read[lrid] == lrid_total_read[lrid]) {
        return NULL;
    }

    while (true) {
        // try to receive a msg_wait from the unfinished wait msgs
        msg = get_msg_wait_from_lrid(lrid);
        if (msg == NULL) {
            // try to acquire the cc lock
            std::lock_guard<std::mutex> cc_lock(cc_mutex);
            // check the unfinished wait msgs again
            msg = get_msg_wait_from_lrid(lrid);
            if (msg == NULL) {
                struct msg_wait_t *_msg;
                size_t _msg_len;
                // wait for a msg_wait from cc
                while ((result = doca_comm_channel_ep_recvfrom(
                                    dma_ctx.ep, 
                                    (void *)&buf, 
                                    &_msg_len, 
                                    DOCA_CC_MSG_FLAG_NONE,
                                    &dma_ctx.peer_addr)) == DOCA_ERROR_AGAIN) {
                    nanosleep(&ts, &ts);
                }

                if (unlikely(*(int *)buf != MSG_TYPE_WAIT)) {
                    printf("Unknown message type: %d\n", *(int *)buf);
                    return NULL;
                }

                std::lock_guard<std::mutex> read_lock(read_mutex);
                _msg = &wait_msgs[num_wait_msg];
                memcpy(_msg, buf, sizeof(struct msg_wait_t));
                num_wait_msg ++;
                if (unlikely(num_wait_msg == MAX_CC_WAIT_MSG)) {
                    printf("Too many wait messages!\n");
                    return NULL;
                }

                // bring this msg_wait to the right lrid
                int _lrid = msg_id_to_lrid[_msg->msg_id];
                lrid_waits[_lrid].push_back(_msg);
                lrid_total_wait[_lrid] ++;
            }
        }

        // received a msg_wait
        if (msg != NULL) {
            if ((int64_t)msg->size == -1 && (int64_t)msg->addr == -1) {
                // the last msg_wait of a msg_read
                std::lock_guard<std::mutex> read_lock(read_mutex);
                lrid_finished_read[lrid] ++;
            }
            else {
                break;
            }
        }
    }

    jbyteArray ret = env->NewByteArray(msg->size);
    env->SetByteArrayRegion(ret, 0, msg->size, (const jbyte*)msg->addr);
    return ret;
}
