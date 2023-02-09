CC=gcc
INC=-Iinclude
CFLAG=-std=c99 -Wall -Wextra -g -gdwarf-4 -c $(INC)
LIB=-llog -lcollectc
LDFLAG=-Llib $(LIB)

all: server client test

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
	rm -rf obj server client test
