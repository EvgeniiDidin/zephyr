/*
 * Copyright (c) 2020 Synopsys, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <stdio.h>
#include <sys/crc.h>
#include <random/rand32.h>

/* Amount of execution threads to create and run */
#define THREADS_NUM	16

/* Amount of package headers in a queue */
#define SIZE_OF_QUEUE 10000

/* ASize of package header (in bytes) */
#define SIZE_OF_HEADER 24


/* Crc16 polynomial */
#define POLYNOMIAL 0x8005

#ifdef CONFIG_SMP
#define CORES_NUM	CONFIG_MP_NUM_CPUS
#else
#define CORES_NUM	1
#endif

#define STACK_SIZE	2048
static K_THREAD_STACK_ARRAY_DEFINE(tstack, THREADS_NUM, STACK_SIZE);
static struct k_thread tthread[THREADS_NUM];

K_MUTEX_DEFINE(sender_queue_mtx);
K_MUTEX_DEFINE(receiver_queue_mtx);

struct phdr_descriptor {
   struct phdr_descriptor  *next;    /* Next datagram descriptor in respective queue */
   u8_t *ptr;                           /* Pointer to header */
};

struct descriptor_queue {
    struct phdr_descriptor  *head;   /* remove package headers from here */
    struct phdr_descriptor  *tail;   /* add package headers here */
    u32_t                    cnt;   /* maintain the count of elements in the queue*/
};

/* Array of package header descriptors */
struct phdr_descriptor descriptors[SIZE_OF_QUEUE];
struct descriptor_queue sender, receiver;

/* Array of package headers */
u8_t data[SIZE_OF_QUEUE][SIZE_OF_HEADER];

/* Function for initializing "sender" package header queue */
void init_datagram_queue(struct descriptor_queue* queue) {

  int i,j;

  queue->head = &descriptors[0];
  queue->cnt = 0;

  for (i = 0; i < SIZE_OF_QUEUE; i++){
    queue->tail = &descriptors[i];
    descriptors[i].ptr = (u8_t *)&data[i];
    /* Fill package header with random values */
    for (j = 0; j < SIZE_OF_HEADER; j++){
      /* leave crc field zeroed */
      if (j < 10 || j > 11)
        descriptors[i].ptr[j] = sys_rand32_get();
      else
        descriptors[i].ptr[j] = 0;
    }
    /* Compute crc for further comparisson */
    u16_t crc;
    crc = crc16(descriptors[i].ptr, SIZE_OF_HEADER, POLYNOMIAL,0,0);

    /* Save crc value in header[10-11] field */
    descriptors[i].ptr[10] = (u8_t)(crc >> 8);
    descriptors[i].ptr[11] = (u8_t)(crc);
    descriptors[i].next = &descriptors[i+1];

    queue->cnt++;
  }
}

/* Put a pkg header in a queue defined in argument */
void desc_enqueue(struct descriptor_queue* queue, struct phdr_descriptor* desc,
                  struct k_mutex* mutex ) {

  /* Locking queue */
  k_mutex_lock(mutex,K_FOREVER);

  if (queue->cnt == 0) {
    queue->head = queue->tail = desc;
  } else {
    queue->tail->next = desc;
    queue->tail = desc;
  }
  /* do we need atomic here ? */
  queue->cnt++;

  /* Unlocking queue */
  k_mutex_unlock(mutex);

  desc->next = NULL;
}

/* Take a package header from queue defined in argument */
struct phdr_descriptor* desc_dequeue(struct descriptor_queue* queue, struct k_mutex* mutex) {
                                        struct phdr_descriptor* return_ptr = NULL;
  /* Locking queue */
  k_mutex_lock(mutex,K_FOREVER);

  if (queue->cnt != 0) {
      /* do we need atomic here ? */
    queue->cnt--;
    return_ptr = queue->head;
    queue->head = queue->head->next;
  }

  /* Unlocking queue */
  k_mutex_unlock(mutex);

  return return_ptr;
}

/* Thread tries to take package from "sender" queue and puts it to "receiver" queue */
void test_thread(void *arg1, void *arg2, void *arg3)
{
	struct descriptor_queue * sender_queue = (struct descriptor_queue *)arg1;
	struct descriptor_queue * receiver_queue = (struct descriptor_queue *)arg2;
  struct phdr_descriptor * qin_ptr = NULL;

  ARG_UNUSED(arg3);

  u16_t crc, crc_orig;

  qin_ptr = desc_dequeue(sender_queue,&sender_queue_mtx);
  while(qin_ptr != NULL) {
    /* Store original crc value from header */
    crc_orig  =  qin_ptr->ptr[10] << 8;
    crc_orig |=   qin_ptr->ptr[11];
    /* Crc field should be zero before crc calculation */
    qin_ptr->ptr[10] = 0;
    qin_ptr->ptr[11] = 0;
    crc = crc16(qin_ptr->ptr, SIZE_OF_HEADER, POLYNOMIAL,0,0);

    /* Compare computed crc with crc from phdr_descriptor->crc */
    if (crc == crc_orig)
      desc_enqueue(receiver_queue, qin_ptr, &receiver_queue_mtx);

    /* Take next element from "sender queue" */
    qin_ptr = desc_dequeue(sender_queue,&sender_queue_mtx);
  }

}

void main(void)
{
	u32_t start_time, stop_time, cycles_spent, nanoseconds_spent;
	int i;
  printk("Simulating IP header validation on multiple cores.\n");
  printk("Amount of package headers to process: %d\n",SIZE_OF_QUEUE);
  printk("Bytes in package header: %d\n", SIZE_OF_HEADER);

  /* initializing "sender" queue */
  init_datagram_queue(&sender);

	/* Capture initial time stamp */
	start_time = k_cycle_get_32();

	for (i = 0; i < THREADS_NUM; i++) {
		k_thread_create(&tthread[i], tstack[i], STACK_SIZE,
			       (k_thread_entry_t)test_thread,
			       (void *)&sender, (void *)&receiver, NULL,
			       K_PRIO_COOP(10), 0, K_NO_WAIT);
	}

	/* Wait until sender queue is not empty */
	while (sender.cnt != 0){
      printk("sender.cnt = %d\n",sender.cnt);
    		k_sleep(K_MSEC(1));
  }

	/* Capture final time stamp */
	stop_time = k_cycle_get_32();

	cycles_spent = stop_time - start_time;

	nanoseconds_spent = (u32_t)k_cyc_to_ns_floor64(cycles_spent);

  printk("Packages processed: %d\n", receiver.cnt);
	printk("All %d threads executed by %d cores in %d msec\n", THREADS_NUM,
	       CORES_NUM, nanoseconds_spent / 1000 / 1000);
  k_sleep(K_MSEC(100));

}
