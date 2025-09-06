/*
 * ts_test.h - Test interface for ts functions
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

#ifndef TS_TEST_H
#define TS_TEST_H

#include "config.h"

/* Feature test macros to enable POSIX functions */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/time.h>
#include <regex.h>
#include <errno.h>
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>

// Error codes
typedef enum {
    TS_SUCCESS = 0,
    TS_ERROR_INVALID_ARGUMENT = -1,
    TS_ERROR_BUFFER_OVERFLOW = -2,
    TS_ERROR_REGEX_COMPILE = -3,
    TS_ERROR_REGEX_EXEC = -4,
    TS_ERROR_TIME_PARSE = -5,
    TS_ERROR_SYSTEM = -6
} ts_error_t;

// High-resolution timestamp structure
typedef struct {
    time_t seconds;
    long nanoseconds;
} high_res_time_t;

// Function declarations for testing
ts_error_t safe_strcat(char *dest, size_t dest_size, const char *src);
ts_error_t safe_snprintf(char *dest, size_t dest_size, const char *format, ...);
high_res_time_t get_high_res_time(bool monotonic_mode);
ts_error_t parse_unix_timestamp_fractional(const char *timestamp_str, time_t *result);
ts_error_t parse_unix_timestamp_plain(const char *timestamp_str, time_t *result);
ts_error_t find_timestamp_match(const char *line, int *start_pos, int *end_pos);
ts_error_t replace_timestamp_in_line(char *output, size_t output_size,
                                   const char *line, const char *new_timestamp);
ts_error_t format_relative_time(char *buffer, size_t buffer_size, time_t timestamp, long fractional_seconds);
ts_error_t format_timestamp_with_subsecond(char *buffer, size_t buffer_size,
                                         const char *format, const high_res_time_t *timestamp);
ts_error_t process_line(const char *line, const char *format,
                       const high_res_time_t *current_time);
ts_error_t parse_timestamp_in_line_with_fractional(const char *line, time_t *result, long *fractional_seconds);

#endif /* TS_TEST_H */
