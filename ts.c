/*
 * ts - timestamp tool
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
 */
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

// Compile-time assertions for portability
#ifdef HAVE_64BIT_TIME_T
_Static_assert(sizeof(time_t) >= 8, "time_t must be at least 64 bits");
#endif

#ifdef HAVE_64BIT_LONG
_Static_assert(sizeof(long) >= 8, "long must be at least 64 bits");
#endif

// Configuration constants
#define MAX_LINE_LENGTH 4096
#define MAX_FORMAT_LENGTH 256
#define MAX_TIMESTAMP_LENGTH 128
#define MAX_TIME_STR_LENGTH 64
#define SECONDS_PER_MINUTE 60
#define SECONDS_PER_HOUR 3600
#define SECONDS_PER_DAY 86400
#define NANOSECONDS_PER_SECOND 1000000000L
#define MICROSECONDS_PER_SECOND 1000000L
#define FUTURE_THRESHOLD_DAYS 30

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

// Timestamp format patterns for detection
typedef struct {
    const char *pattern;
    const char *format;
    const char *name;
} timestamp_format_t;

// Common timestamp formats
static const timestamp_format_t timestamp_formats[] = {
    // syslog format: Dec 22 22:25:23
    {"[A-Za-z]{3} [0-9]{1,2} [0-9]{2}:[0-9]{2}:[0-9]{2}", "%b %d %H:%M:%S", "syslog"},

    // ISO-8601: 2025-12-22T22:25:23.123Z
    {"[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}", "%Y-%m-%dT%H:%M:%S", "ISO-8601"},

    // RFC format: 16 Jun 94 07:29:35
    {"[0-9]{1,2} [A-Za-z]{3} [0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}", "%d %b %y %H:%M:%S", "RFC"},

    // lastlog format: Mon Dec 22 22:25
    {"[A-Za-z]{3} [A-Za-z]{3} [0-9]{2} [0-9]{2}:[0-9]{2}", "%a %b %d %H:%M", "lastlog"},

    // 21 dec 17:05 (with optional year)
    {"[0-9]{2} [a-z]{3} [0-9]{2}:[0-9]{2}", "%d %b %H:%M", "short"},

    // 22 dec/93 17:05:30 with year
    {"[0-9]{2} [a-z]{3}/[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}", "%d %b/%y %H:%M:%S", "short_with_year"},

    // Unix timestamp with fractional seconds: 1755921813.123456
    {"[0-9]{10,}\\.[0-9]{1,9}", NULL, "unix_fractional"},

    // Plain Unix timestamp: 1755921813
    {"[0-9]{10,}", NULL, "unix_plain"},

    {NULL, NULL, NULL} // terminator
};

// Safe string concatenation with bounds checking
static ts_error_t safe_strcat(char *dest, size_t dest_size, const char *src) {
    if (!dest || !src) {
        return TS_ERROR_INVALID_ARGUMENT;
    }

    size_t dest_len = strnlen(dest, dest_size);
    size_t src_len = strlen(src);

    if (dest_len + src_len >= dest_size) {
        return TS_ERROR_BUFFER_OVERFLOW;
    }

    memcpy(dest + dest_len, src, src_len + 1);
    return TS_SUCCESS;
}

// Safe string formatting with bounds checking
static ts_error_t safe_snprintf(char *dest, size_t dest_size, const char *format, ...) {
    if (!dest || !format) {
        return TS_ERROR_INVALID_ARGUMENT;
    }

    va_list args;
    va_start(args, format);
    int result = vsnprintf(dest, dest_size, format, args);
    va_end(args);

    if (result < 0 || (size_t)result >= dest_size) {
        return TS_ERROR_BUFFER_OVERFLOW;
    }

    return TS_SUCCESS;
}

// Get high-resolution timestamp
static high_res_time_t get_high_res_time(bool monotonic_mode) {
    high_res_time_t result = {0};
    struct timespec ts;

#ifdef HAVE_CLOCK_MONOTONIC
    clockid_t clock_id = monotonic_mode ? CLOCK_MONOTONIC : CLOCK_REALTIME;
#else
    clockid_t clock_id = CLOCK_REALTIME;
    if (monotonic_mode) {
        // Fallback to realtime if monotonic is not available
        fprintf(stderr, "Warning: CLOCK_MONOTONIC not available, using CLOCK_REALTIME\n");
    }
#endif

    if (clock_gettime(clock_id, &ts) == 0) {
        result.seconds = ts.tv_sec;
        result.nanoseconds = ts.tv_nsec;
    }

    return result;
}

