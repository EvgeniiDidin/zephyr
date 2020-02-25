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
#define QUEUE_NUM 2

/* Amount of execution threads per pair of queues*/
#define THREADS_NUM  4

/* Amount of packet headers in a queue */
#define SIZE_OF_QUEUE 5000

/* Size of packet header (in bytes) */
#define SIZE_OF_HEADER 24

/* CRC16 polynomial */
#define POLYNOMIAL 0x8005

#define STACK_SIZE	2048


