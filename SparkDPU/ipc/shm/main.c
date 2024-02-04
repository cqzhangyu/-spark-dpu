#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <netinet/in.h>

// #define MAX_MAPPER 256
// #define MAX_SHARED 131072
#define MAX_MAPPER 64
#define MAX_SHARED 131072

static inline int
shm_stat(char* shm_name) {
    struct stat ret;
    char full_path[40] = {0};
    sprintf(full_path, "/dev/shm/%s", shm_name);
    stat(full_path, &ret);
    return ret.st_size;
}

static inline 
int shm_delete(char* shm_name) {
    int fd = shm_open(shm_name, O_RDWR, 0777);
    if (fd < 0) {
        return 0;
    }
    int size = shm_stat(shm_name);
    printf("%s %d\n", shm_name, size);
    char* addr = (char *)mmap(NULL, size, PROT_WRITE, MAP_SHARED, fd, 0);
    munmap(addr, size);
    shm_unlink(shm_name);
    close(fd);
    return 0;
}

static inline 
int shm_create(char* shm_name, size_t size) {
    int fd = shm_open(shm_name, O_CREAT | O_RDWR, 0777);
    if (fd < 0) {
        printf("Cannot open %s\n", shm_name);
        return 0;
    }
    int ret = ftruncate(fd, size);
    if (ret < 0) {
        printf("Cannot ftruncate %s\n", shm_name);
    }
    mmap(NULL, size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    printf("Create %s\n", shm_name);
    return 0;
}

int shm_clean() {  
    struct stat ret;
    for (int outer = 0; outer < 96; outer++) {
        for (int inner = 0; inner < 96; inner++) {
            char shm_name[20] = {0};
            sprintf(shm_name,  "shuffle_0_%d_%d", outer, inner);
            shm_delete(shm_name);
        }
    }

    for (int loop = 0; loop < 100; loop++) {
        char shm_name[20] = {0};
        sprintf(shm_name,  "key_%d", loop);
        shm_delete(shm_name);
    }

    for (int loop = 0; loop < 300; loop++) {
        char shm_name[20] = {0};
        sprintf(shm_name,  "myshm_%d", loop);
        shm_delete(shm_name);
    }

    for (int loop = 0; loop < 100; loop++) {
        char shm_name[20] = {0};
        sprintf(shm_name,  "shm_map_%d", loop);
        shm_delete(shm_name);
    }

    shm_delete("shm_req");
    shm_delete("shm_test");
    shm_delete("be_test_0");
    shm_delete("be_test_1");
    shm_delete("be_test_2");
    shm_delete("be_test_3");
}

int shm_init() {
    shm_create("shm_req", 48 * 1024 * 1024);
    for (int loop = 0; loop < 48; loop++) {
        char shm_name[20] = {0};
        sprintf(shm_name, "shm_map_%d", loop);
        shm_create(shm_name, 200 * 1024 * 1024);
    }
    for (int loop = 0; loop < 48; loop++) {
        char shm_name[20] = {0};
        sprintf(shm_name, "key_%d", loop);
        shm_create(shm_name, 200 * 1024 * 1024);
    }

    size_t be_test_len = 8589934592;
    // size_t be_test_len = 17179869184;
    shm_create("be_test_0", be_test_len);
    shm_create("be_test_1", be_test_len);
    shm_create("be_test_2", be_test_len);
    shm_create("be_test_3", be_test_len);
}

int main(int argc, char* argv) {
    shm_clean();
    shm_init();
    return 0;
}
