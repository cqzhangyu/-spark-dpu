/*
 * Copyright (c) 2022-2023 NVIDIA CORPORATION & AFFILIATES, ALL RIGHTS RESERVED.
 *
 * This software product is a proprietary product of NVIDIA CORPORATION &
 * AFFILIATES (the "Company") and all right, title, and interest in and to the
 * software product, including all associated intellectual property rights, are
 * and shall remain exclusively with the Company.
 *
 * This software product is governed by the End User License Agreement
 * provided with the software product.
 *
 */


#include <stdint.h>
#include <string.h>
#include <time.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <signal.h>

#include <doca_buf.h>
#include <doca_buf_inventory.h>
#include <doca_ctx.h>
#include <doca_dev.h>
#include <doca_dma.h>
#include <doca_mmap.h>

#include "../common/dma_copy_core.h"
#include "../common/common.h"
#include "../common/utils.h"
#include "../common/pack.h"
#include "../common/log.h"
#include "../common/types.h"


volatile int force_quit = 0;

void sig_handler(int sig) {
    force_quit = 1;
}

/*
 * DMA copy application main function
 *
 * @argc [in]: command line arguments size
 * @argv [in]: array of command line arguments
 * @return: EXIT_SUCCESS on success and EXIT_FAILURE otherwise
 */
