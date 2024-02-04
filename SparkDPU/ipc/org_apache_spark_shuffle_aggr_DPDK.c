#define _GNU_SOURCE
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

#include "org_apache_spark_shuffle_aggr_DPDK.h"

#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)

#define MAX_MAPPER_PER_WORKER  48
#define MAX_REDUCER_PER_WORKER 48
#define MAX_WORKER             10
#define MAX_TOTAL_MAPPER       (MAX_MAPPER_PER_WORKER  * MAX_WORKER)
#define MAX_TOTAL_REDUCER      (MAX_REDUCER_PER_WORKER * MAX_WORKER)
#define KV_NUM_PER_PACKET      32

#define MAX_PARTITION          (200 * 1024 * 1024)

#define KEY_DIR  1000
#define KEY_PUSH 1001
#define KEY_WAIT 1002
#define KEY_WRITE 1003
int msgid_dir  = 0;
int msgid_push = 0;
int msgid_wait = 0;
int msgid_write = 0;

struct shm_mem_t {
    char* addr;
    long off;
};
struct msg_dir_t {
    long type;
    char path[160];
};
struct msg_push_t {
    long type;
    uint32_t dst;
    long off;
    long len;
};
struct msg_wait_t {
    long type;
    long len;
};
struct msg_write_t {
    long type;
    int sid;
    int mid;
    int num_partitions;
};
struct kv_pair_t {
    int key;
    int val;
};
struct mapout_t {
    char* addr;
    long  base;
    long  len;
    long  cap;
    long  off[KV_NUM_PER_PACKET];
};

struct shm_mem_t shm_req[MAX_MAPPER_PER_WORKER] = {0};

JNIEXPORT jint JNICALL Java_org_apache_spark_shuffle_aggr_DPDK_ipc_1init(JNIEnv * env, jclass obj) {
    int ret = 0;
    msgid_dir  = msgget((key_t)KEY_DIR,  0664|IPC_CREAT);
    if(msgid_dir == -1) {
        exit(-1);
    }
    msgid_push = msgget((key_t)KEY_PUSH, 0664|IPC_CREAT);
    if(msgid_push == -1) {
        exit(-1);
    }
    msgid_wait = msgget((key_t)KEY_WAIT, 0664|IPC_CREAT);
    if(msgid_wait == -1) {
        exit(-1);
    }
    msgid_write = msgget((key_t)KEY_WRITE, 0664|IPC_CREAT);
    if(msgid_write == -1) {
        exit(-1);
    }

    int fd = shm_open("shm_req", O_RDWR, 0777);
    char *base_addr = mmap(NULL, MAX_MAPPER_PER_WORKER * 1024 * 1024, PROT_WRITE, MAP_SHARED, fd, 0);
    for (int loop = 0; loop < MAX_MAPPER_PER_WORKER; loop++) {
        shm_req[loop].addr = (char*) (base_addr + loop * 1024 * 1024);
        shm_req[loop].off  = 0;
    }
    close(fd);
    
    FILE *fp = fopen("/home/zcq/target/java_DPDK.txt", "a");
    static int count = 0;
    fprintf(fp, "init_ipc %d\n", count ++);
    fclose(fp);

    return 0;
}

volatile bool first_workerdir = true;
struct msg_dir_t msg_dir = {0};
JNIEXPORT jint JNICALL Java_org_apache_spark_shuffle_aggr_DPDK_ipc_1workdir(JNIEnv *env, jclass obj,
                                                                            jstring workdir) {
    // if (first_workerdir == true) {
    //     first_workerdir = false;
    // } else {
    //     return 0;
    // }
    const char *inCStr = (*env)->GetStringUTFChars(env, workdir, NULL);
    memset(&msg_dir, 0, sizeof(struct msg_dir_t));
    msg_dir.type = 1;
    strcpy(msg_dir.path, inCStr);
    (*env)->ReleaseStringUTFChars(env, workdir, inCStr);
    msgsnd(msgid_dir, (void*)&msg_dir, sizeof(struct msg_dir_t) - sizeof(long), IPC_NOWAIT);
    return 0;
}

JNIEXPORT jint JNICALL Java_org_apache_spark_shuffle_aggr_DPDK_ipc_1push(JNIEnv *env, jclass obj,
                                                                         jint key,
                                                                         jstring   hostName,
                                                                         jintArray shuffle_ids,
                                                                         jintArray map_ids,
                                                                         jintArray reduce_ids) {
    const char* dst_ip = (*env)->GetStringUTFChars(env, hostName, NULL);
    struct msg_push_t msg = {0};
    struct in_addr dst_addr;
    inet_aton(dst_ip, &dst_addr);

    jsize len = (*env)->GetArrayLength(env, shuffle_ids) * sizeof(int);
    jint  *ss = (*env)->GetIntArrayElements(env, shuffle_ids, 0);
    jint  *ms = (*env)->GetIntArrayElements(env, map_ids, 0);
    jint  *rs = (*env)->GetIntArrayElements(env, reduce_ids, 0);

    msg.type = (long) key;
    msg.dst  = dst_addr.s_addr;
    msg.off  = shm_req[key].off;
    msg.len  = 3 * len;

    memcpy(shm_req[key].addr + shm_req[key].off, ss, len);
    shm_req[key].off += len;
    memcpy(shm_req[key].addr + shm_req[key].off, ms, len);
    shm_req[key].off += len;
    memcpy(shm_req[key].addr + shm_req[key].off, rs, len);
    shm_req[key].off += len;

    (*env)->ReleaseIntArrayElements(env, shuffle_ids, ss, 0);
    (*env)->ReleaseIntArrayElements(env, map_ids, ms, 0);
    (*env)->ReleaseIntArrayElements(env, reduce_ids, rs, 0);
    (*env)->ReleaseStringUTFChars(env, hostName, dst_ip);
    msgsnd(msgid_push, (void*)&msg, sizeof(struct  msg_push_t) - sizeof(long), IPC_NOWAIT);

    return 0;
}

