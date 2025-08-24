# Automake configuration for ts
# Copyright (C) 2025 Michael Rice <michael@ricelan.org>
# This file is part of TS. See the COPYING file for license terms.

# Set the program name
bin_PROGRAMS = ts

# Source files for the main program
ts_SOURCES = ts.c

# Compiler flags
AM_CFLAGS = -Wall -Wextra -std=c11

# Include directories
AM_CPPFLAGS = -I$(top_srcdir)

# Test program
check_PROGRAMS = test_ts_simple
test_ts_simple_SOURCES = test_ts_simple.c

# Test target
TESTS = test_ts_simple

# Additional files to distribute
EXTRA_DIST = README.md configure.ac Makefile.am NEWS AUTHORS ChangeLog

# Clean additional files
CLEANFILES = *.o *.lo *.la

# Install man page if available
# man_MANS = ts.1

# Uninstall hook to clean up any additional files
uninstall-hook:
	rm -f $(DESTDIR)$(bindir)/ts

# Development targets
.PHONY: maintainer-clean-local
maintainer-clean-local:
	rm -rf autom4te.cache
	rm -f config.h.in~
	rm -f configure~

# Debug target
debug: CFLAGS += -g -DDEBUG
debug: all

# Release target
release: CFLAGS += -DNDEBUG
release: all

# Coverage target (if gcov is available)
coverage: CFLAGS += --coverage
coverage: LDFLAGS += --coverage
coverage: clean all
	./test_ts_simple
	gcov -r ts.c
	@echo "Coverage report generated" 