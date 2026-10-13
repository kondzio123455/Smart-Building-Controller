CC = gcc
CFLAGS = -Wall -Wextra
LDFLAGS = -lraylib -lm
SRC = $(wildcard *.c)

ifeq ($(shell uname -s),Darwin)
BREW := $(shell brew --prefix)
CFLAGS += -I$(BREW)/include
LDFLAGS = -L$(BREW)/lib -lraylib -lm
endif

app: $(SRC) building.h
	$(CC) $(CFLAGS) $(SRC) -o app $(LDFLAGS)

clean:
	rm -f app
