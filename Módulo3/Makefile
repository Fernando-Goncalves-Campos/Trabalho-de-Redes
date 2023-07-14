CC=g++
CFLAGS=-I -Wall -Werror -pthread
PROGRAMS= server client

all: $(PROGRAMS)

$(PROGRAMS): %: %.cpp
	$(CC) $(CFLAGS) -o $@ $< 