/*
 * Copyright (c) 2020 Synopsys, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <stdio.h>
#include <sys/crc.h>
#include <random/rand32.h>


/* Amount of parallel processed sender/receiver queues of packet headers */
#define QUEUE_NUM 4

/* Amount of execution threads per pair of queues*/
#define THREADS_NUM  1

/* Amount of packet headers in a queue */
#define SIZE_OF_QUEUE 5000

/* Size of packet header (in bytes) */
#define SIZE_OF_HEADER 24


/* Crc16 polynomial */
#define POLYNOMIAL 0x8005

#ifdef CONFIG_SMP
#define CORES_NUM	CONFIG_MP_NUM_CPUS
#else
#define CORES_NUM	1
#endif

#define STACK_SIZE	2048

static struct k_thread tthread[THREADS_NUM*QUEUE_NUM];
static struct k_thread qthread[QUEUE_NUM];

/* Each queue has its own mutex */
struct k_mutex sender_queue_mtx[QUEUE_NUM];
struct k_mutex receiver_queue_mtx[QUEUE_NUM];

/* Variable which indicates the amount of processed queues */
atomic_t queues_remain = QUEUE_NUM;
/* Variable to define current queue in thread */
atomic_t current_queue = 0;

/* Array of packet header descriptors */
struct phdr_desc descriptors[QUEUE_NUM][SIZE_OF_QUEUE];

/* Arrays of receiver and sender queues */
struct phdr_desc_queue sender[QUEUE_NUM], receiver[QUEUE_NUM];

/* Array of packet headers */
u8_t headers[SIZE_OF_QUEUE][SIZE_OF_HEADER];