// Parse Unix timestamp with fractional seconds
static ts_error_t parse_unix_timestamp_fractional(const char *timestamp_str, time_t *result) {
    if (!timestamp_str || !result) {
        return TS_ERROR_INVALID_ARGUMENT;
    }

    char *dot_pos = strchr(timestamp_str, '.');
    if (!dot_pos) {
        return TS_ERROR_TIME_PARSE;
    }

    // Temporarily null-terminate at the dot
    *dot_pos = '\0';

    char *endptr;
    errno = 0;
    unsigned long seconds = strtoul(timestamp_str, &endptr, 10);

    // Restore the dot
    *dot_pos = '.';

    if (errno == ERANGE || *endptr != '\0' || seconds == 0) {
        return TS_ERROR_TIME_PARSE;
    }

    *result = (time_t)seconds;
    return TS_SUCCESS;
}

// Parse plain Unix timestamp
static ts_error_t parse_unix_timestamp_plain(const char *timestamp_str, time_t *result) {
    if (!timestamp_str || !result) {
        return TS_ERROR_INVALID_ARGUMENT;
    }

    char *endptr;
    errno = 0;
    unsigned long seconds = strtoul(timestamp_str, &endptr, 10);

    if (errno == ERANGE || *endptr != '\0' || seconds == 0) {
        return TS_ERROR_TIME_PARSE;
    }

    *result = (time_t)seconds;
    return TS_SUCCESS;
}

// Parse timestamp using strptime
static ts_error_t parse_timestamp_strptime(const char *timestamp_str, const char *format, time_t *result) {
    if (!timestamp_str || !format || !result) {
        return TS_ERROR_INVALID_ARGUMENT;
    }

    struct tm tm_info = {0};
    char *parse_result = strptime(timestamp_str, format, &tm_info);

    if (!parse_result) {
        return TS_ERROR_TIME_PARSE;
    }

    // Set default values for missing fields
    if (tm_info.tm_year == 0) {
        time_t now = time(NULL);
        if (now == (time_t)-1) {
            return TS_ERROR_SYSTEM;
        }
        struct tm *now_tm = localtime(&now);
        if (!now_tm) {
            return TS_ERROR_SYSTEM;
        }
        tm_info.tm_year = now_tm->tm_year;
    }

    if (tm_info.tm_mon == 0) {
        tm_info.tm_mon = 0; // January
    }

    if (tm_info.tm_mday == 0) {
        tm_info.tm_mday = 1; // First day
    }

    *result = mktime(&tm_info);
    if (*result == (time_t)-1) {
        return TS_ERROR_TIME_PARSE;
    }

    // Check if the parsed time is in the future (likely wrong year assumption)
    time_t now = time(NULL);
    if (now != (time_t)-1 && *result > now + SECONDS_PER_DAY * FUTURE_THRESHOLD_DAYS) {
        // Try with previous year
        tm_info.tm_year--;
        *result = mktime(&tm_info);
        if (*result == (time_t)-1) {
            return TS_ERROR_TIME_PARSE;
        }
    }

    return TS_SUCCESS;
}

