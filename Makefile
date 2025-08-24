CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2
TARGET = ts
SOURCE = ts.c
TEST_TARGET = test_ts_simple
TEST_SOURCE = test_ts_simple.c

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE)

$(TEST_TARGET): $(TEST_SOURCE)
	$(CC) $(CFLAGS) -o $(TEST_TARGET) $(TEST_SOURCE)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -f $(TARGET) $(TEST_TARGET) ts_clang ts_c99

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/$(TARGET)

.PHONY: all clean install uninstall test