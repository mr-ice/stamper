/*
 * test_ts_simple.c - Simple coverage test runner for ts
 *
 * Copyright (C) 2025  Michael Rice <michael@riceclan.org>
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
#include <assert.h>

// Define TS_TESTING to make functions available for testing
#define TS_TESTING

// Include the ts.c source directly for coverage testing
// We need to define main as something else to avoid conflicts
#define main ts_main
#include "ts.c"
#undef main

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

// Test the safe_strcat function
static test_result_t test_safe_strcat() {
    test_result_t result = {false, NULL};
    
    char dest[100] = "hello";
    const char *src = " world";
    
    // Test normal concatenation
    ts_error_t ret = safe_strcat(dest, sizeof(dest), src);
    if (ret != TS_SUCCESS) {
        result.error_msg = "safe_strcat failed on normal concatenation";
        return result;
    }
    
    if (strcmp(dest, "hello world") != 0) {
        result.error_msg = "safe_strcat produced wrong result";
        return result;
    }
    
    // Test buffer overflow
    char small_dest[5] = "hi";
    ret = safe_strcat(small_dest, sizeof(small_dest), " there");
    if (ret != TS_ERROR_BUFFER_OVERFLOW) {
        result.error_msg = "safe_strcat should have detected buffer overflow";
        return result;
    }
    
    // Test NULL arguments
    ret = safe_strcat(NULL, 100, "test");
    if (ret != TS_ERROR_INVALID_ARGUMENT) {
        result.error_msg = "safe_strcat should have detected NULL dest";
        return result;
    }
    
    ret = safe_strcat(dest, sizeof(dest), NULL);
    if (ret != TS_ERROR_INVALID_ARGUMENT) {
        result.error_msg = "safe_strcat should have detected NULL src";
        return result;
    }
    
    result.passed = true;
    return result;
}

// Test the safe_snprintf function
static test_result_t test_safe_snprintf() {
    test_result_t result = {false, NULL};
    
    char buffer[100];
    
    // Test normal formatting
    ts_error_t ret = safe_snprintf(buffer, sizeof(buffer), "Hello %s %d", "World", 42);
    if (ret != TS_SUCCESS) {
        result.error_msg = "safe_snprintf failed on normal formatting";
        return result;
    }
    
    if (strcmp(buffer, "Hello World 42") != 0) {
        result.error_msg = "safe_snprintf produced wrong result";
        return result;
    }
    
    // Test buffer overflow
    char small_buffer[5];
    ret = safe_snprintf(small_buffer, sizeof(small_buffer), "This is a very long string");
    if (ret != TS_ERROR_BUFFER_OVERFLOW) {
        result.error_msg = "safe_snprintf should have detected buffer overflow";
        return result;
    }
    
    // Test NULL arguments
    ret = safe_snprintf(NULL, 100, "test");
    if (ret != TS_ERROR_INVALID_ARGUMENT) {
        result.error_msg = "safe_snprintf should have detected NULL buffer";
        return result;
    }
    
    ret = safe_snprintf(buffer, sizeof(buffer), NULL);
    if (ret != TS_ERROR_INVALID_ARGUMENT) {
        result.error_msg = "safe_snprintf should have detected NULL format";
        return result;
    }
    
    result.passed = true;
    return result;
}

// Test the get_high_res_time function
static test_result_t test_get_high_res_time() {
    test_result_t result = {false, NULL};
    
    // Test realtime clock
    high_res_time_t time1 = get_high_res_time(false);
    if (time1.seconds == 0 && time1.nanoseconds == 0) {
        result.error_msg = "get_high_res_time failed to get realtime";
        return result;
    }
    
    // Test monotonic clock
    high_res_time_t time2 = get_high_res_time(true);
    if (time2.seconds == 0 && time2.nanoseconds == 0) {
        result.error_msg = "get_high_res_time failed to get monotonic time";
        return result;
    }
    
    result.passed = true;
    return result;
}

// Test the parse_unix_timestamp_plain function
static test_result_t test_parse_unix_timestamp_plain() {
    test_result_t result = {false, NULL};
    
    time_t parsed_time;
    
    // Test valid timestamp
    ts_error_t ret = parse_unix_timestamp_plain("1755921813", &parsed_time);
    if (ret != TS_SUCCESS) {
        result.error_msg = "parse_unix_timestamp_plain failed on valid timestamp";
        return result;
    }
    
    if (parsed_time != 1755921813) {
        result.error_msg = "parse_unix_timestamp_plain parsed wrong value";
        return result;
    }
    
    // Test invalid timestamp
    ret = parse_unix_timestamp_plain("invalid", &parsed_time);
    if (ret != TS_ERROR_TIME_PARSE) {
        result.error_msg = "parse_unix_timestamp_plain should have failed on invalid timestamp";
        return result;
    }
    
    // Test NULL arguments
    ret = parse_unix_timestamp_plain(NULL, &parsed_time);
    if (ret != TS_ERROR_INVALID_ARGUMENT) {
        result.error_msg = "parse_unix_timestamp_plain should have detected NULL string";
        return result;
    }
    
    ret = parse_unix_timestamp_plain("123", NULL);
    if (ret != TS_ERROR_INVALID_ARGUMENT) {
        result.error_msg = "parse_unix_timestamp_plain should have detected NULL result";
        return result;
    }
    
    result.passed = true;
    return result;
}

// Test the find_timestamp_match function
static test_result_t test_find_timestamp_match() {
    test_result_t result = {false, NULL};
    
    int start_pos, end_pos;
    
    // Test finding Unix timestamp
    ts_error_t ret = find_timestamp_match("1755921813 test line", &start_pos, &end_pos);
    if (ret != TS_SUCCESS) {
        result.error_msg = "find_timestamp_match failed to find Unix timestamp";
        return result;
    }
    
    if (start_pos != 0 || end_pos != 10) {
        result.error_msg = "find_timestamp_match found wrong position for Unix timestamp";
        return result;
    }
    
    // Test finding syslog timestamp
    ret = find_timestamp_match("Dec 22 22:25:23 test line", &start_pos, &end_pos);
    if (ret != TS_SUCCESS) {
        result.error_msg = "find_timestamp_match failed to find syslog timestamp";
        return result;
    }
    
    // Test line without timestamp
    ret = find_timestamp_match("no timestamp here", &start_pos, &end_pos);
    if (ret != TS_ERROR_TIME_PARSE) {
        result.error_msg = "find_timestamp_match should have failed on line without timestamp";
        return result;
    }
    
    // Test NULL arguments
    ret = find_timestamp_match(NULL, &start_pos, &end_pos);
    if (ret != TS_ERROR_INVALID_ARGUMENT) {
        result.error_msg = "find_timestamp_match should have detected NULL line";
        return result;
    }
    
    ret = find_timestamp_match("test", NULL, &end_pos);
    if (ret != TS_ERROR_INVALID_ARGUMENT) {
        result.error_msg = "find_timestamp_match should have detected NULL start_pos";
        return result;
    }
    
    result.passed = true;
    return result;
}

// Test the format_relative_time function
static test_result_t test_format_relative_time() {
    test_result_t result = {false, NULL};
    
    char buffer[100];
    time_t now = time(NULL);
    time_t past_time = now - 3600; // 1 hour ago
    time_t future_time = now + 3600; // 1 hour from now
    
    // Test past time
    ts_error_t ret = format_relative_time(buffer, sizeof(buffer), past_time, 0);
    if (ret != TS_SUCCESS) {
        result.error_msg = "format_relative_time failed on past time";
        return result;
    }
    
    if (!matches_pattern(buffer, ".*ago$")) {
        result.error_msg = "format_relative_time should have produced 'ago' format";
        return result;
    }
    
    // Test future time
    ret = format_relative_time(buffer, sizeof(buffer), future_time, 0);
    if (ret != TS_SUCCESS) {
        result.error_msg = "format_relative_time failed on future time";
        return result;
    }
    
    if (!matches_pattern(buffer, "^in .*$")) {
        result.error_msg = "format_relative_time should have produced 'in' format";
        return result;
    }
    
    // Test NULL buffer
    ret = format_relative_time(NULL, 100, now, 0);
    if (ret != TS_ERROR_INVALID_ARGUMENT) {
        result.error_msg = "format_relative_time should have detected NULL buffer";
        return result;
    }
    
    result.passed = true;
    return result;
}

// Main test runner
int main() {
    printf("Running ts simple coverage tests...\n");
    
    int passed = 0;
    int total = 0;
    test_result_t result;
    
    // Test safe_strcat
    total++;
    result = test_safe_strcat();
    if (result.passed) {
        printf("PASS: safe_strcat\n");
        passed++;
    } else {
        printf("FAIL: safe_strcat - %s\n", result.error_msg);
        if (result.error_msg) free(result.error_msg);
    }
    
    // Test safe_snprintf
    total++;
    result = test_safe_snprintf();
    if (result.passed) {
        printf("PASS: safe_snprintf\n");
        passed++;
    } else {
        printf("FAIL: safe_snprintf - %s\n", result.error_msg);
        if (result.error_msg) free(result.error_msg);
    }
    
    // Test get_high_res_time
    total++;
    result = test_get_high_res_time();
    if (result.passed) {
        printf("PASS: get_high_res_time\n");
        passed++;
    } else {
        printf("FAIL: get_high_res_time - %s\n", result.error_msg);
        if (result.error_msg) free(result.error_msg);
    }
    
    // Test parse_unix_timestamp_plain
    total++;
    result = test_parse_unix_timestamp_plain();
    if (result.passed) {
        printf("PASS: parse_unix_timestamp_plain\n");
        passed++;
    } else {
        printf("FAIL: parse_unix_timestamp_plain - %s\n", result.error_msg);
        if (result.error_msg) free(result.error_msg);
    }
    
    // Test find_timestamp_match
    total++;
    result = test_find_timestamp_match();
    if (result.passed) {
        printf("PASS: find_timestamp_match\n");
        passed++;
    } else {
        printf("FAIL: find_timestamp_match - %s\n", result.error_msg);
        if (result.error_msg) free(result.error_msg);
    }
    
    // Test format_relative_time
    total++;
    result = test_format_relative_time();
    if (result.passed) {
        printf("PASS: format_relative_time\n");
        passed++;
    } else {
        printf("FAIL: format_relative_time - %s\n", result.error_msg);
        if (result.error_msg) free(result.error_msg);
    }
    
    printf("\nResults: %d/%d tests passed\n", passed, total);
    
    if (passed == total) {
        printf("All simple coverage tests passed! 🎉\n");
        return 0;
    } else {
        printf("Some simple coverage tests failed! ❌\n");
        return 1;
    }
}