// Detect and parse timestamp in a line
static ts_error_t parse_timestamp_in_line(const char *line, time_t *result) {
    if (!line || !result) {
        return TS_ERROR_INVALID_ARGUMENT;
    }

    regex_t regex;
    regmatch_t matches[1];
    char timestamp_str[MAX_TIMESTAMP_LENGTH];

    // Try each format pattern
    for (int i = 0; timestamp_formats[i].pattern != NULL; i++) {
        int compile_result = regcomp(&regex, timestamp_formats[i].pattern, REG_EXTENDED);
        if (compile_result != 0) {
            continue; // Skip invalid regex
        }

        int exec_result = regexec(&regex, line, 1, matches, 0);
        if (exec_result == 0) {
            // Found a match, extract the timestamp
            size_t len = matches[0].rm_eo - matches[0].rm_so;
            if (len >= MAX_TIMESTAMP_LENGTH) {
                regfree(&regex);
                continue; // Timestamp too long
            }

            memcpy(timestamp_str, line + matches[0].rm_so, len);
            timestamp_str[len] = '\0';

            ts_error_t parse_result = TS_ERROR_TIME_PARSE;

            // Handle Unix timestamp patterns specially
            if (strcmp(timestamp_formats[i].name, "unix_fractional") == 0) {
                parse_result = parse_unix_timestamp_fractional(timestamp_str, result);
            } else if (strcmp(timestamp_formats[i].name, "unix_plain") == 0) {
                parse_result = parse_unix_timestamp_plain(timestamp_str, result);
            } else if (timestamp_formats[i].format != NULL) {
                parse_result = parse_timestamp_strptime(timestamp_str, timestamp_formats[i].format, result);
            }

            regfree(&regex);

            if (parse_result == TS_SUCCESS) {
                return TS_SUCCESS;
            }
        } else {
            regfree(&regex);
        }
    }

    return TS_ERROR_TIME_PARSE; // No valid timestamp found
}

// Find the leftmost timestamp match in a line
static ts_error_t find_timestamp_match(const char *line, int *start_pos, int *end_pos) {
    if (!line || !start_pos || !end_pos) {
        return TS_ERROR_INVALID_ARGUMENT;
    }

    regex_t regex;
    regmatch_t matches[1];
    int best_match_start = -1;
    int best_match_end = -1;

    // Try each format pattern to find the timestamp
    for (int i = 0; timestamp_formats[i].pattern != NULL; i++) {
        int compile_result = regcomp(&regex, timestamp_formats[i].pattern, REG_EXTENDED);
        if (compile_result != 0) {
            continue;
        }

        int exec_result = regexec(&regex, line, 1, matches, 0);
        if (exec_result == 0) {
            // Found a match, check if it's the leftmost one
            if (best_match_start == -1 || matches[0].rm_so < best_match_start) {
                best_match_start = matches[0].rm_so;
                best_match_end = matches[0].rm_eo;
            }
        }
        regfree(&regex);
    }

    if (best_match_start != -1) {
        *start_pos = best_match_start;
        *end_pos = best_match_end;
        return TS_SUCCESS;
    }

    return TS_ERROR_TIME_PARSE; // No timestamp found
}

// Replace timestamp in a line with a new formatted timestamp
static ts_error_t replace_timestamp_in_line(char *output, size_t output_size,
                                          const char *line, const char *new_timestamp) {
    if (!output || !line || !new_timestamp) {
        return TS_ERROR_INVALID_ARGUMENT;
    }

    int start_pos, end_pos;
    ts_error_t find_result = find_timestamp_match(line, &start_pos, &end_pos);

    if (find_result == TS_SUCCESS) {
        // Found a match, replace it
        size_t before_len = start_pos;
        size_t after_start = end_pos;
        size_t new_timestamp_len = strlen(new_timestamp);
        size_t after_len = strlen(line + after_start);

        // Check if the result would fit
        if (before_len + new_timestamp_len + after_len >= output_size) {
            return TS_ERROR_BUFFER_OVERFLOW;
        }

        // Copy the part before the timestamp
        memcpy(output, line, before_len);
        output[before_len] = '\0';

        // Add the new timestamp
        memcpy(output + before_len, new_timestamp, new_timestamp_len);
        output[before_len + new_timestamp_len] = '\0';

        // Add the part after the timestamp
        memcpy(output + before_len + new_timestamp_len, line + after_start, after_len + 1);
    } else {
        // No timestamp found, just copy the line
        size_t line_len = strlen(line);
        if (line_len >= output_size) {
            return TS_ERROR_BUFFER_OVERFLOW;
        }
        memcpy(output, line, line_len + 1);
    }

    return TS_SUCCESS;
}

