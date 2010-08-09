CFLAGS=-Wall -O2 -DTEST_SQLITE
LDFLAGS=-lsqlite3

all: fs-test
clean:
	rm -f fs-test
