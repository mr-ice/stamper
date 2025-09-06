/*
 * test_ts_simple.c - comprehensive test runner for ts
 *
 * Copyright (C) 2025  Michael Rice <michael@riceclan.org>
 *
 * based on work by Joey Hess <joey@kitenet.net>
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
 */

/* Feature test macros to enable POSIX functions */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <regex.h>
#include <time.h>

// Test result structure
typedef struct {
    bool passed;
    char *error_msg;
} test_result_t;

// Helper function to check if a string matches a regex pattern
static bool matches_pattern(const char *str, const char *pattern) {
    regex_t regex;
    int ret = regcomp(&regex, pattern, REG_EXTENDED | REG_NOSUB);
    if (ret != 0) {
        return false;
    }

    ret = regexec(&regex, str, 0, NULL, 0);
    regfree(&regex);
    return ret == 0;
}

// Helper function to check if output contains expected pattern
static bool output_contains_pattern(const char *output, const char *pattern) {
    if (!output || !pattern) return false;

    char *line = strtok((char*)output, "\n");
    while (line != NULL) {
        if (matches_pattern(line, pattern)) {
            return true;
        }
        line = strtok(NULL, "\n");
    }
    return false;
}

// Helper function to count lines in output
static int count_output_lines(const char *output) {
    if (!output) return 0;

    int count = 0;
    const char *ptr = output;
    while (*ptr) {
        if (*ptr == '\n') count++;
        ptr++;
    }
    return count;
}

// Enhanced test runner with validation
static test_result_t run_test_with_validation(const char *input,
                                            const char *args, const char *expected_pattern,
                                            int expected_lines) {
    test_result_t result = {false, NULL};
    char cmd[512];
    char input_file[] = "/tmp/ts_test_XXXXXX";

    // Create temporary input file
    int fd = mkstemp(input_file);
    if (fd == -1) {
        result.error_msg = "Could not create temp file";
        return result;
    }

    write(fd, input, strlen(input));
    close(fd);

    // Build command
    snprintf(cmd, sizeof(cmd), "./ts %s < %s", args ? args : "", input_file);

    // Run command and capture output
    FILE *pipe = popen(cmd, "r");
    if (!pipe) {
        result.error_msg = "Could not run command";
        unlink(input_file);
        return result;
    }

    char output[2048];
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

    // Validate output
    if (strlen(output) == 0) {
        result.error_msg = "No output produced";
        return result;
    }

    // Check line count if specified
    if (expected_lines > 0) {
        int actual_lines = count_output_lines(output);
        if (actual_lines != expected_lines) {
            result.error_msg = malloc(128);
            snprintf(result.error_msg, 128, "Expected %d lines, got %d", expected_lines, actual_lines);
            return result;
        }
    }

    // Check pattern if specified
    if (expected_pattern) {
        if (!output_contains_pattern(output, expected_pattern)) {
            result.error_msg = malloc(256);
            snprintf(result.error_msg, 256, "Output does not match expected pattern: %s\nActual output: %s",
                    expected_pattern, output);
            return result;
        }
    }

    result.passed = true;
    return result;
}