// Format time difference as "X ago" or "in X"
static ts_error_t format_relative_time(char *buffer, size_t buffer_size, time_t timestamp) {
    if (!buffer) {
        return TS_ERROR_INVALID_ARGUMENT;
    }

    time_t now = time(NULL);
    if (now == (time_t)-1) {
        return TS_ERROR_SYSTEM;
    }

    time_t diff = now - timestamp;

    if (diff < 0) {
        // Future time
        diff = -diff;
        if (diff < SECONDS_PER_MINUTE) {
            return safe_snprintf(buffer, buffer_size, "in %lds", diff);
        } else if (diff < SECONDS_PER_HOUR) {
            long minutes = diff / SECONDS_PER_MINUTE;
            long seconds = diff % SECONDS_PER_MINUTE;
            if (seconds > 0) {
                return safe_snprintf(buffer, buffer_size, "in %ldm%lds", minutes, seconds);
            } else {
                return safe_snprintf(buffer, buffer_size, "in %ldm", minutes);
            }
        } else if (diff < SECONDS_PER_DAY) {
            long hours = diff / SECONDS_PER_HOUR;
            long minutes = (diff % SECONDS_PER_HOUR) / SECONDS_PER_MINUTE;
            if (minutes > 0) {
                return safe_snprintf(buffer, buffer_size, "in %ldh%ldm", hours, minutes);
            } else {
                return safe_snprintf(buffer, buffer_size, "in %ldh", hours);
            }
        } else {
            long days = diff / SECONDS_PER_DAY;
            long hours = (diff % SECONDS_PER_DAY) / SECONDS_PER_HOUR;
            if (hours > 0) {
                return safe_snprintf(buffer, buffer_size, "in %ldd%ldh", days, hours);
            } else {
                return safe_snprintf(buffer, buffer_size, "in %ldd", days);
            }
        }
    } else {
        // Past time
        if (diff < SECONDS_PER_MINUTE) {
            return safe_snprintf(buffer, buffer_size, "%lds ago", diff);
        } else if (diff < SECONDS_PER_HOUR) {
            long minutes = diff / SECONDS_PER_MINUTE;
            long seconds = diff % SECONDS_PER_MINUTE;
            if (seconds > 0) {
                return safe_snprintf(buffer, buffer_size, "%ldm%lds ago", minutes, seconds);
            } else {
                return safe_snprintf(buffer, buffer_size, "%ldm ago", minutes);
            }
        } else if (diff < SECONDS_PER_DAY) {
            long hours = diff / SECONDS_PER_HOUR;
            long minutes = (diff % SECONDS_PER_HOUR) / SECONDS_PER_MINUTE;
            if (minutes > 0) {
                return safe_snprintf(buffer, buffer_size, "%ldh%ldm ago", hours, minutes);
            } else {
                return safe_snprintf(buffer, buffer_size, "%ldh ago", hours);
            }
        } else {
            long days = diff / SECONDS_PER_DAY;
            long hours = (diff % SECONDS_PER_DAY) / SECONDS_PER_HOUR;
            if (hours > 0) {
                return safe_snprintf(buffer, buffer_size, "%ldd%ldh ago", days, hours);
            } else {
                return safe_snprintf(buffer, buffer_size, "%ldd ago", days);
            }
        }
    }
}

