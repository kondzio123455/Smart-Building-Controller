CC = gcc
CFLAGS = -Wall -Wextra -I$(shell brew --prefix)/include
LDFLAGS = -L$(shell brew --prefix)/lib -lraylib -lm
SRC = $(wildcard *.c)

app: $(SRC) building.h
	$(CC) $(CFLAGS) $(SRC) -o app $(LDFLAGS)

clean:
	rm -f app
