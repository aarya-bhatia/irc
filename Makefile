CC=cc
INC=-Isrc -Isrc/include
CFLAG=-std=c99 -Wall -Wextra -pedantic -gdwarf-4 -O0 -c $(INC)
LIB=-llog -lcollectc -laaryab2
LDFLAG=-Llib $(LIB)

all: obj server client

COMMON_OBJ=obj/common.o obj/message.o obj/queue.o

SERVER_OBJ=obj/server_main.o obj/server.o 
SERVER_OBJ+=obj/user.o obj/load_nicks.o obj/register.o
SERVER_OBJ+=$(COMMON_OBJ)

CLIENT_OBJ=obj/client.o obj/client_threads.o 
CLIENT_OBJ+=$(COMMON_OBJ)

client: $(CLIENT_OBJ)
	$(CC) -pthread $^ $(LDFLAG) -o $@

server: $(SERVER_OBJ)
	$(CC) $^ $(LDFLAG) -o $@

test: obj/test.o $(COMMON_OBJ)
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

obj:
	mkdir -p obj

obj/%.o: src/%.c
	@mkdir -p $(dir $@);
	$(CC) $(CFLAG) $< -o $@

tags:
	cd src && ctags -R --sort=yes --c++-kinds=+p --fields=+iaS --extra=+q .

clean:
	rm -rf obj server client *.out
