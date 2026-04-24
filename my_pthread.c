// File:	my_pthread.c
// Author:	Jiayi Yang
// Date:	April 2025

#include "my_pthread_t.h"

// INITAILIZE ALL YOUR VARIABLES HERE
// YOUR CODE HERE
my_pthread_queue_t readyQueue;
my_pthread_queue_t blockQueue;
my_pthread_queue_t mutexQueue;
my_pthread_queue_t finishedQueue;
my_pthread_t threadIdCounter = 0;
ucontext_t schedulerContext;
char schedulerStack[8192];
tcb* currentThread;
sigset_t timerMask;
volatile sig_atomic_t timerFlag = 0;
volatile sig_atomic_t preemptPending = 0;
static int inited = 0;
const int MLFQSize = 6;
const int timeQuantum = 5000;

void threadBootstrap();
void maybePreempt();

void init() {
	if (inited) return;
	inited = 1;
	queueClear(&readyQueue);
	queueClear(&blockQueue);
	queueClear(&mutexQueue);
	queueClear(&finishedQueue);
	currentThread = (tcb*)malloc(sizeof(tcb));
	currentThread->threadId = 0;
	currentThread->status = RUNNING;
	currentThread->executedTime = 0;
	currentThread->returnValue = NULL;
	currentThread->priority = 0;
	getcontext(&schedulerContext);
	schedulerContext.uc_stack.ss_sp = schedulerStack;
	schedulerContext.uc_stack.ss_size = sizeof(schedulerStack);
	schedulerContext.uc_link = NULL;
	makecontext(&schedulerContext, (void (*)(void))sched_stcf, 0);
	setTimer(timeQuantum);
}

/* create a new thread */
int my_pthread_create(my_pthread_t* thread, pthread_attr_t* attr,
	void* (*function)(void*), void* arg) {
	// Create Thread Control Block
	// Create and initialize the context of this thread
	// Allocate space of stack for this thread to run
	// After everything is all set, push this thread into run queue
	init();
	tcb* newThread = (tcb*)malloc(sizeof(tcb));
	newThread->threadId = ++threadIdCounter;
	newThread->status = NOT_STARTED;
	newThread->executedTime = 0;
	newThread->returnValue = NULL;
	newThread->priority = 0;
	newThread->startRoutine = function;
	newThread->startArg = arg;
	getcontext(&newThread->context);
	newThread->context.uc_stack.ss_sp = newThread->stack;
	newThread->context.uc_stack.ss_size = sizeof(newThread->stack);
	newThread->context.uc_link = NULL;
	makecontext(&newThread->context, threadBootstrap, 0);
	queuePush(&readyQueue, newThread);
	*thread = newThread->threadId;
	// YOUR CODE HERE
	return 0;
};

/* give CPU pocession to other user level threads voluntarily */
int my_pthread_yield() {
	// Change thread state from Running to Ready
	// Save context of this thread to its thread control block
	// Switch from thread context to scheduler context
	init();
	if (currentThread == NULL) return -1;
	preemptPending = 0;
	currentThread->status = SUSPENDED;
	currentThread->executedTime++;
	queuePush(&readyQueue, currentThread);
	swapcontext(&currentThread->context, &schedulerContext);
	// YOUR CODE HERE
	return 0;
};

/* terminate a thread */
void my_pthread_exit(void* value_ptr) {
	// Deallocated any dynamic memory created when starting this thread
	init();
	currentThread->status = TERMINATED;
	currentThread->returnValue = value_ptr;
	queuePush(&finishedQueue, currentThread);
	swapcontext(&currentThread->context, &schedulerContext);
	// YOUR CODE HERE
};


/* wait for thread termination */
int my_pthread_join(my_pthread_t thread, void** value_ptr) {
	init();
	// Waiting for a specific thread to terminate
	// Once this thread finishes,
	// Deallocated any dynamic memory created when starting this thread
	if (currentThread != NULL && currentThread->threadId == thread) return -1;
	while (1) {
		blockTimer();
		tcb* terminated = queueFindByThreadId(&finishedQueue, thread);
		if (terminated != NULL) {
			terminated = queueRemoveByThreadId(&finishedQueue, thread);
			unblockTimer();
			if (terminated == NULL) return -1;
			if (value_ptr != NULL) *value_ptr = terminated->returnValue;
			free(terminated);
			return 0;
		}
		if (!queueContainsThreadId(&readyQueue, thread) &&
			!queueContainsThreadId(&blockQueue, thread) &&
			(currentThread == NULL || currentThread->threadId != thread)) {
			unblockTimer();
			return -1;
		}
		unblockTimer();
		my_pthread_yield();
	}
	// YOUR CODE HERE
};

