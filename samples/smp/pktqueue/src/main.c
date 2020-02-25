/*
 * Copyright (c) 2020 Synopsys, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "pktqueue.h"
#include "main.h"

static K_THREAD_STACK_ARRAY_DEFINE(tstack, THREADS_NUM*QUEUE_NUM, STACK_SIZE);
static K_THREAD_STACK_ARRAY_DEFINE(qstack, QUEUE_NUM, STACK_SIZE);

K_MUTEX_DEFINE(fetch_queue_mtx);

/* Function for initializing "sender" packet header queue */
void init_datagram_queue(struct phdr_desc_queue * queue, int queue_num)
{

	int i,j;
	queue->head = &descriptors[queue_num][0];

	for (i = 0; i < SIZE_OF_QUEUE; i++){
		queue->tail = &descriptors[queue_num][i];
		descriptors[queue_num][i].ptr = (u8_t *)&headers[i];
		/* Fill packet header with random values */
		for (j = 0; j < SIZE_OF_HEADER; j++){
			/* leave crc field zeroed */
			if (j < 10 || j > 11)
				descriptors[queue_num][i].ptr[j] = sys_rand32_get();
			else
				descriptors[queue_num][i].ptr[j] = 0;
   		}
		/* Compute crc for further comparisson */
		u16_t crc;
		crc = crc16(descriptors[queue_num][i].ptr, SIZE_OF_HEADER, POLYNOMIAL,0,0);

		/* Save crc value in header[10-11] field */
		descriptors[queue_num][i].ptr[10] = (u8_t)(crc >> 8);
		descriptors[queue_num][i].ptr[11] = (u8_t)(crc);
		atomic_inc(&queue->count);
		descriptors[queue_num][i].next = &descriptors[queue_num][i+1];
	}
}


/* Thread takes packet from "sender" queue and puts it to "receiver" queue.
 * Each queue can be accessed only by one thread in a time. */
void test_thread(void *arg1, void *arg2, void *arg3)
{
	struct phdr_desc_queue * sender_queue = (struct phdr_desc_queue *)arg1;
	struct phdr_desc_queue * receiver_queue = (struct phdr_desc_queue *)arg2;
	struct phdr_desc * qin_ptr = NULL;
	int * queue_num = (int *)arg3;

	u16_t crc, crc_orig;

	qin_ptr = phdr_desc_dequeue(sender_queue,&sender_queue_mtx[*queue_num]);
	while(qin_ptr != NULL) {
		/* Store original crc value from header */
		crc_orig  =  qin_ptr->ptr[10] << 8;
		crc_orig |=   qin_ptr->ptr[11];
		/* Crc field should be zero before crc calculation */
		qin_ptr->ptr[10] = 0;
		qin_ptr->ptr[11] = 0;
		crc = crc16(qin_ptr->ptr, SIZE_OF_HEADER, POLYNOMIAL,0,0);

		/* Compare computed crc with crc from phdr_desc->crc */
		if (crc == crc_orig) {
			phdr_desc_enqueue(receiver_queue, qin_ptr, &receiver_queue_mtx[*queue_num]);
		}
		/* Take next element from "sender queue" */
		qin_ptr = phdr_desc_dequeue(sender_queue,&sender_queue_mtx[*queue_num]);
	}
}

/* Thread that processes one pair of sender/receiver queue */
void queue_thread(void *arg1, void *arg2, void *arg3)
{

	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	int queue_num;
	/* Fetching one queue */
	k_mutex_lock(&fetch_queue_mtx,K_FOREVER);
	queue_num = current_queue;
	atomic_inc(&current_queue);
	k_mutex_unlock(&fetch_queue_mtx);

	for (int i = 0; i < THREADS_NUM; i++) {
		k_thread_create(&tthread[i+THREADS_NUM*queue_num], tstack[i+THREADS_NUM*queue_num], STACK_SIZE,
		      (k_thread_entry_t)test_thread,
			    (void *)&sender[queue_num], (void *)&receiver[queue_num], (void*)&queue_num,
			    K_PRIO_COOP(10), 0, K_NO_WAIT);
	}

	/* Wait until sender queue is not empty */
	while (sender[queue_num].count != 0)
		k_sleep(K_MSEC(1));

	/* Decrementing queue counter */
	atomic_dec(&queues_remain);
}

void main(void)
{
	u32_t start_time, stop_time, cycles_spent, nanoseconds_spent;
	int i;

	printk("Simulating IP header validation on multiple cores.\n");
	printk("Each of %d parallel queues is processed by %d threads"
		" and contain %d packet headers.\n",
		QUEUE_NUM, THREADS_NUM, SIZE_OF_QUEUE);
	printk("Bytes in packet header: %d\n\n", SIZE_OF_HEADER);

	/* initializing "sender" queue */
	for (i = 0; i<QUEUE_NUM; i++){
		init_datagram_queue(&sender[i],i);
		k_mutex_init(&sender_queue_mtx[i]);
		k_mutex_init(&receiver_queue_mtx[i]);
	}
	k_sleep(K_MSEC(10));

	/* Capture initial time stamp */
	start_time = k_cycle_get_32();

	for (i = 0; i < QUEUE_NUM; i++) {
		k_thread_create(&qthread[i], qstack[i], STACK_SIZE,
			       (k_thread_entry_t)queue_thread,
			       (void *)&sender[i], (void *)&receiver[i], (void *)&i,
			       K_PRIO_COOP(10), 0, K_NO_WAIT);
	}

	/* Wait until all queues are not processed */
	while(queues_remain > 0)
		k_sleep(K_MSEC(1));

	/* Capture final time stamp */
	stop_time = k_cycle_get_32();
	cycles_spent = stop_time - start_time;
	nanoseconds_spent = (u32_t)k_cyc_to_ns_floor64(cycles_spent);

	printk("All %d packet headers were processed in %d msec\n",
	SIZE_OF_QUEUE*QUEUE_NUM, nanoseconds_spent / 1000 / 1000);

	k_sleep(K_MSEC(10));
}