int
main(int argc, char **argv)
{
    doca_error_t result;
    struct dma_copy_cfg dma_cfg = {0};
    struct core_state core_state = {0};
    struct doca_comm_channel_ep_t *ep;
    struct doca_comm_channel_addr_t *peer_addr = NULL;
    struct doca_dev *cc_dev = NULL;
    struct doca_dev_rep *cc_dev_rep = NULL;
    int exit_status = EXIT_SUCCESS;

    dma_cfg.mode = DMA_COPY_MODE_DPU;
    set_log_level(LOG_LEVEL_DEBUG);

    // handle sigint and sigterm
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    /* Register a logger backend */
    result = doca_log_create_standard_backend();
    if (result != DOCA_SUCCESS)
        return EXIT_FAILURE;

    result = doca_argp_init("doca_dma_copy", &dma_cfg);
    if (result != DOCA_SUCCESS) {
        LOG_ERRO("Failed to init ARGP resources: %s", doca_get_error_string(result));
        return EXIT_FAILURE;
    }
    LOG_DEBUG("Successfully initialized ARGP resources\n");
    register_dma_copy_params();
    result = doca_argp_start(argc, argv);
    if (result != DOCA_SUCCESS) {
        LOG_ERRO("Failed to parse application input: %s", doca_get_error_string(result));
        return EXIT_FAILURE;
    }
    LOG_DEBUG("Successfully parsed application input\n");

    result = init_cc(&dma_cfg, &ep, &cc_dev, &cc_dev_rep);
    if (result != DOCA_SUCCESS) {
        LOG_ERRO("Failed to Initiate Comm Channel");
        return EXIT_FAILURE;
    }
    LOG_DEBUG("Successfully initiated Comm Channel\n");

    /* Open DOCA dma device */
    result = open_dma_device(&core_state.dev);
    if (result != DOCA_SUCCESS) {
        LOG_ERRO("Failed to open DMA device");
        exit_status = EXIT_FAILURE;
        goto destroy_resources;
    }
    LOG_DEBUG("Successfully opened DMA device\n");

    /* Create DOCA core objects */
    result = create_core_objs(&core_state, dma_cfg.mode);
    if (result != DOCA_SUCCESS) {
        LOG_ERRO("Failed to create DOCA core structures");
        exit_status = EXIT_FAILURE;
        goto destroy_resources;
    }
    LOG_DEBUG("Successfully created DOCA core structures\n");

    /* Init DOCA core objects */
    result = init_core_objs(&core_state, &dma_cfg);
    if (result != DOCA_SUCCESS) {
        LOG_ERRO("Failed to initialize DOCA core structures");
        exit_status = EXIT_FAILURE;
        goto destroy_resources;
    }
    LOG_DEBUG("Successfully initialized DOCA core structures\n");

    // start listening
    LOG_INFO("Starting cc server\n");

    result = doca_comm_channel_ep_listen(ep, SERVER_NAME);
    if (result != DOCA_SUCCESS) {
        LOG_ERRO("Comm Channel endpoint couldn't start listening: %s", doca_get_error_string(result));
        return result;
    }

    if (result != DOCA_SUCCESS)
        exit_status = EXIT_FAILURE;

    char buf[CC_MAX_MSG_SIZE];
    struct timespec ts = {
        .tv_nsec = SLEEP_TIME,
    };
        
    /* Wait until Host negotiation message will arrive */
    size_t msg_len = CC_MAX_MSG_SIZE;
    while (!force_quit && (result = doca_comm_channel_ep_recvfrom(ep, buf, &msg_len,
                               DOCA_CC_MSG_FLAG_NONE, &peer_addr)) == DOCA_ERROR_AGAIN) {
        nanosleep(&ts, &ts);
        msg_len = CC_MAX_MSG_SIZE;
    }
    if (result != DOCA_SUCCESS) {
        LOG_ERRO("Cannot receive msg_init\n");
        return result;
    }

    if (msg_len != sizeof(struct msg_init_t)) {
        LOG_ERRO("msg size mismatch with msg_init!\n");
        return 1;
    }
    struct msg_init_t *msg_init = (struct msg_init_t *)buf;
    if (msg_init->type != MSG_TYPE_INIT) {
        LOG_ERRO("msg type mismatch with msg_init. Expected : %d, got %d\n", MSG_TYPE_INIT, msg_init->type);
        return 1;
    }
    LOG_INFO("received msg_init : type = %d, sid = %d, num_mapper = %d, num_reducer = %d\n", msg_init->type, msg_init->sid, msg_init->num_mapper, msg_init->num_reducer);
    void *read_addr = msg_init->read_addr;

    while (!force_quit) {
        msg_len = CC_MAX_MSG_SIZE;
        while (!force_quit && (result = doca_comm_channel_ep_recvfrom(ep, buf, &msg_len,
                                DOCA_CC_MSG_FLAG_NONE, &peer_addr)) == DOCA_ERROR_AGAIN) {
            nanosleep(&ts, &ts);
            msg_len = CC_MAX_MSG_SIZE;
        }

        int msg_type = *(int *)buf;
        if (msg_type == MSG_TYPE_WRITE) {
            struct msg_write_t *msg_write = (struct msg_write_t *)buf;
            LOG_DEBUG("received msg_write : msg_id = %d, mid = %d, num_task = %d\n", msg_write->msg_id, msg_write->mid, msg_write->num_task);
        }
        else if (msg_type == MSG_TYPE_READ) {
            struct msg_read_t *msg_read = (struct msg_read_t *)buf;
            LOG_DEBUG("received msg_read : dst=%08x msg_id = %d, num_task = %d\n", msg_read->dst, msg_read->msg_id, msg_read->num_task);

            // send back a wait msg
            int msg_id = msg_read->msg_id;
            struct msg_wait_t *msg_wait = (struct msg_wait_t *)buf;
            msg_wait->type = MSG_TYPE_WAIT;
            msg_wait->msg_id = msg_id;
            msg_wait->addr = read_addr;
            msg_wait->size = 24;
            while (!force_quit && (result = doca_comm_channel_ep_sendto(
                ep, 
                msg_wait, 
                sizeof(struct msg_wait_t), 
                DOCA_CC_MSG_FLAG_NONE,
                peer_addr)) == DOCA_ERROR_AGAIN)
                nanosleep(&ts, &ts);

            if (result != DOCA_SUCCESS) {
                LOG_ERRO("Failed to send msg_write to DPU: %s", doca_get_error_string(result));
                return result;
            }
            LOG_DEBUG("sent msg_wait : msg_id = %d, size = %d\n", msg_wait->msg_id, msg_wait->size);

            msg_wait->type = MSG_TYPE_WAIT;
            msg_wait->msg_id = msg_id;
            msg_wait->addr = (void *)-1;
            msg_wait->size = -1;
            while (!force_quit && (result = doca_comm_channel_ep_sendto(
                ep, 
                msg_wait, 
                sizeof(struct msg_wait_t), 
                DOCA_CC_MSG_FLAG_NONE,
                peer_addr)) == DOCA_ERROR_AGAIN)
                nanosleep(&ts, &ts);

            if (result != DOCA_SUCCESS) {
                LOG_ERRO("Failed to send msg_write to DPU: %s", doca_get_error_string(result));
                return result;
            }
            LOG_DEBUG("sent msg_wait : msg_id = %d, size = %d\n", msg_wait->msg_id, msg_wait->size);
        }
        else {
            LOG_ERRO("Unknown message type: %d\n", msg_type);
            break;
        }
    }

destroy_resources:

    /* Destroy Comm Channel */
    destroy_cc(ep, peer_addr, cc_dev, cc_dev_rep);

    /* Destroy core objects */
    destroy_core_objs(&core_state, &dma_cfg);

    /* ARGP destroy_resources */
    doca_argp_destroy();

    return exit_status;
}
