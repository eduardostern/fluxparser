CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -pthread
LDFLAGS = -lm -pthread

TARGETS = parser_test test_vars example_usage test_safety demo_safety test_advanced test_research test_calculus test_numerical test_new_features calculate_pi
HEADERS = parser.h ast.h

.PHONY: all clean run

all: $(TARGETS)

parser_test: test.o parser.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test_vars: test_vars.o parser.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

example_usage: example_usage.o parser.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test_safety: test_safety.o parser.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

demo_safety: demo_safety.o parser.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test_advanced: test_advanced.o parser.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test_research: test_research.o ast.o parser.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test_calculus: test_calculus.o ast.o parser.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test_numerical: test_numerical.o ast.o parser.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test_new_features: test_new_features.o ast.o parser.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

calculate_pi: calculate_pi.o ast.o parser.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o $(TARGETS)

run: parser_test
	./parser_test
