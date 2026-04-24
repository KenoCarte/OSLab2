#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

pthread_t t1, t2;
pthread_mutex_t mutex;
int x = 0;

void* inc_shared_counter(void* arg) {
	/* Increment shared counter x for 5 times */
	// Add your code here for Part 1
	for (int i = 0;i < 5;i++) {
		pthread_mutex_lock(&mutex);
		x++;
		printf("x is incremented to %d\n", x);
		pthread_mutex_unlock(&mutex);
	}
	return NULL;
}

int main() {
	/* Part 1: Recall of pthread programming */
	// Add your code here for Part 1
	pthread_mutex_init(&mutex, NULL);
	pthread_create(&t1, NULL, inc_shared_counter, NULL);
	pthread_create(&t2, NULL, inc_shared_counter, NULL);
	pthread_join(t1, NULL);
	pthread_join(t2, NULL);
	printf("The final value of x is %d\n", x);
	pthread_mutex_destroy(&mutex);
	return 0;
}
