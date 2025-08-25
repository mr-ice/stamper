# Automake configuration for ts
# Copyright (C) 2025 Michael Rice <michael@ricelan.org>
# This file is part of TS. See the COPYING file for license terms.

# Set the program name
bin_PROGRAMS = ts

# Source files for the main program
ts_SOURCES = ts.c

# Default target
all: ## Build the ts program (default)

# Compiler flags
AM_CFLAGS = -Wall -Wextra -std=c11

# Include directories
AM_CPPFLAGS = -I$(top_srcdir)

# Test program
check_PROGRAMS = test_ts_simple
test_ts_simple_SOURCES = test_ts_simple.c

# Test target
TESTS = test_ts_simple



# Custom test target that builds and runs test_ts_simple directly
test: test_ts_simple ## Run detailed test suite
	./test_ts_simple

# Help target
help: ## Show this help message
	@echo "Available targets:"
	@echo ""
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' Makefile | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-20s\033[0m %s\n", $$1, $$2}'
	@echo ""
	@echo "For more information, see the README.md and INSTALL files."

# Additional files to distribute
EXTRA_DIST = README.md configure.ac Makefile.am NEWS AUTHORS ChangeLog doc/ts.1 doc/ts.texi

# Clean additional files
CLEANFILES = *.o *.lo *.la *.log *.trs test-suite.log

# Install man page if available
# man_MANS = ts.1

# Uninstall hook to clean up any additional files
uninstall-hook: ## Remove installed files
	rm -f $(DESTDIR)$(bindir)/ts

# Development targets
.PHONY: maintainer-clean-local
maintainer-clean-local: ## Clean autotools generated files
	rm -rf autom4te.cache
	rm -f config.h.in~
	rm -f configure~

# Standard targets with descriptions
check: ## Run standard automake test framework
install: ## Install the program system-wide
clean: clean-test clean-generic ## Remove build artifacts and test files
distclean: ## Remove all generated files
maintainer-clean: ## Remove autotools generated files

# Additional clean target for test artifacts
clean-test: ## Remove test log and result files
	rm -f *.log *.trs test-suite.log

# Documentation
man_MANS = doc/ts.1
info_TEXINFOS = doc/ts.texi

# Documentation targets
docs: ## Build documentation (man and info)
	@echo "Building documentation..."

install-docs: install-man install-info ## Install documentation

# Packaging targets
dist-tarball: dist ## Create distribution tarball
	@echo "Distribution tarball created: $(PACKAGE)-$(VERSION).tar.gz"

package: dist-tarball ## Create distribution package (tarball)
	@echo "Package created: $(PACKAGE)-$(VERSION).tar.gz"

# Debug target
debug: CFLAGS += -g -DDEBUG ## Build with debug symbols
debug: all

# Release target
release: CFLAGS += -DNDEBUG ## Build optimized release version
release: all

# Coverage target (if gcov is available)
coverage: CFLAGS += --coverage ## Build with coverage instrumentation
coverage: LDFLAGS += --coverage
coverage: clean all
	./test_ts_simple
	gcov -r ts.c
	@echo "Coverage report generated"
