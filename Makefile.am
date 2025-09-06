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
check_PROGRAMS = test_ts_runner
test_ts_runner_SOURCES = test_ts_runner.c

# Test target
TESTS = test_ts_runner



# Custom test target that builds and runs test_ts_runner directly
test: test_ts_runner ## Run detailed test suite
	./test_ts_runner

# Help target
help: ## Show this help message
	@echo "Available targets:"
	@echo ""
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' Makefile | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-20s\033[0m %s\n", $$1, $$2}'
	@echo ""
	@echo "Clean target order (from least to most aggressive):"
	@echo "  make clean-test    # Remove test artifacts"
	@echo "  make clean         # Remove build artifacts (object files, executables)"
	@echo "  make maintainer-clean # Remove autotools files (Makefile.in, configure, etc.)"
	@echo "  make distclean     # Remove generated files (Makefile, config.h, etc.)"
	@echo "  ./super-clean      # Remove ALL generated files (including autotools files)"
	@echo ""
	@echo "For more information, see the README.md and INSTALL files."

# Additional files to distribute
EXTRA_DIST = README.md configure.ac Makefile.am NEWS AUTHORS ChangeLog doc/ts.1 doc/ts.texi

# Clean additional files
CLEANFILES = *.o *.lo *.la *.log *.trs test-suite.log ts test_ts_runner doc/*.info doc/.dirstamp

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
	rm -f Makefile.in
	rm -f aclocal.m4
	rm -rf build-aux
	rm -f configure
	rm -f *.tar.gz
	rm -rf packaging

# Standard targets with descriptions
check: ## Run standard automake test framework
install: ## Install the program system-wide

# Additional clean target for test artifacts
clean-test: ## Remove test log and result files
	rm -f *.log *.trs test-suite.log

# Documentation
man_MANS = doc/ts.1
info_TEXINFOS = doc/ts.texi

# Documentation targets
docs: info ## Build documentation (man and info)
	@echo "Documentation built successfully"

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
coverage: ## Build with coverage instrumentation and generate coverage report
	@echo "Cleaning previous coverage data..."
	rm -f *.gcno *.gcda *.gcov
	$(MAKE) clean
	@echo "Building with coverage instrumentation..."
	$(MAKE) CFLAGS="$(AM_CFLAGS) --coverage" LDFLAGS="--coverage" all
	$(MAKE) CFLAGS="$(AM_CFLAGS) --coverage" LDFLAGS="--coverage" test_ts_runner
	@echo "Running tests..."
	./test_ts_runner
	@echo "Generating coverage report..."
	gcov -r ts.c
	@echo "Coverage report generated: ts.c.gcov"
	@echo "Note: Coverage shows 0% because ts is called as subprocess from test runner"
