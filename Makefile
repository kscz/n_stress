CC = gcc
CFLAGS = -Wall
LDFLAGS = -lpthread
ifeq (${DEBUG}, yes)
  CFLAGS += -g
  OPTIMIZATION = -O0
else
  OPTIMIZATION = -O2
endif

all: n_stress

n_stress: n_stress.o packets.o ccrc32.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(OPTIMIZATION) -o $@ *.o

%.o : %.c
	$(CC) $(CFLAGS) $(OPTIMIZATION) -c $< -o $@

clean :
	rm -f *.o n_stress