int main() {
    printf("Running comprehensive ts tests...\n");

    // Compile ts program
    system("make clean && make");

    if (access("./ts", X_OK) != 0) {
        printf("ERROR: ts executable not found\n");
        return 1;
    }

    int passed = 0;
    int total = 0;
    test_result_t result;

    // Test 1: Basic timestamp - should match default format (Dec 22 22:25:23)
    total++;
    result = run_test_with_validation("test line\n", "",
                                    "^[A-Za-z]{3} [0-9]{1,2} [0-9]{2}:[0-9]{2}:[0-9]{2} test line$", 1);
    if (result.passed) {
        printf("PASS: %s\n", "Basic timestamp");
        passed++;
    } else {
        printf("FAIL: %s - %s\n", "Basic timestamp", result.error_msg);
        if (result.error_msg) free(result.error_msg);
    }

    // Test 2: Custom format - should match YYYY-MM-DD format
    total++;
    result = run_test_with_validation("test line\n", "\"%Y-%m-%d\"",
                                    "^[0-9]{4}-[0-9]{2}-[0-9]{2} test line$", 1);
    if (result.passed) {
        printf("PASS: %s\n", "Custom format");
        passed++;
    } else {
        printf("FAIL: %s - %s\n", "Custom format", result.error_msg);
        if (result.error_msg) free(result.error_msg);
    }

    // Test 3: Subsecond format - should have fractional seconds
    total++;
    result = run_test_with_validation("test line\n", "\"%.S\"",
                                    "^[0-9]{2}\\.[0-9]{6} test line$", 1);
    if (result.passed) {
        printf("PASS: %s\n", "Subsecond format");
        passed++;
    } else {
        printf("FAIL: %s - %s\n", "Subsecond format", result.error_msg);
        if (result.error_msg) free(result.error_msg);
    }

    // Test 4: Unix timestamp format - should be numeric
    total++;
    result = run_test_with_validation("test line\n", "\"%s\"",
                                    "^[0-9]{10,} test line$", 1);
    if (result.passed) {
        printf("PASS: %s\n", "Unix timestamp");
        passed++;
    } else {
        printf("FAIL: %s - %s\n", "Unix timestamp", result.error_msg);
        if (result.error_msg) free(result.error_msg);
    }

    // Test 5: Relative mode with Unix timestamp - should show relative time
    total++;
    result = run_test_with_validation("1755921813 test\n", "-r",
                                    ".*ago test$", 1);
    if (result.passed) {
        printf("PASS: %s\n", "Relative mode Unix");
        passed++;
    } else {
        printf("FAIL: %s - %s\n", "Relative mode Unix", result.error_msg);
        if (result.error_msg) free(result.error_msg);
    }

    // Test 6: Relative mode with syslog format - should show relative time
    total++;
    result = run_test_with_validation("Dec 22 22:25:23 test\n", "-r",
                                    ".*ago test$", 1);
    if (result.passed) {
        printf("PASS: %s\n", "Relative mode syslog");
        passed++;
    } else {
        printf("FAIL: %s - %s\n", "Relative mode syslog", result.error_msg);
        if (result.error_msg) free(result.error_msg);
    }

    // Test 7: Relative mode ISO-8601 - should show relative time
    total++;
    result = run_test_with_validation("2025-12-22T22:25:23.123Z test\n", "-r",
                                    ".*ago.*test$", 1);
    if (result.passed) {
        printf("PASS: %s\n", "Relative mode ISO-8601");
        passed++;
    } else {
        printf("FAIL: %s - %s\n", "Relative mode ISO-8601", result.error_msg);
        if (result.error_msg) free(result.error_msg);
    }

    // Test 8: Relative mode ISO-8601 with timezone - should show relative time
    total++;
    result = run_test_with_validation("2025-09-05T10:10:09-0500 verbose\n", "-r",
                                    ".*ago verbose$", 1);
    if (result.passed) {
        printf("PASS: %s\n", "Relative mode ISO-8601 timezone");
        passed++;
    } else {
        printf("FAIL: %s - %s\n", "Relative mode ISO-8601 timezone", result.error_msg);
        if (result.error_msg) free(result.error_msg);
    }

    // Test 9: Relative mode ISO-8601 with fractional seconds and timezone - should show relative time
    total++;
    result = run_test_with_validation("2025-09-05T10:10:10.124456-0500 verbose 2\n", "-r",
                                    ".*ago verbose 2$", 1);
    if (result.passed) {
        printf("PASS: %s\n", "Relative mode ISO-8601 fractional timezone");
        passed++;
    } else {
        printf("FAIL: %s - %s\n", "Relative mode ISO-8601 fractional timezone", result.error_msg);
        if (result.error_msg) free(result.error_msg);
    }

    // Test 10: Relative mode ISO-8601 with fractional seconds (no timezone) - should show relative time
    total++;
    result = run_test_with_validation("2025-09-05T10:10:10.500000 verbose 2\n", "-r",
                                    ".*ago verbose 2$", 1);
    if (result.passed) {
        printf("PASS: %s\n", "Relative mode ISO-8601 fractional");
        passed++;
    } else {
        printf("FAIL: %s - %s\n", "Relative mode ISO-8601 fractional", result.error_msg);
        if (result.error_msg) free(result.error_msg);
    }

    // Test 11: Relative mode RFC - should show relative time
    total++;
    result = run_test_with_validation("16 Jun 94 07:29:35 test\n", "-r",
                                    ".*ago test$", 1);
    if (result.passed) {
        printf("PASS: %s\n", "Relative mode RFC");
        passed++;
    } else {
        printf("FAIL: %s - %s\n", "Relative mode RFC", result.error_msg);
        if (result.error_msg) free(result.error_msg);
    }

    // Test 12: Relative mode lastlog - should show relative time
    total++;
    result = run_test_with_validation("Mon Dec 22 22:25 test\n", "-r",
                                    ".*ago test$", 1);
    if (result.passed) {
        printf("PASS: %s\n", "Relative mode lastlog");
        passed++;
    } else {
        printf("FAIL: %s - %s\n", "Relative mode lastlog", result.error_msg);
        if (result.error_msg) free(result.error_msg);
    }

    // Test 13: Relative mode short - should show relative time
    total++;
    result = run_test_with_validation("22 dec 17:05 test\n", "-r",
                                    ".*ago test$", 1);
    if (result.passed) {
        printf("PASS: %s\n", "Relative mode short");
        passed++;
    } else {
        printf("FAIL: %s - %s\n", "Relative mode short", result.error_msg);
        if (result.error_msg) free(result.error_msg);
    }

    // Test 14: Relative mode short with year - should show relative time
    total++;
    result = run_test_with_validation("22 dec/93 17:05:30 test\n", "-r",
                                    ".*ago test$", 1);
    if (result.passed) {
        printf("PASS: %s\n", "Relative mode short_with_year");
        passed++;
    } else {
        printf("FAIL: %s - %s\n", "Relative mode short_with_year", result.error_msg);
        if (result.error_msg) free(result.error_msg);
    }

    // Test 15: Relative mode unix fractional - should show relative time
    total++;
    result = run_test_with_validation("1755921813.123456 test\n", "-r",
                                    ".*ago test$", 1);
    if (result.passed) {
        printf("PASS: %s\n", "Relative mode unix_fractional");
        passed++;
    } else {
        printf("FAIL: %s - %s\n", "Relative mode unix_fractional", result.error_msg);
        if (result.error_msg) free(result.error_msg);
    }

    // Test 16: Unique mode - should filter duplicates (only 2 lines output)
    total++;
    result = run_test_with_validation("same\nsame\ndifferent\n", "-u",
                                    NULL, 2);
    if (result.passed) {
        printf("PASS: %s\n", "Unique mode");
        passed++;
    } else {
        printf("FAIL: %s - %s\n", "Unique mode", result.error_msg);
        if (result.error_msg) free(result.error_msg);
    }

    // Test 17: Incremental mode - should show time differences
    total++;
    result = run_test_with_validation("line1\nline2\n", "-i",
                                    "^[0-9]{2}:[0-9]{2}:[0-9]{2} line[12]$", 2);
    if (result.passed) {
        printf("PASS: %s\n", "Incremental mode");
        passed++;
    } else {
        printf("FAIL: %s - %s\n", "Incremental mode", result.error_msg);
        if (result.error_msg) free(result.error_msg);
    }

    // Test 18: Since start mode - should show elapsed time
    total++;
    result = run_test_with_validation("line1\nline2\n", "-s",
                                    "^[0-9]{2}:[0-9]{2}:[0-9]{2} line[12]$", 2);
    if (result.passed) {
        printf("PASS: %s\n", "Since start mode");
        passed++;
    } else {
        printf("FAIL: %s - %s\n", "Since start mode", result.error_msg);
        if (result.error_msg) free(result.error_msg);
    }

    // Test 19: Monotonic clock - should produce timestamp
    total++;
    result = run_test_with_validation("test line\n", "-m",
                                    "^[A-Za-z]{3} [0-9]{1,2} [0-9]{2}:[0-9]{2}:[0-9]{2} test line$", 1);
    if (result.passed) {
        printf("PASS: %s\n", "Monotonic clock");
        passed++;
    } else {
        printf("FAIL: %s - %s\n", "Monotonic clock", result.error_msg);
        if (result.error_msg) free(result.error_msg);
    }

    // Test 20: Mixed format - should match complex format
    total++;
    result = run_test_with_validation("test line\n", "\"%Y%m%d-%H%M%S.%.S\"",
                                    "^[0-9]{8}-[0-9]{6}\\.[0-9]{2}\\.[0-9]{6} test line$", 1);
    if (result.passed) {
        printf("PASS: %s\n", "Mixed format");
        passed++;
    } else {
        printf("FAIL: %s - %s\n", "Mixed format", result.error_msg);
        if (result.error_msg) free(result.error_msg);
    }

    // Test 21: Line without timestamp in relative mode - should pass through unchanged
    total++;
    result = run_test_with_validation("no timestamp here\n", "-r",
                                    "^no timestamp here$", 1);
    if (result.passed) {
        printf("PASS: %s\n", "No timestamp in relative");
        passed++;
    } else {
        printf("FAIL: %s - %s\n", "No timestamp in relative", result.error_msg);
        if (result.error_msg) free(result.error_msg);
    }

    printf("\nResults: %d/%d tests passed\n", passed, total);

    if (passed == total) {
        printf("All tests passed! 🎉\n");
        return 0;
    } else {
        printf("Some tests failed! ❌\n");
        return 1;
    }
}