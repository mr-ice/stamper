# TS - Timestamp Input

A C implementation of the `ts` command from moreutils that adds timestamps to the beginning of each line of input.

## Features

- Add timestamps to each line of input
- Support for custom timestamp formats using strftime format strings
- Incremental timestamp modes (-i, -s)
- Help option (-h)
- Compatible with the original `ts` command interface

## Usage

```bash
./ts [options] [format]
```

### Options

- `-r`: Convert existing timestamps to relative times (not yet implemented)
- `-i`: Report incremental timestamps (time since last timestamp)
- `-s`: Report incremental timestamps (time since start)
- `-m`: Use monotonic clock (not yet implemented)
- `-u`: Only output lines that are unique (different from previous line)
- `-h`: Show help message

### Format

The format parameter is a strftime format string. The default format is `"%b %d %H:%M:%S"`.

## Examples

### Basic usage
```bash
echo "Hello world" | ./ts
# Output: Aug 22 22:11:28 Hello world
```

### Multiple lines
```bash
echo -e "Line 1\nLine 2\nLine 3" | ./ts
# Output:
# Aug 22 22:11:29 Line 1
# Aug 22 22:11:29 Line 2
# Aug 22 22:11:29 Line 3
```

### Custom format
```bash
echo "Test" | ./ts "%Y-%m-%d %H:%M:%S"
# Output: 2025-08-22 22:11:31 Test
```

### Subsecond resolution
```bash
echo "Test" | ./ts "%.S"
# Output: 51.065386 Test (seconds with microsecond precision)

echo "Test" | ./ts "%.s"
# Output: 1755919195.193392 Test (unix timestamp with microsecond precision)

echo "Test" | ./ts "%.T"
# Output: 22:19:59.020945 Test (time with microsecond precision)
```

### Incremental timestamps (since start)
```bash
(echo "Start"; sleep 1; echo "After 1 second"; sleep 2; echo "After 2 more seconds") | ./ts -s
# Output:
# 00:00:00 Start
# 00:00:01 After 1 second
# 00:00:03 After 2 more seconds
```

### Unique lines only
```bash
echo -e "Status: OK\nStatus: OK\nStatus: OK\nStatus: ERROR\nStatus: ERROR\nStatus: OK" | ./ts -u
# Output:
# Aug 22 22:25:04 Status: OK
# Aug 22 22:25:04 Status: ERROR
# Aug 22 22:25:04 Status: OK
```

### Unique lines with incremental timing
```bash
(echo "Status: OK"; sleep 0.1; echo "Status: OK"; sleep 0.1; echo "Status: ERROR") | ./ts -u -s "%.S"
# Output:
# 00.000013 Status: OK
# 00.212358 Status: ERROR
```

### Relative timestamps
```bash
echo "Dec 22 22:25:23 server: message" | ./ts -r
# Output: 242d23h ago server: message

echo "Dec 22 22:25:23 server: message" | ./ts -r "%Y-%m-%d %H:%M:%S"
# Output: 2024-12-22 22:25:23 server: message
```

## Building

```bash
make
```

## Installation

```bash
make install
```

This will install the program to `/usr/local/bin/`.

## Uninstallation

```bash
make uninstall
```

## Limitations

- The `-m` (monotonic clock) mode is not yet implemented

## License

This is a reimplementation of the `ts` command from moreutils. The original `ts` command is licensed under the GNU GPL. 