struct mapout_t mapoutput[MAX_MAPPER_PER_WORKER][MAX_TOTAL_REDUCER];

JNIEXPORT jint JNICALL Java_org_apache_spark_shuffle_aggr_DPDK_ipc_1initshm(JNIEnv *env, jclass obj,
                                                                            jint map_id,
                                                                            jint num_partitions) {
    int row = map_id % MAX_MAPPER_PER_WORKER;
    char shm_name[20] = {0};
    sprintf(shm_name, "shm_map_%d", row);
    int fd = shm_open(shm_name, O_RDWR, 0777);
    char* base_addr = (char*) mmap(NULL, MAX_PARTITION, PROT_WRITE, MAP_SHARED, fd, 0);

    int seg_len = ((MAX_PARTITION / num_partitions) / 256 + 1) * 256;
    for (int loop = 0; loop < num_partitions; loop++) {
        memset(&(mapoutput[row][loop]), 0, sizeof(struct mapout_t));
        mapoutput[row][loop].cap  = seg_len;
        mapoutput[row][loop].base = loop * seg_len;
        mapoutput[row][loop].addr = (char *) (base_addr + loop * seg_len);
        mapoutput[row][loop].len  = 0;
        for (int bucket_id = 0; bucket_id < KV_NUM_PER_PACKET; bucket_id++) {
            mapoutput[row][loop].off[bucket_id] = bucket_id * sizeof(struct kv_pair_t);
        }
    }
    close(fd);
    return 0;
}

volatile bool first_init = true;
JNIEXPORT jint JNICALL Java_org_apache_spark_shuffle_aggr_DPDK_ipc_1initall(JNIEnv *env, jclass obj, jint num_partitions) {
    
    FILE *fp = fopen("/home/zcq/target/java_DPDK.txt", "a");
    static int count = 0;
    fprintf(fp, "ipc_initall %d\n", count ++);
    fclose(fp);
    for (int loop = 0; loop < MAX_MAPPER_PER_WORKER; loop++) {
        Java_org_apache_spark_shuffle_aggr_DPDK_ipc_1initshm(env, obj, loop, num_partitions);
    }
    return 0;
}

JNIEXPORT jint JNICALL Java_org_apache_spark_shuffle_aggr_DPDK_ipc_1cleanshm(JNIEnv *env, jclass obj, jint map_id, jint reduce_id) {
    // int row = map_id % MAX_MAPPER_PER_WORKER;
    // if (mapoutput[row][reduce_id].cap != 0) {
    //     munmap(mapoutput[row][reduce_id].addr, mapoutput[row][reduce_id].cap);
    //     char shm_name[20] = {0};
    //     sprintf(shm_name, "shuffle_0_%d_%d", map_id, reduce_id);
    //     shm_unlink(shm_name);
    // }
    return 0;
}

JNIEXPORT jint JNICALL Java_org_apache_spark_shuffle_aggr_DPDK_ipc_1write(JNIEnv *env, jclass obj, jint map_id, jint num_partitions, jintArray kv, jint num) {
    struct kv_pair_t *kv_pairs = (struct kv_pair_t*) (*env)->GetIntArrayElements(env, kv, 0);
    int kv_num = num / 2;

    int row = map_id % MAX_MAPPER_PER_WORKER;
    int bucket_id = 0;
    int partition_id = 0;
    int offset = 0;
    struct mapout_t* tempout = NULL;
    // FILE *fp = fopen("/home/zcq/target/java_DPDK.txt", "w+");
    for (int loop = 0; loop < kv_num; loop++) {
        partition_id = kv_pairs[loop].key % num_partitions;
        // bucket_id    = (kv_pairs[loop].key / num_partitions) % KV_NUM_PER_PACKET;
        bucket_id = 0;
        tempout      = &(mapoutput[row][partition_id]);
        offset       = tempout->off[bucket_id];

        int temp_key = htonl(kv_pairs[loop].key);
        int temp_val = htonl(kv_pairs[loop].val);
        memcpy(tempout->addr + offset, &temp_key, sizeof(int));
        memcpy(tempout->addr + offset + sizeof(int), &temp_val, sizeof(int));

        //  tempout->off[bucket_id] = tempout->off[bucket_id] + KV_NUM_PER_PACKET * sizeof(struct kv_pair_t);
        tempout->off[bucket_id] = tempout->off[bucket_id] + sizeof(struct kv_pair_t);
        if (tempout->off[bucket_id] > tempout->len) {
            tempout->len = tempout->off[bucket_id];
        }
        // fprintf(fp, "key %d, partition %d, bucket %d, offset %x, len %ld\n", kv_pairs[loop].key, partition_id, bucket_id, offset, tempout->len);
    }
    // fclose(fp);

    (*env)->ReleaseIntArrayElements(env, kv, (jint*) kv_pairs, 0);
    return 0;
}