// Format timestamp with subsecond resolution
static ts_error_t format_timestamp_with_subsecond(char *buffer, size_t buffer_size,
                                                 const char *format, const high_res_time_t *timestamp) {
    if (!buffer || !format || !timestamp) {
        return TS_ERROR_INVALID_ARGUMENT;
    }

    char temp_buffer[MAX_FORMAT_LENGTH];
    struct tm *tm_info = localtime(&timestamp->seconds);
    if (!tm_info) {
        return TS_ERROR_SYSTEM;
    }

    // First pass: handle special patterns and build the result
    char result[MAX_FORMAT_LENGTH] = "";
    const char *current = format;

    while (*current != '\0') {
        if (strncmp(current, "%.S", 3) == 0) {
            // %.S - seconds with subsecond resolution
            ts_error_t result_code = safe_snprintf(result + strlen(result),
                                                 sizeof(result) - strlen(result),
                                                 "%02d.%06ld", tm_info->tm_sec,
                                                 timestamp->nanoseconds / 1000);
            if (result_code != TS_SUCCESS) {
                return result_code;
            }
            current += 3;
        } else if (strncmp(current, "%.s", 3) == 0) {
            // %.s - unix timestamp with subsecond resolution
            ts_error_t result_code = safe_snprintf(result + strlen(result),
                                                 sizeof(result) - strlen(result),
                                                 "%ld.%06ld", timestamp->seconds,
                                                 timestamp->nanoseconds / 1000);
            if (result_code != TS_SUCCESS) {
                return result_code;
            }
            current += 3;
        } else if (strncmp(current, "%.T", 3) == 0) {
            // %.T - time with subsecond resolution
            char time_str[MAX_TIME_STR_LENGTH];
            if (strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info) == 0) {
                return TS_ERROR_BUFFER_OVERFLOW;
            }
            ts_error_t result_code = safe_snprintf(result + strlen(result),
                                                 sizeof(result) - strlen(result),
                                                 "%s.%06ld", time_str,
                                                 timestamp->nanoseconds / 1000);
            if (result_code != TS_SUCCESS) {
                return result_code;
            }
            current += 3;
        } else if (strncmp(current, "%N", 2) == 0) {
            // %N - nanoseconds
            ts_error_t result_code = safe_snprintf(result + strlen(result),
                                                 sizeof(result) - strlen(result),
                                                 "%09ld", timestamp->nanoseconds);
            if (result_code != TS_SUCCESS) {
                return result_code;
            }
            current += 2;
        } else if (strncmp(current, "%s", 2) == 0) {
            // %s - unix timestamp
            ts_error_t result_code = safe_snprintf(result + strlen(result),
                                                 sizeof(result) - strlen(result),
                                                 "%ld", timestamp->seconds);
            if (result_code != TS_SUCCESS) {
                return result_code;
            }
            current += 2;
        } else {
            // Copy the character and continue
            char temp[2] = {*current, '\0'};
            ts_error_t result_code = safe_strcat(result, sizeof(result), temp);
            if (result_code != TS_SUCCESS) {
                return result_code;
            }
            current++;
        }
    }

    // Second pass: if the result still contains strftime patterns, process them
    if (strstr(result, "%") != NULL) {
        if (strftime(temp_buffer, sizeof(temp_buffer), result, tm_info) == 0) {
            return TS_ERROR_BUFFER_OVERFLOW;
        }
        size_t temp_len = strlen(temp_buffer);
        if (temp_len >= buffer_size) {
            return TS_ERROR_BUFFER_OVERFLOW;
        }
        memcpy(buffer, temp_buffer, temp_len + 1);
    } else {
        size_t result_len = strlen(result);
        if (result_len >= buffer_size) {
            return TS_ERROR_BUFFER_OVERFLOW;
        }
        memcpy(buffer, result, result_len + 1);
    }

    return TS_SUCCESS;
}

// Process a single line with timestamp
static ts_error_t process_line(const char *line, const char *format,
                              const high_res_time_t *current_time) {
    if (!line || !format || !current_time) {
        return TS_ERROR_INVALID_ARGUMENT;
    }

    char timestamp[MAX_FORMAT_LENGTH];
    ts_error_t result = format_timestamp_with_subsecond(timestamp, sizeof(timestamp),
                                                       format, current_time);
    if (result != TS_SUCCESS) {
        return result;
    }

    printf("%s %s", timestamp, line);
    return TS_SUCCESS;
}

// Print usage information
static void print_usage(const char *program_name) {
    fprintf(stderr, "Usage: %s [-r] [-i | -s] [-m] [-u] [format]\n", program_name);
    fprintf(stderr, "Add timestamps to the beginning of each line of input.\n");
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  -r    Convert existing timestamps to relative times\n");
    fprintf(stderr, "  -i    Report incremental timestamps (time since last timestamp)\n");
    fprintf(stderr, "  -s    Report incremental timestamps (time since start)\n");
    fprintf(stderr, "  -m    Use monotonic clock\n");
    fprintf(stderr, "  -u    Only output lines that are unique (different from previous line)\n");
    fprintf(stderr, "  -h    Show this help message\n");
    fprintf(stderr, "\nFormat is a strftime format string. Default: \"%%b %%d %%H:%%M:%%S\"\n");
    fprintf(stderr, "Special extensions:\n");
    fprintf(stderr, "  %%.S    seconds with subsecond resolution\n");
    fprintf(stderr, "  %%.s    unix timestamp with subsecond resolution\n");
    fprintf(stderr, "  %%.T    time with subsecond resolution\n");
    fprintf(stderr, "  %%s     unix time_t (compatible with date command)\n");
    fprintf(stderr, "  %%N     nanoseconds (compatible with date command)\n");
}

