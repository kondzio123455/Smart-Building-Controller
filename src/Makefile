CC = gcc
CFLAGS = -std=c11 -pedantic -Wall -Wextra
LDFLAGS = -lraylib -lm
SRC = $(filter-out test_sim.c, $(wildcard *.c))
HDR = $(wildcard *.h)

ifeq ($(shell uname -s),Darwin)
BREW := $(shell brew --prefix 2>/dev/null)
CFLAGS += -I$(BREW)/include
LDFLAGS = -L$(BREW)/lib -lraylib -lm
endif

app: $(SRC) $(HDR)
	$(CC) $(CFLAGS) $(SRC) -o app $(LDFLAGS)

# headless tests: the simulation only, no window and no raylib needed
test: test_sim.c simulation.c building.h
	$(CC) -std=c11 -pedantic -Wall -Wextra test_sim.c simulation.c -o test_sim -lm
	./test_sim

# proves the project is plain C: no C++ files, no C++ code, and every file compiles as strict C11
check-c:
	@if ls *.cpp *.cc *.cxx *.hpp *.hxx 2>/dev/null | grep -q .; then echo "FAIL: C++ files found"; exit 1; fi
	@if grep -nE 'std::|namespace |template ?<|#include <(iostream|vector|string|cstdio|cstdlib|cmath|cstring)>|nullptr|class [A-Za-z_]+ *[:{]' *.c *.h; then echo "FAIL: C++ code found"; exit 1; fi
	$(CC) $(CFLAGS) -pedantic-errors -fsyntax-only $(SRC) test_sim.c
	@echo "OK: the whole project is plain C (checked as strict C11)"

clean:
	rm -f app test_sim

.PHONY: clean test check-c