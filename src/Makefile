CC = gcc
CFLAGS = -Wall -Wextra
LDFLAGS = -lraylib -lm
SRC = $(wildcard *.c)
HDR = $(wildcard *.h)

ifeq ($(shell uname -s),Darwin)
BREW := $(shell brew --prefix 2>/dev/null)
CFLAGS += -I$(BREW)/include
LDFLAGS = -L$(BREW)/lib -lraylib -lm
endif

app: $(SRC) $(HDR)
	$(CC) $(CFLAGS) $(SRC) -o app $(LDFLAGS)

clean:
	rm -f app

.PHONY: clean
