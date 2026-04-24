CC = gcc
CFLAGS = -g -w
LIB_DIR = ..
LIB_FILE = $(LIB_DIR)/libmy_pthread.a
LDLIBS = -L$(LIB_DIR) -lmy_pthread

all: parallelCal vectorMultiply externalCal

$(LIB_FILE):
	$(MAKE) -C $(LIB_DIR) my_pthread.a

parallelCal: parallelCal.c $(LIB_FILE)
	$(CC) $(CFLAGS) -pthread -o parallelCal parallelCal.c $(LDLIBS)

vectorMultiply: vectorMultiply.c $(LIB_FILE)
	$(CC) $(CFLAGS) -pthread -o vectorMultiply vectorMultiply.c $(LDLIBS)

externalCal: externalCal.c $(LIB_FILE)
	$(CC) $(CFLAGS) -pthread -o externalCal externalCal.c $(LDLIBS)

clean:
	rm -rf testcase parallelCal vectorMultiply externalCal *.o ./record/