JNIEXPORT jlongArray JNICALL Java_org_apache_spark_shuffle_aggr_DPDK_ipc_1lengths(JNIEnv *env, jclass obj, jint map_id, jint num_partitions) {
    jlongArray ret = (*env)->NewLongArray(env, num_partitions);
    char index_path[190];
    sprintf(index_path, "%s/shuffle_0_%d_0.index", msg_dir.path, map_id);

    FILE* fp = fopen(index_path, "w+");

    jlong fill[MAX_TOTAL_REDUCER];
    int row = map_id % MAX_MAPPER_PER_WORKER;
    for (int loop = 0; loop < num_partitions; loop++) {
        // mapoutput[row][loop].len = mapoutput[row][loop].len & 0xffffffffffffff00;
        fill[loop] = mapoutput[row][loop].len;

        fwrite(&(mapoutput[row][loop].base), sizeof(long), 1, fp);
        fwrite(&(mapoutput[row][loop].len),  sizeof(long), 1, fp);
    }
    (*env)->SetLongArrayRegion(env, ret, 0, num_partitions, fill);

    fclose(fp);

    struct msg_write_t msg = {0};
    msg.type = 1;
    msg.sid = 0;
    msg.mid = map_id;
    msg.num_partitions = num_partitions;

    msgsnd(msgid_write, (void*)&msg, sizeof(struct msg_write_t) - sizeof(long), IPC_NOWAIT);

    // fp = fopen("/home/zcq/target/java_DPDK.txt", "w+");
    // fprintf(fp, "MEEEE Sent msg_write shuffle_id = %d, map_id = %d, num_partitions = %d\n", msg.sid, msg.mid, msg.num_partitions);
    // for (int loop = 0; loop < num_partitions; loop++) {
    //     fprintf(fp, "row %d loop %d base %ld len %ld\n", row, loop, mapoutput[row][loop].base, mapoutput[row][loop].len);
    // }
    // fclose(fp);

    return ret;
}

static
int combineByKey(struct kv_pair_t* kv_arr, int num) {
    int size = 0;
    for (int loop = 1; loop < num; loop++) {
       if (kv_arr[size].key == kv_arr[loop].key) {
           kv_arr[size].val = htonl(ntohl(kv_arr[size].val) + ntohl(kv_arr[loop].val));
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
    return (ntohl(((struct kv_pair_t *)a)->key) - ntohl(((struct kv_pair_t *)b)->key));
}
JNIEXPORT jbyteArray JNICALL Java_org_apache_spark_shuffle_aggr_DPDK_ipc_1read(JNIEnv *env, jclass obj, jint map_id, jint reduce_id) {
    int row = map_id % MAX_MAPPER_PER_WORKER;

    qsort(mapoutput[row][reduce_id].addr, mapoutput[row][reduce_id].len / sizeof(struct kv_pair_t), sizeof(struct kv_pair_t), cmpfunc);
    int size = combineByKey((struct kv_pair_t*) mapoutput[row][reduce_id].addr, mapoutput[row][reduce_id].len / sizeof(struct kv_pair_t));

    jbyteArray ret = (*env)->NewByteArray(env, size * sizeof(struct kv_pair_t));
    (*env)->SetByteArrayRegion(env, ret, 0, size * sizeof(struct kv_pair_t), mapoutput[row][reduce_id].addr);
    return ret;
}

JNIEXPORT jbyteArray JNICALL Java_org_apache_spark_shuffle_aggr_DPDK_ipc_1wait(JNIEnv *env, jclass obj,
                                                                               jint key) {
    struct msg_wait_t msg = {0};
    char shm_name[20] = {0};
    sprintf(shm_name, "key_%d", key);
    int fd = shm_open(shm_name, O_RDWR, 0777);
    msgrcv(msgid_wait, (void*)&msg, sizeof(struct msg_wait_t) - sizeof(long), (long) key, 0);

    char* addr = (char*) mmap(NULL, msg.len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    // printf("key = %d, len = %ld\n", key, msg.len);

    qsort(addr, msg.len / sizeof(struct kv_pair_t), sizeof(struct kv_pair_t), cmpfunc);
    int size = combineByKey((struct kv_pair_t*) addr, msg.len / sizeof(struct kv_pair_t));

    // printf("key = %d, size= %d\n", key, size);

    jbyteArray ret = (*env)->NewByteArray(env, size * sizeof(struct kv_pair_t));
    (*env)->SetByteArrayRegion(env, ret, 0, size * sizeof(struct kv_pair_t), addr);
    close(fd);
    return ret;
}
