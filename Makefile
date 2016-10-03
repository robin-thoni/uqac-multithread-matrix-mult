CFLAGS = -Wall -Wextra -Werror -pedantic -std=c99
LDLIBS = -lpthread
CC = gcc
SOURCES = main.c
OBJS = $(SOURCES:.c=.o)
OUT = multithread-matrix-mult

all: release

debug: CFLAGS += -g3 -ggdb3
debug: $(OUT)

release: CFLAGS += -o3
release: $(OUT)

$(OUT): $(OBJS)
	  $(LINK.c) $(OUTPUT_OPTION) $(OBJS) $(LDLIBS)

clean:
	  rm -f *.o

distclean: clean
	  rm -f *.a $(OUT)
