#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/time.h>
#include <regex.h>

#define MAX_LINE_LENGTH 4096
#define MAX_FORMAT_LENGTH 256
#define MAX_TIMESTAMP_LENGTH 128

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
    
    {NULL, NULL, NULL} // terminator
};

// Function to detect and parse timestamp in a line
time_t parse_timestamp_in_line(const char *line) {
    regex_t regex;
    regmatch_t matches[1];
    char timestamp_str[MAX_TIMESTAMP_LENGTH];
    struct tm tm_info = {0};
    time_t result = 0;
    
    // Try each format pattern
    for (int i = 0; timestamp_formats[i].pattern != NULL; i++) {
        if (regcomp(&regex, timestamp_formats[i].pattern, REG_EXTENDED) != 0) {
            continue; // Skip invalid regex
        }
        
        if (regexec(&regex, line, 1, matches, 0) == 0) {
            // Found a match, extract the timestamp
            int len = matches[0].rm_eo - matches[0].rm_so;
            if (len < MAX_TIMESTAMP_LENGTH) {
                strncpy(timestamp_str, line + matches[0].rm_so, len);
                timestamp_str[len] = '\0';
                
                // Try to parse with strptime
                if (strptime(timestamp_str, timestamp_formats[i].format, &tm_info) != NULL) {
                    // Set default values for missing fields
                    if (tm_info.tm_year == 0) {
                        // Assume current year if year is missing
                        time_t now = time(NULL);
                        struct tm *now_tm = localtime(&now);
                        tm_info.tm_year = now_tm->tm_year;
                    }
                    if (tm_info.tm_mon == 0) {
                        tm_info.tm_mon = 0; // January
                    }
                    if (tm_info.tm_mday == 0) {
                        tm_info.tm_mday = 1; // First day
                    }
                    
                    result = mktime(&tm_info);
                    if (result != -1) {
                        // Check if the parsed time is in the future (likely wrong year assumption)
                        time_t now = time(NULL);
                        if (result > now + 86400 * 30) { // More than 30 days in future
                            // Try with previous year
                            tm_info.tm_year--;
                            result = mktime(&tm_info);
                        }
                        regfree(&regex);
                        return result;
                    }
                }
            }
        }
        regfree(&regex);
    }
    
    return 0; // No valid timestamp found
}

// Function to replace timestamp in a line with a new formatted timestamp
void replace_timestamp_in_line(char *output, size_t output_size, const char *line, const char *new_timestamp) {
    regex_t regex;
    regmatch_t matches[1];
    int best_match_start = -1;
    int best_match_end = -1;
    
    // Try each format pattern to find the timestamp
    for (int i = 0; timestamp_formats[i].pattern != NULL; i++) {
        if (regcomp(&regex, timestamp_formats[i].pattern, REG_EXTENDED) != 0) {
            continue;
        }
        
        if (regexec(&regex, line, 1, matches, 0) == 0) {
            // Found a match, check if it's the leftmost one
            if (best_match_start == -1 || matches[0].rm_so < best_match_start) {
                best_match_start = matches[0].rm_so;
                best_match_end = matches[0].rm_eo;
            }
        }
        regfree(&regex);
    }
    
    if (best_match_start != -1) {
        // Found a match, replace it
        int before_len = best_match_start;
        int after_start = best_match_end;
        
        // Copy the part before the timestamp
        strncpy(output, line, before_len);
        output[before_len] = '\0';
        
        // Add the new timestamp
        strcat(output, new_timestamp);
        
        // Add the part after the timestamp
        strcat(output, line + after_start);
    } else {
        // No timestamp found, just copy the line
        strncpy(output, line, output_size - 1);
        output[output_size - 1] = '\0';
    }
}

