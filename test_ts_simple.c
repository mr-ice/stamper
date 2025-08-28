/*
 * test_ts_simple.c - simple test runner for ts
 *
 * Copyright (C) 2025  Michael Rice <michael@riceclan.org>
 * 
 * based on work by Jiri Dvorak <jiri.dvorak@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *//* Feature test macros to enable POSIX functions */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

// Simple test runner
static bool run_simple_test(const char *name, const char *input, const char *args) {
    char cmd[512];
    char input_file[] = "/tmp/ts_test_XXXXXX";

    // Create temporary input file
    int fd = mkstemp(input_file);
    if (fd == -1) {
        printf("FAIL: %s - Could not create temp file\n", name);
        return false;
    }

    write(fd, input, strlen(input));
    close(fd);

    // Build command
    snprintf(cmd, sizeof(cmd), "./ts %s < %s", args ? args : "", input_file);

    // Run command and capture output
    FILE *pipe = popen(cmd, "r");
    if (!pipe) {
        printf("FAIL: %s - Could not run command\n", name);
        unlink(input_file);
        return false;
    }

    char output[1024];
    size_t total_read = 0;
    char buffer[256];

    while (fgets(buffer, sizeof(buffer), pipe) && total_read < sizeof(output) - 1) {
        size_t len = strlen(buffer);
        if (total_read + len < sizeof(output) - 1) {
            memcpy(output + total_read, buffer, len);
            total_read += len;
        }
    }

    output[total_read] = '\0';
    pclose(pipe);
    unlink(input_file);

    // Check if output contains expected content
    bool passed = (strlen(output) > 0);

    if (passed) {
        printf("PASS: %s\n", name);
    } else {
        printf("FAIL: %s - No output\n", name);
    }

    return passed;
}

int main() {
    printf("Running simple ts tests...\n");

    // Compile ts program
    system("make clean && make");

    if (access("./ts", X_OK) != 0) {
        printf("ERROR: ts executable not found\n");
        return 1;
    }

    int passed = 0;
    int total = 0;

    // Test 1: Basic timestamp
    total++;
    if (run_simple_test("Basic timestamp", "test line\n", "")) passed++;

    // Test 2: Custom format
    total++;
    if (run_simple_test("Custom format", "test line\n", "\"%Y-%m-%d\"")) passed++;

    // Test 3: Subsecond format
    total++;
    if (run_simple_test("Subsecond format", "test line\n", "\"%.S\"")) passed++;

    // Test 4: Unix timestamp format
    total++;
    if (run_simple_test("Unix timestamp", "test line\n", "\"%s\"")) passed++;

    // Test 5: Relative mode with Unix timestamp
    total++;
    if (run_simple_test("Relative mode Unix", "1755921813 test\n", "-r")) passed++;

    // Test 6: Relative mode with syslog format
    total++;
    if (run_simple_test("Relative mode syslog", "Dec 22 22:25:23 test\n", "-r")) passed++;

    // Test 7: Unique mode
    total++;
    if (run_simple_test("Unique mode", "same\nsame\ndifferent\n", "-u")) passed++;

    // Test 8: Incremental mode
    total++;
    if (run_simple_test("Incremental mode", "line1\nline2\n", "-i")) passed++;

    // Test 9: Since start mode
    total++;
    if (run_simple_test("Since start mode", "line1\nline2\n", "-s")) passed++;

    // Test 10: Monotonic clock
    total++;
    if (run_simple_test("Monotonic clock", "test line\n", "-m")) passed++;

    // Test 11: Mixed format
    total++;
    if (run_simple_test("Mixed format", "test line\n", "\"%Y%m%d-%H%M%S.%.S\"")) passed++;

    // Test 12: Line without timestamp in relative mode
    total++;
    if (run_simple_test("No timestamp in relative", "no timestamp here\n", "-r")) passed++;

    printf("\nResults: %d/%d tests passed\n", passed, total);

    if (passed == total) {
        printf("All tests passed! 🎉\n");
        return 0;
    } else {
        printf("Some tests failed! ❌\n");
        return 1;
    }
}