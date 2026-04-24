// File:	my_pthread_t.h
// Author:	Jiayi Yang
// Date:	April 2025

#ifndef MY_PTHREAD_T_H
#define MY_PTHREAD_T_H

#define _GNU_SOURCE

/* To use real pthread Library in Benchmark, you have to comment the USE_MY_PTHREAD macro */
#define USE_MY_PTHREAD 1

/* include lib header files that you need here: */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <signal.h>
#include <semaphore.h>
#include <stdbool.h>
#include <sys/time.h>

/* defile necessary MACRO here, for example, thread upper bound,
   stack size, priority queue levels, time quantum, etc. */


typedef uint my_pthread_t;

typedef struct my_pthread_list my_pthread_list_t;

// Thread Status
typedef enum threadStatus {
	NOT_STARTED = 0,
	RUNNING,
	SUSPENDED,
	TERMINATED,
	FINISHED,
} threadStatus;

typedef struct threadControlBlock {
	/* add important states in a thread control block */
	// thread Id
	// thread status
	// thread context
	// thread stack
	// thread priority
	// And more ...
	my_pthread_t threadId;
	threadStatus status;
	ucontext_t context;
	char stack[8192];
	int priority;
	void* returnValue;
	int executedTime;
	void* (*startRoutine)(void*);
	void* startArg;
	// YOUR CODE HERE
} tcb;

// Feel free to add your own auxiliary data structures (linked list or queue etc...)
typedef struct node_t {
	tcb* thread;
	struct node_t* next;
} node;

typedef struct my_pthread_list {
	node* head;
	node* tail;
	int size;
} my_pthread_list_t;

/* mutex struct definition */
typedef struct my_pthread_mutex_t {
	/* add something here */
	struct __lock_t {
		int lock;
		my_pthread_t owner;
		unsigned int nusers;
		int kind;
		my_pthread_list_t list;
	} lock;
	char size[40];
	long int align;
	// YOUR CODE HERE
} my_pthread_mutex_t;

/* define your data structures here: */
// Below are some examples, feel free to modify and define your own structures:

// Schedule Policy
typedef enum schedPolicy {
	POLICY_RR = 0,
	POLICY_MLFQ,
	POLICY_PSJF
} schedPolicy;

// YOUR CODE HERE
typedef struct my_pthread_queue {
	node* head;
	node* tail;
	int size;
} my_pthread_queue_t;

/* Function Declarations: */

/* create a new thread */
int my_pthread_create(my_pthread_t* thread, pthread_attr_t* attr, void* (*function)(void*), void* arg);

/* give CPU pocession to other user level threads voluntarily */
int my_pthread_yield();

/* terminate a thread */
void my_pthread_exit(void* value_ptr);

/* wait for thread termination */
int my_pthread_join(my_pthread_t thread, void** value_ptr);

/* initial the mutex lock */
int my_pthread_mutex_init(my_pthread_mutex_t* mutex, const pthread_mutexattr_t* mutexattr);

/* aquire the mutex lock */
int my_pthread_mutex_lock(my_pthread_mutex_t* mutex);

/* release the mutex lock */
int my_pthread_mutex_unlock(my_pthread_mutex_t* mutex);

/* destroy the mutex */
int my_pthread_mutex_destroy(my_pthread_mutex_t* mutex);

static void schedule();

static void sched_stcf();

static void sched_mlfq();

void init();

void timerHander(int signo);

void setTimer();

void blockTimer();

void unblockTimer();

void listPush(my_pthread_list_t* list, tcb* thread);

tcb* listPop(my_pthread_list_t* list);

bool listIsEmpty(my_pthread_list_t* list);

void queuePush(my_pthread_queue_t* queue, tcb* thread);

tcb* queuePop(my_pthread_queue_t* queue);

bool queueIsEmpty(my_pthread_queue_t* queue);

void queueClear(my_pthread_queue_t* queue);

tcb* queueFindByThreadId(my_pthread_queue_t* queue, my_pthread_t threadId);

tcb* queueRemoveByThreadId(my_pthread_queue_t* queue, my_pthread_t threadId);

bool queueContainsThreadId(my_pthread_queue_t* queue, my_pthread_t threadId);

tcb* queueFindMinExecutedTime(my_pthread_queue_t* queue);

tcb* queueFindMinPriority(my_pthread_queue_t* queue);

#ifdef USE_MY_PTHREAD
#define pthread_t my_pthread_t
#define pthread_mutex_t my_pthread_mutex_t
#define pthread_create my_pthread_create
#define pthread_exit my_pthread_exit
#define pthread_join my_pthread_join
#define pthread_mutex_init my_pthread_mutex_init
#define pthread_mutex_lock my_pthread_mutex_lock
#define pthread_mutex_unlock my_pthread_mutex_unlock
#define pthread_mutex_destroy my_pthread_mutex_destroy
#endif

#endif
