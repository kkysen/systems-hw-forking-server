CC = gcc -ggdb -std=gnu99 -Wall -Werror
BASIC_SERVER_OUT = bserver
FORKING_SERVER_OUT = fserver
CLIENT_OUT = client

util/utils.o:
	$(CC) -c util/utils.c -o util/utils.o

util/string_utils.o:
	$(CC) -c util/string_utils.c -o util/string_utils.o

util/sized_string.o:
	$(CC) -c util/sized_string.c -o util/sized_string.o

util/string_builder.o:
	$(CC) -c util/string_builder.c -o util/string_builder.o

util/sigaction.o:
	$(CC) -c util/sigaction.c -o util/sigaction.o

util/stacktrace.o:
	$(CC) -c util/stacktrace.c -o util/stacktrace.o

util/io.o:
	$(CC) -c util/io.c -o util/io.o

util/random.o:
	$(CC) -c util/random.c -o util/random.o

util/hash.o:
	$(CC) -c util/hash.c -o util/hash.o

pipe_networking.o:
	$(CC) -c pipe_networking.c

modify_text.o:
	$(CC) -c modify_text.c

UTILS = modify_text.o pipe_networking.o util/hash.o util/random.o util/io.o util/stacktrace.o util/sigaction.o util/string_builder.o util/sized_string.o util/string_utils.o util/utils.o -lssl -lcrypto

basic_server.o:
	$(CC) -c basic_server.c

forking_server.o:
	$(CC) -c forking_server.c

client.o:
	$(CC) -c client.c

$(BASIC_SERVER_OUT): basic_server.o $(UTILS)
	$(CC) -o $(BASIC_SERVER_OUT) basic_server.o $(UTILS)

$(FORKING_SERVER_OUT): forking_server.o $(UTILS)
	$(CC) -o $(FORKING_SERVER_OUT) forking_server.o $(UTILS)

$(CLIENT_OUT): client.o $(UTILS)
	$(CC) -o $(CLIENT_OUT) client.o $(UTILS)

all: clean $(BASIC_SERVER_OUT) $(FORKING_SERVER_OUT) $(CLIENT_OUT)

clean:
	touch dummy.o
	find . -name '*.o' -delete
	rm -f $(BASIC_SERVER_OUT)
	rm -f $(FORKING_SERVER_OUT)
	rm -f $(CLIENT_OUT)

install: clean all

run: install
	./$(FORKING_SERVER_OUT)

rerun: all
	./$(FORKING_SERVER_OUT)

valgrind: clean all
	valgrind -v --leak-check=full ./$(FORKING_SERVER_OUT)

test:
	gcc -E pipe_networking.c | grep -v '^$$' | ./client