/* intialize data structures for this mutex
	mutex->itialize the mutex lock */
int my_pthread_mutex_init(my_pthread_mutex_t* mutex,
	const pthread_mutexattr_t* mutexattr) {
	// .kind = 0;
	init();
	mutex->lock.lock = 0;
	mutex->lock.owner = 0;
	mutex->lock.nusers = 0;
	mutex->lock.kind = 0;
	mutex->lock.list.head = NULL;
	mutex->lock.list.tail = NULL;
	mutex->lock.list.size = 0;
	// YOUR CODE HERE
	return 0;
};

/* aquire the mutex lock */
int my_pthread_mutex_lock(my_pthread_mutex_t* mutex) {
	// Use the built-in test-and-set atomic function to test the mutex
	// If mutex is acquired successfuly, enter critical section
	// If acquiring mutex fails, push current thread into block list 
	// and context switch to scheduler
	init();
	while (__sync_lock_test_and_set(&mutex->lock.lock, 1) == 1) {
		currentThread->status = SUSPENDED;
		listPush(&mutex->lock.list, currentThread);
		swapcontext(&currentThread->context, &schedulerContext);
	}
	mutex->lock.owner = currentThread->threadId;
	// YOUR CODE HERE
	return 0;
};

/* release the mutex lock */
int my_pthread_mutex_unlock(my_pthread_mutex_t* mutex) {
	// Release mutex and make it available again. 
	// Put threads in block list to run queue 
	// so that they could compete for mutex later.
	init();
	__sync_lock_release(&mutex->lock.lock);
	mutex->lock.owner = 0;
	while (!listIsEmpty(&mutex->lock.list)) {
		tcb* thread = listPop(&mutex->lock.list);
		thread->status = NOT_STARTED;
		queuePush(&readyQueue, thread);
	}
	maybePreempt();
	// YOUR CODE HERE
	return 0;
};


/* destroy the mutex */
int my_pthread_mutex_destroy(my_pthread_mutex_t* mutex) {
	// Deallocate dynamic memory created in my_pthread_mutex_init
	init();
	while (!listIsEmpty(&mutex->lock.list)) listPop(&mutex->lock.list);
	mutex->lock.lock = 0;
	mutex->lock.owner = 0;
	mutex->lock.nusers = 0;
	mutex->lock.kind = 0;
	return 0;
};

/* scheduler */
static void schedule() {
	// Every time when timer interrup happens, your thread library 
	// should be contexted switched from thread context to this 
	// schedule function

	// Invoke different actual scheduling algorithms
	// according to policy (STCF or MLFQ)

	// if (sched == STCF)
	//		sched_stcf();
	// else if (sched == MLFQ)
	// 		sched_mlfq();

	// YOUR CODE HERE

// schedule policy
#ifndef MLFQ
	// Choose STCF
	makecontext(&schedulerContext, (void (*)(void))sched_stcf, 0);
#else 
	// Choose MLFQ
	makecontext(&schedulerContext, (void (*)(void))sched_mlfq, 0);
#endif

}

/* Preemptive SJF (STCF) scheduling algorithm */
static void sched_stcf() {
	// Your own implementation of STCF
	// (feel free to modify arguments and return types)
	timerFlag = 1;
	while (true) {
		blockTimer();
		tcb* nextThread = queueFindMinExecutedTime(&readyQueue);
		if (nextThread == NULL) {
			unblockTimer();
			continue;
		}
		queueRemoveByThreadId(&readyQueue, nextThread->threadId);
		nextThread->status = RUNNING;
		currentThread = nextThread;
		unblockTimer();
		swapcontext(&schedulerContext, &nextThread->context);
	}
	// YOUR CODE HERE
}

/* Preemptive MLFQ schedulingl  algorithm */
static void sched_mlfq() {
	// Your own implementation of MLFQ
	// (feel free to modify arguments and return types)
	timerFlag = 1;
	while (true) {
		blockTimer();
		tcb* nextThread = queueFindMinPriority(&readyQueue);
		if (nextThread == NULL) {
			unblockTimer();
			continue;
		}
		queueRemoveByThreadId(&readyQueue, nextThread->threadId);
		nextThread->status = RUNNING;
		nextThread->priority++;
		if (nextThread->priority >= MLFQSize) nextThread->priority = MLFQSize - 1;
		setTimer(1000 * (1 << (MLFQSize - 1 - nextThread->priority)));
		currentThread = nextThread;
		unblockTimer();
		swapcontext(&schedulerContext, &nextThread->context);
	}
	// YOUR CODE HERE
}

// Feel free to add any other functions you need

