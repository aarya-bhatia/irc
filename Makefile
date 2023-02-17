CC=cc
INC=-Isrc -Isrc/include
CFLAG=-std=c99 -Wall -Wextra -pedantic -gdwarf-4 -O0 -c $(INC)
LIB=-llog -lcollectc -laaryab2
LDFLAG=-Llib $(LIB)

all: server client

OBJ=obj/common.o obj/message.o obj/queue.o obj/timer.o
SERVER_OBJ=obj/server_main.o obj/server.o obj/user.o obj/command.o $(OBJ)
CLIENT_OBJ=obj/client.o obj/client_threads.o $(OBJ)

client: $(CLIENT_OBJ)
	$(CC) -pthread $^ $(LDFLAG) -o $@

server: $(SERVER_OBJ)
	$(CC) $^ $(LDFLAG) -o $@

test: obj/test.o $(OBJ)
	$(CC) $^ $(LDFLAG) -o $@

ht: obj/test/ht.o $(OBJ)
	$(CC) $^ $(LDFLAG) -o $@
	
libaaryab2:
	mkdir -p obj/lib/aaryab2;
	mkdir -p lib;
	cc $(CFLAG) src/include/aaryab2/String.c -o obj/lib/aaryab2/String.o;
	rm -rf lib/libaaryab2.a;
	ar rs lib/libaaryab2.a obj/lib/aaryab2/String.o;
	ar t lib/libaaryab2.a;

obj/test/%.o: test/%.c
	@mkdir -p $(dir $@);
	$(CC) $(CFLAG) $< -o $@

obj/%.o: src/%.c
	@mkdir -p $(dir $@);
	$(CC) $(CFLAG) $< -o $@

tags:
	cd src && ctags -R --sort=yes --c++-kinds=+p --fields=+iaS --extra=+q .

clean:
	rm -rf obj server client *.out
