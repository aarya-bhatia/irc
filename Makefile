CLIENT_DIR=src/client
SERVER_DIR=src/server
COMMON_DIR=src/common

INCLUDE_DIRS=$(CLIENT_DIR) $(SERVER_DIR) $(COMMON_DIR)
INCLUDES=$(addprefix -I, $(INCLUDE_DIRS))

LDFLAGS=-Llib -llog

CFLAGS=-std=c99 -Wall -Wextra -pedantic -gdwarf-4 -MMD -MP -O0 -c $(INCLUDES)

COMMON_FILES=$(shell find $(COMMON_DIR) -type f -name "*.c")
SERVER_FILES=$(shell find $(SERVER_DIR) -type f -name "*.c")
CLIENT_FILES=$(shell find $(CLIENT_DIR) -type f -name "*.c")

COMMON_OBJ=$(COMMON_FILES:src/%.c=obj/%.o)
SERVER_OBJ=$(SERVER_FILES:src/%.c=obj/%.o) $(COMMON_OBJ)
CLIENT_OBJ=$(CLIENT_FILES:src/%.c=obj/%.o) $(COMMON_OBJ)
TEST_OBJ=obj/test/test.o $(COMMON_OBJ)

SERVER_EXE=build/server
CLIENT_EXE=build/client
TEST_EXE=build/test

all: $(SERVER_EXE) $(CLIENT_EXE) $(TEST_EXE)

$(CLIENT_EXE): $(CLIENT_OBJ)
	@mkdir -p $(dir $@);
	$(CC) -pthread $^ $(LDFLAGS) -o $@

$(SERVER_EXE): $(SERVER_OBJ)
	@mkdir -p $(dir $@);
	$(CC) $^ $(LDFLAGS) -o $@

$(TEST_EXE): $(TEST_OBJ)
	@mkdir -p $(dir $@);
	$(CC) $^ $(LDFLAGS) -o $@

obj/%.o: src/%.c
	@mkdir -p $(dir $@);
	$(CC) $(CFLAGS) $< -o $@

tags:
	cd src && ctags -R --sort=yes --c++-kinds=+p --fields=+iaS --extra=+q .

clean:
	rm -rf obj build

.PHONY: clean tags

-include $(OBJS_DIR)/*.d