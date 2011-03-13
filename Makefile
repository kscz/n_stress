CC = gcc
CFLAGS = -Wall -Werror -o2
LDFLAGS = -lpthread

all: n_stress

n_stress: n_stress.o packets.o ccrc32.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm *.o n_stress
