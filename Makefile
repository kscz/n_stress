CC = gcc

# In future versions when we have code located in RAM we will have jumps larger
# than 25 bits away we'll need -mlong-calls option.
#
# Also, when we need tighter controls over what code executes where, we should
# modify the linker script in conjuntion with the -ffunction-sections to put
# each function in its own section
CFLAGS = -Wall -Werror -g
#  -mthumb-interwork
LDFLAGS = -lpthread

all: client server


server : server.o packets.o ccrc32.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

client : client.o packets.o ccrc32.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

client.o : client.c
	$(CC) $(CFLAGS) -c $< -o $@

server.o : server.c
	$(CC) $(CFLAGS) -c $< -o $@

packets.o : packets.c
	$(CC) $(CFLAGS) -c $< -o $@

ccrc32.o : ccrc32.c
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm *.o server client
