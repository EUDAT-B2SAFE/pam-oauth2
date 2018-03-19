CC = gcc
CFLAGS = -O0 -g -Wall -Wextra -std=c89 -pedantic-errors

CPPC = g++
CPPFLAGS = -O0 -g -Wall -Wextra

#all: test testcpp
all: libparson.a

libparson.a: parson.o
	$(AR) rc $@ $^

%.o: %.c parson.h
	$(CC) -c $(CFLAGS) $< -o $@

.PHONY: test testcpp
test: tests.c parson.c
	$(CC) $(CFLAGS) -o $@ tests.c parson.c
	./$@

testcpp: tests.c parson.c
	$(CPPC) $(CPPFLAGS) -o $@ tests.c parson.c
	./$@

clean:
	rm -f test *.o
	rm -f *.a *.so