// Function to format time difference as "X ago" or "in X"
void format_relative_time(char *buffer, size_t buffer_size, time_t timestamp) {
    time_t now = time(NULL);
    time_t diff = now - timestamp;
    
    if (diff < 0) {
        // Future time
        diff = -diff;
        if (diff < 60) {
            snprintf(buffer, buffer_size, "in %lds", diff);
        } else if (diff < 3600) {
            long minutes = diff / 60;
            long seconds = diff % 60;
            if (seconds > 0) {
                snprintf(buffer, buffer_size, "in %ldm%lds", minutes, seconds);
            } else {
                snprintf(buffer, buffer_size, "in %ldm", minutes);
            }
        } else if (diff < 86400) {
            long hours = diff / 3600;
            long minutes = (diff % 3600) / 60;
            if (minutes > 0) {
                snprintf(buffer, buffer_size, "in %ldh%ldm", hours, minutes);
            } else {
                snprintf(buffer, buffer_size, "in %ldh", hours);
            }
        } else {
            long days = diff / 86400;
            long hours = (diff % 86400) / 3600;
            if (hours > 0) {
                snprintf(buffer, buffer_size, "in %ldd%ldh", days, hours);
            } else {
                snprintf(buffer, buffer_size, "in %ldd", days);
            }
        }
    } else {
        // Past time
        if (diff < 60) {
            snprintf(buffer, buffer_size, "%lds ago", diff);
        } else if (diff < 3600) {
            long minutes = diff / 60;
            long seconds = diff % 60;
            if (seconds > 0) {
                snprintf(buffer, buffer_size, "%ldm%lds ago", minutes, seconds);
            } else {
                snprintf(buffer, buffer_size, "%ldm ago", minutes);
            }
        } else if (diff < 86400) {
            long hours = diff / 3600;
            long minutes = (diff % 3600) / 60;
            if (minutes > 0) {
                snprintf(buffer, buffer_size, "%ldh%ldm ago", hours, minutes);
            } else {
                snprintf(buffer, buffer_size, "%ldh ago", hours);
            }
        } else {
            long days = diff / 86400;
            long hours = (diff % 86400) / 3600;
            if (hours > 0) {
                snprintf(buffer, buffer_size, "%ldd%ldh ago", days, hours);
            } else {
                snprintf(buffer, buffer_size, "%ldd ago", days);
            }
        }
    }
}

void print_usage(const char *program_name) {
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
    fprintf(stderr, "Special extensions: %%.S (seconds with subsecond), %%.s (unix timestamp with subsecond), %%.T (time with subsecond)\n");
    fprintf(stderr, "Nanosecond support: %%N (nanoseconds, compatible with date command)\n");
}

high_res_time_t get_high_res_time(int monotonic_mode) {
    high_res_time_t result;
    struct timespec ts;
    
    if (monotonic_mode) {
        clock_gettime(CLOCK_MONOTONIC, &ts);
    } else {
        clock_gettime(CLOCK_REALTIME, &ts);
    }
    
    result.seconds = ts.tv_sec;
    result.nanoseconds = ts.tv_nsec;
    return result;
}

void format_timestamp_with_subsecond(char *buffer, size_t buffer_size, const char *format, high_res_time_t timestamp) {
    (void)buffer_size; // Suppress unused parameter warning
    char temp_buffer[MAX_FORMAT_LENGTH];
    struct tm *tm_info = localtime(&timestamp.seconds);
    
    // First pass: handle special patterns and build the result
    char result[MAX_FORMAT_LENGTH] = "";
    const char *current = format;
    
    while (*current != '\0') {
        if (strncmp(current, "%.S", 3) == 0) {
            // %.S - seconds with subsecond resolution
            snprintf(result + strlen(result), sizeof(result) - strlen(result), 
                    "%02d.%06ld", tm_info->tm_sec, timestamp.nanoseconds / 1000);
            current += 3;
        } else if (strncmp(current, "%.s", 3) == 0) {
            // %.s - unix timestamp with subsecond resolution
            snprintf(result + strlen(result), sizeof(result) - strlen(result), 
                    "%ld.%06ld", timestamp.seconds, timestamp.nanoseconds / 1000);
            current += 3;
        } else if (strncmp(current, "%.T", 3) == 0) {
            // %.T - time with subsecond resolution
            char time_str[32];
            strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);
            snprintf(result + strlen(result), sizeof(result) - strlen(result), 
                    "%s.%06ld", time_str, timestamp.nanoseconds / 1000);
            current += 3;
        } else if (strncmp(current, "%N", 2) == 0) {
            // %N - nanoseconds
            snprintf(result + strlen(result), sizeof(result) - strlen(result), 
                    "%09ld", timestamp.nanoseconds);
            current += 2;
        } else if (strncmp(current, "%s", 2) == 0) {
            // %s - unix timestamp
            snprintf(result + strlen(result), sizeof(result) - strlen(result), 
                    "%ld", timestamp.seconds);
            current += 2;
        } else {
            // Copy the character and continue
            char temp[2] = {*current, '\0'};
            strcat(result, temp);
            current++;
        }
    }
    
    // Second pass: if the result still contains strftime patterns, process them
    if (strstr(result, "%") != NULL) {
        strftime(temp_buffer, sizeof(temp_buffer), result, tm_info);
        strcpy(buffer, temp_buffer);
    } else {
        strcpy(buffer, result);
    }
}

void format_timestamp(char *buffer, size_t buffer_size, const char *format, time_t timestamp) {
    struct tm *tm_info = localtime(&timestamp);
    strftime(buffer, buffer_size, format, tm_info);
}

void process_line(const char *line, const char *format, high_res_time_t current_time) {
    char timestamp[MAX_FORMAT_LENGTH];
    format_timestamp_with_subsecond(timestamp, sizeof(timestamp), format, current_time);
    printf("%s %s", timestamp, line);
}