// Main function
int main(int argc, char *argv[]) {
    char line[MAX_LINE_LENGTH];
    char format[MAX_FORMAT_LENGTH] = "%b %d %H:%M:%S";
    int opt;
    bool relative_mode = false;
    bool incremental_mode = false;
    bool since_start_mode = false;
    bool monotonic_mode = false;
    bool unique_mode = false;
    high_res_time_t start_time;
    high_res_time_t last_time;
    char last_line[MAX_LINE_LENGTH] = "";

    // Parse command line options
    while ((opt = getopt(argc, argv, "rismuh")) != -1) {
        switch (opt) {
            case 'r':
                relative_mode = true;
                break;
            case 'i':
                incremental_mode = true;
                strncpy(format, "%H:%M:%S", sizeof(format) - 1);
                format[sizeof(format) - 1] = '\0';
                break;
            case 's':
                since_start_mode = true;
                strncpy(format, "%H:%M:%S", sizeof(format) - 1);
                format[sizeof(format) - 1] = '\0';
                break;
            case 'm':
                monotonic_mode = true;
                break;
            case 'u':
                unique_mode = true;
                break;
            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    // Check for format argument
    if (optind < argc) {
        strncpy(format, argv[optind], sizeof(format) - 1);
        format[sizeof(format) - 1] = '\0';
    }

    // Initialize timing
    start_time = get_high_res_time(monotonic_mode);
    last_time = start_time;

    // Process input line by line
    while (fgets(line, sizeof(line), stdin)) {
        // Check if line is unique (different from previous line)
        if (unique_mode && strcmp(line, last_line) == 0) {
            continue; // Skip duplicate lines
        }

        high_res_time_t current_time = get_high_res_time(monotonic_mode);

        if (relative_mode) {
            // Parse existing timestamp in the line
            time_t parsed_time;
            ts_error_t parse_result = parse_timestamp_in_line(line, &parsed_time);

            if (parse_result == TS_SUCCESS) {
                // Found a timestamp
                if (optind < argc) {
                    // Custom format specified, convert to that format
                    char formatted_time[MAX_FORMAT_LENGTH];
                    char replaced_line[MAX_LINE_LENGTH];
                    struct tm *tm_info = localtime(&parsed_time);
                    if (!tm_info) {
                        fprintf(stderr, "Error: Failed to convert timestamp\n");
                        continue;
                    }
                    if (strftime(formatted_time, sizeof(formatted_time), format, tm_info) == 0) {
                        fprintf(stderr, "Error: Format string too long\n");
                        continue;
                    }
                    ts_error_t replace_result = replace_timestamp_in_line(replaced_line,
                                                                        sizeof(replaced_line),
                                                                        line, formatted_time);
                    if (replace_result == TS_SUCCESS) {
                        printf("%s", replaced_line);
                    } else {
                        fprintf(stderr, "Error: Failed to replace timestamp\n");
                        printf("%s", line);
                    }
                } else {
                    // No format specified, convert to relative time
                    char relative_time[MAX_FORMAT_LENGTH];
                    char replaced_line[MAX_LINE_LENGTH];
                    ts_error_t format_result = format_relative_time(relative_time,
                                                                  sizeof(relative_time),
                                                                  parsed_time);
                    if (format_result == TS_SUCCESS) {
                        ts_error_t replace_result = replace_timestamp_in_line(replaced_line,
                                                                            sizeof(replaced_line),
                                                                            line, relative_time);
                        if (replace_result == TS_SUCCESS) {
                            printf("%s", replaced_line);
                        } else {
                            fprintf(stderr, "Error: Failed to replace timestamp\n");
                            printf("%s", line);
                        }
                    } else {
                        fprintf(stderr, "Error: Failed to format relative time\n");
                        printf("%s", line);
                    }
                }
            } else {
                // No timestamp found, pass through the line
                printf("%s", line);
            }
        } else if (incremental_mode) {
            // Time since last timestamp
            long diff_sec = current_time.seconds - last_time.seconds;
            long diff_nsec = current_time.nanoseconds - last_time.nanoseconds;

            if (diff_nsec < 0) {
                diff_sec--;
                diff_nsec += NANOSECONDS_PER_SECOND;
            }

            char timestamp[MAX_FORMAT_LENGTH];
            ts_error_t format_result = TS_ERROR_BUFFER_OVERFLOW;

            if (strstr(format, "%.s") != NULL) {
                format_result = safe_snprintf(timestamp, sizeof(timestamp),
                                            "%ld.%06ld", diff_sec, diff_nsec / 1000);
            } else if (strstr(format, "%.S") != NULL) {
                long total_seconds = diff_sec % 60;
                format_result = safe_snprintf(timestamp, sizeof(timestamp),
                                            "%02ld.%06ld", total_seconds, diff_nsec / 1000);
            } else if (strstr(format, "%.T") != NULL) {
                long hours = diff_sec / SECONDS_PER_HOUR;
                long minutes = (diff_sec % SECONDS_PER_HOUR) / SECONDS_PER_MINUTE;
                long seconds = diff_sec % SECONDS_PER_MINUTE;
                format_result = safe_snprintf(timestamp, sizeof(timestamp),
                                            "%02ld:%02ld:%02ld.%06ld", hours, minutes,
                                            seconds, diff_nsec / 1000);
            } else {
                struct tm *tm_info = gmtime(&diff_sec);
                if (tm_info && strftime(timestamp, sizeof(timestamp), format, tm_info) > 0) {
                    format_result = TS_SUCCESS;
                }
            }

            if (format_result == TS_SUCCESS) {
                printf("%s %s", timestamp, line);
            } else {
                fprintf(stderr, "Error: Failed to format timestamp\n");
                printf("%s", line);
            }

            if (unique_mode) {
                strncpy(last_line, line, sizeof(last_line) - 1);
                last_line[sizeof(last_line) - 1] = '\0';
            }
            last_time = current_time;
        } else if (since_start_mode) {
            // Time since start
            long diff_sec = current_time.seconds - start_time.seconds;
            long diff_nsec = current_time.nanoseconds - start_time.nanoseconds;

            if (diff_nsec < 0) {
                diff_sec--;
                diff_nsec += NANOSECONDS_PER_SECOND;
            }

            char timestamp[MAX_FORMAT_LENGTH];
            ts_error_t format_result = TS_ERROR_BUFFER_OVERFLOW;

            if (strstr(format, "%.s") != NULL) {
                format_result = safe_snprintf(timestamp, sizeof(timestamp),
                                            "%ld.%06ld", diff_sec, diff_nsec / 1000);
            } else if (strstr(format, "%.S") != NULL) {
                long total_seconds = diff_sec % 60;
                format_result = safe_snprintf(timestamp, sizeof(timestamp),
                                            "%02ld.%06ld", total_seconds, diff_nsec / 1000);
            } else if (strstr(format, "%.T") != NULL) {
                long hours = diff_sec / SECONDS_PER_HOUR;
                long minutes = (diff_sec % SECONDS_PER_HOUR) / SECONDS_PER_MINUTE;
                long seconds = diff_sec % SECONDS_PER_MINUTE;
                format_result = safe_snprintf(timestamp, sizeof(timestamp),
                                            "%02ld:%02ld:%02ld.%06ld", hours, minutes,
                                            seconds, diff_nsec / 1000);
            } else {
                struct tm *tm_info = gmtime(&diff_sec);
                if (tm_info && strftime(timestamp, sizeof(timestamp), format, tm_info) > 0) {
                    format_result = TS_SUCCESS;
                }
            }

            if (format_result == TS_SUCCESS) {
                printf("%s %s", timestamp, line);
            } else {
                fprintf(stderr, "Error: Failed to format timestamp\n");
                printf("%s", line);
            }

            if (unique_mode) {
                strncpy(last_line, line, sizeof(last_line) - 1);
                last_line[sizeof(last_line) - 1] = '\0';
            }
        } else {
            // Default absolute timestamp mode
            ts_error_t result = process_line(line, format, &current_time);
            if (result != TS_SUCCESS) {
                fprintf(stderr, "Error: Failed to process line\n");
                printf("%s", line);
            }
            if (unique_mode) {
                strncpy(last_line, line, sizeof(last_line) - 1);
                last_line[sizeof(last_line) - 1] = '\0';
            }
        }
    }

    return EXIT_SUCCESS;
}