void timerHander(int signo) {
	if (signo != SIGALRM) return;
	if (!timerFlag) return;
	if (currentThread == NULL) return;
	if (currentThread->status != RUNNING) return;
	preemptPending = 1;
}

void setTimer(int pritime) {
	struct sigaction act;
	struct itimerval timer;
	act.sa_handler = timerHander;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_RESTART;
	sigaction(SIGALRM, &act, NULL);
	sigemptyset(&timerMask);
	sigaddset(&timerMask, SIGALRM);
	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = pritime;
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = pritime;
	setitimer(ITIMER_REAL, &timer, NULL);
}

void blockTimer() {
	sigprocmask(SIG_BLOCK, &timerMask, NULL);
}

void unblockTimer() {
	sigprocmask(SIG_UNBLOCK, &timerMask, NULL);
}

void listPush(my_pthread_list_t* list, tcb* thread) {
	node* newNode = (node*)malloc(sizeof(node));
	newNode->thread = thread;
	newNode->next = NULL;
	if (list->tail == NULL) list->head = list->tail = newNode;
	else list->tail->next = newNode, list->tail = newNode;
	list->size++;
}

tcb* listPop(my_pthread_list_t* list) {
	if (list->head == NULL) return NULL;
	node* temp = list->head;
	tcb* thread = temp->thread;
	list->head = list->head->next;
	if (list->head == NULL) list->tail = NULL;
	free(temp);
	list->size--;
	return thread;
}

bool listIsEmpty(my_pthread_list_t* list) {
	return list->size == 0;
}

void queuePush(my_pthread_queue_t* queue, tcb* thread) {
	node* newNode = (node*)malloc(sizeof(node));
	newNode->thread = thread;
	newNode->next = NULL;
	if (queue->tail == NULL) queue->head = queue->tail = newNode;
	else queue->tail->next = newNode, queue->tail = newNode;
	queue->size++;
}

tcb* queuePop(my_pthread_queue_t* queue) {
	if (queue->head == NULL) return NULL;
	node* temp = queue->head;
	tcb* thread = temp->thread;
	queue->head = queue->head->next;
	if (queue->head == NULL) queue->tail = NULL;
	free(temp);
	queue->size--;
	return thread;
}

bool queueIsEmpty(my_pthread_queue_t* queue) {
	return queue->size == 0;
}

void queueClear(my_pthread_queue_t* queue) {
	while (queue->head != NULL) {
		node* temp = queue->head;
		queue->head = queue->head->next;
		free(temp);
	}
	queue->tail = NULL;
	queue->size = 0;
}

tcb* queueFindByThreadId(my_pthread_queue_t* queue, my_pthread_t threadId) {
	node* temp = queue->head;
	while (temp != NULL) {
		if (temp->thread != NULL && temp->thread->threadId == threadId) return temp->thread;
		temp = temp->next;
	}
	return NULL;
}

tcb* queueRemoveByThreadId(my_pthread_queue_t* queue, my_pthread_t threadId) {
	node* prev = NULL;
	node* temp = queue->head;
	while (temp != NULL) {
		if (temp->thread != NULL && temp->thread->threadId == threadId) {
			tcb* found = temp->thread;
			if (prev == NULL) queue->head = temp->next;
			else prev->next = temp->next;
			if (queue->tail == temp) queue->tail = prev;
			queue->size--;
			free(temp);
			return found;
		}
		prev = temp;
		temp = temp->next;
	}
	return NULL;
}

bool queueContainsThreadId(my_pthread_queue_t* queue, my_pthread_t threadId) {
	return queueFindByThreadId(queue, threadId) != NULL;
}

tcb* queueFindMinExecutedTime(my_pthread_queue_t* queue) {
	if (queue->head == NULL) return NULL;
	node* temp = queue->head;
	tcb* minThread = temp->thread;
	while (temp != NULL) {
		if (temp->thread->executedTime < minThread->executedTime) minThread = temp->thread;
		temp = temp->next;
	}
	return minThread;
}

tcb* queueFindMinPriority(my_pthread_queue_t* queue) {
	if (queue->head == NULL) return NULL;
	node* temp = queue->head;
	tcb* minThread = temp->thread;
	while (temp != NULL) {
		if (temp->thread->priority < minThread->priority) minThread = temp->thread;
		temp = temp->next;
	}
	return minThread;
}

void threadBootstrap() {
	void* returnValue = currentThread->startRoutine(currentThread->startArg);
	my_pthread_exit(returnValue);
}

void maybePreempt() {
	if (!preemptPending) return;
	if (currentThread == NULL || currentThread->status != RUNNING) return;
	my_pthread_yield();
}
// YOUR CODE HERE

