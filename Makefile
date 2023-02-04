CC=gcc
INC=-I/usr/local/include
CFLAG=-std=c99 -Wall -Wextra -g -gdwarf-4 -c $(INC)
LIB=-llog -lcollectc
LDFLAG=-L/usr/local/lib $(LIB)

all: server client test

OBJ=obj/common.o obj/message.o

client: obj/client.o $(OBJ)
	$(CC) $^ $(LDFLAG) -o $@

server: obj/server.o $(OBJ)
	$(CC) $^ $(LDFLAG) -o $@

test: obj/test.o $(OBJ)
	$(CC) $^ $(LDFLAG) -o $@

obj/%.o: src/%.c
	@mkdir -p obj;
	$(CC) $(CFLAG) $< -o $@

clean:
	rm -rf obj server client test
