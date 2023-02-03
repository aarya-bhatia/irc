CC=gcc
INC=-I/usr/local/include
CFLAG=-std=c99 -Wall -Wextra -g -c $(INC)
LIB=-llog -lcollectc
LDFLAG=-L/usr/local/lib $(LIB)

all: server client

client: obj/client.o obj/net.o obj/common.o
	$(CC) $^ $(LDFLAG) -o $@

server: obj/server.o obj/net.o obj/common.o
	$(CC) -pthread $^ $(LDFLAG) -o $@

test: obj/test.o obj/common.o
	$(CC) $^ $(LDFLAG) -o $@

obj/%.o: %.c
	@mkdir -p obj;
	$(CC) $(CFLAG) $< -o $@

clean:
	rm -rf obj/*.o
