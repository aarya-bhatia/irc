all: request

request: obj/request.o obj/log.o
	gcc -std=c99 $^ -o $@

obj/request.o: request.c
	@mkdir -p obj;
	gcc -std=c99 -Wall -g -c $< -o $@

obj/log.o: log.c
	@mkdir -p obj;
	gcc -Wall -std=c99 -DLOG_USE_COLOR -g -c $< -o $@

clean:
	rm -rf obj request