int main(int argc, char *argv[]) {
    char line[MAX_LINE_LENGTH];
    char format[MAX_FORMAT_LENGTH] = "%b %d %H:%M:%S";
    int opt;
    int relative_mode = 0;
    int incremental_mode = 0;
    int since_start_mode = 0;
    int monotonic_mode = 0;
    int unique_mode = 0;
    high_res_time_t start_time;
    high_res_time_t last_time;
    char last_line[MAX_LINE_LENGTH] = "";
    
    // Parse command line options
    while ((opt = getopt(argc, argv, "rismuh")) != -1) {
        switch (opt) {
            case 'r':
                relative_mode = 1;
                break;
            case 'i':
                incremental_mode = 1;
                strcpy(format, "%H:%M:%S");
                break;
            case 's':
                since_start_mode = 1;
                strcpy(format, "%H:%M:%S");
                break;
            case 'm':
                monotonic_mode = 1;
                break;
            case 'u':
                unique_mode = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
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
            time_t parsed_time = parse_timestamp_in_line(line);
            
            if (parsed_time != 0) {
                // Found a timestamp
                if (optind < argc) {
                    // Custom format specified, convert to that format
                    char formatted_time[MAX_FORMAT_LENGTH];
                    char replaced_line[MAX_LINE_LENGTH];
                    struct tm *tm_info = localtime(&parsed_time);
                    strftime(formatted_time, sizeof(formatted_time), format, tm_info);
                    replace_timestamp_in_line(replaced_line, sizeof(replaced_line), line, formatted_time);
                    printf("%s", replaced_line);
                } else {
                    // No format specified, convert to relative time
                    char relative_time[MAX_FORMAT_LENGTH];
                    char replaced_line[MAX_LINE_LENGTH];
                    format_relative_time(relative_time, sizeof(relative_time), parsed_time);
                    replace_timestamp_in_line(replaced_line, sizeof(replaced_line), line, relative_time);
                    printf("%s", replaced_line);
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
                diff_nsec += 1000000000L;
            }
            
            char timestamp[MAX_FORMAT_LENGTH];
            if (strstr(format, "%.s") != NULL) {
                snprintf(timestamp, sizeof(timestamp), "%ld.%06ld", diff_sec, diff_nsec / 1000);
            } else if (strstr(format, "%.S") != NULL) {
                long total_seconds = diff_sec % 60;
                snprintf(timestamp, sizeof(timestamp), "%02ld.%06ld", total_seconds, diff_nsec / 1000);
            } else if (strstr(format, "%.T") != NULL) {
                long hours = diff_sec / 3600;
                long minutes = (diff_sec % 3600) / 60;
                long seconds = diff_sec % 60;
                snprintf(timestamp, sizeof(timestamp), "%02ld:%02ld:%02ld.%06ld", hours, minutes, seconds, diff_nsec / 1000);
            } else {
                struct tm *tm_info = gmtime(&diff_sec);
                strftime(timestamp, sizeof(timestamp), format, tm_info);
            }
            printf("%s %s", timestamp, line);
            if (unique_mode) {
                strcpy(last_line, line);
            }
            last_time = current_time;
        } else if (since_start_mode) {
            // Time since start
            long diff_sec = current_time.seconds - start_time.seconds;
            long diff_nsec = current_time.nanoseconds - start_time.nanoseconds;
            
            if (diff_nsec < 0) {
                diff_sec--;
                diff_nsec += 1000000000L;
            }
            
            char timestamp[MAX_FORMAT_LENGTH];
            if (strstr(format, "%.s") != NULL) {
                snprintf(timestamp, sizeof(timestamp), "%ld.%06ld", diff_sec, diff_nsec / 1000);
            } else if (strstr(format, "%.S") != NULL) {
                long total_seconds = diff_sec % 60;
                snprintf(timestamp, sizeof(timestamp), "%02ld.%06ld", total_seconds, diff_nsec / 1000);
            } else if (strstr(format, "%.T") != NULL) {
                long hours = diff_sec / 3600;
                long minutes = (diff_sec % 3600) / 60;
                long seconds = diff_sec % 60;
                snprintf(timestamp, sizeof(timestamp), "%02ld:%02ld:%02ld.%06ld", hours, minutes, seconds, diff_nsec / 1000);
            } else {
                struct tm *tm_info = gmtime(&diff_sec);
                strftime(timestamp, sizeof(timestamp), format, tm_info);
            }
            printf("%s %s", timestamp, line);
            if (unique_mode) {
                strcpy(last_line, line);
            }
        } else {
            // Default absolute timestamp mode
            process_line(line, format, current_time);
            if (unique_mode) {
                strcpy(last_line, line);
            }
        }
    }
    
    return 0;
} 