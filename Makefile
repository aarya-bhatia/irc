CC=gcc
INC=-I/usr/local/include
CFLAG=-std=c99 -Wall -Wextra -g -c $(INC)
LIB=-llog -lcollectc
LDFLAG=-L/usr/local/lib $(LIB)

all: server client

OBJ=obj/common.o obj/message.o

client: obj/client.o $(OBJ)
	$(CC) $^ $(LDFLAG) -o $@

server: obj/server.o $(OBJ)
	$(CC) -pthread $^ $(LDFLAG) -o $@

test: obj/test.o obj/common.o
	$(CC) $^ $(LDFLAG) -o $@

message: obj/message.o obj/common.o
	$(CC) $^ $(LDFLAG) -o $@

obj/%.o: %.c
	@mkdir -p obj;
	$(CC) $(CFLAG) $< -o $@

clean:
	rm -rf obj
