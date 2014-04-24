CFLAGS=-std=c99 -pedantic -Wall -Wextra -O2 -DTEST_SQLITE
LDFLAGS=-lsqlite3

all: fs-test fs-test-static
fs-test-static: fs-test.c
	diet cc  -std=c99 -pedantic -Wall -Wextra -O2 fs-test.c   -o fs-test-static

clean:
	rm -f fs-test fs-test-static
