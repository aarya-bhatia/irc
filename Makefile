CC=gcc
INC=-Isrc -Isrc/include
CFLAG=-std=c99 -Wall -Wextra -pedantic -g -O0 -c $(INC)
LIB=-llog -lcollectc -laaryab2
LDFLAG=-Llib $(LIB)

all: server client

OBJ=obj/common.o obj/message.o

client: obj/client.o $(OBJ)
	$(CC) $^ $(LDFLAG) -o $@

server: obj/server.o obj/command.o $(OBJ)
	$(CC) $^ $(LDFLAG) -o $@

test: obj/test.o $(OBJ)
	$(CC) $^ $(LDFLAG) -o $@

obj/%.o: src/%.c
	@mkdir -p obj;
	$(CC) $(CFLAG) $< -o $@

tags:
	cd src && ctags -R --sort=yes --c++-kinds=+p --fields=+iaS --extra=+q .

clean:
	rm -rf obj server client
