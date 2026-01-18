# Kryon UI Framework Build System (Plan 9 mkfile)
#
# This mkfile provides Plan 9/9front-compatible build configuration
# Parallel to the GNU Makefile for cross-platform support
#
# Usage:
#   mk              - Build all components
#   mk clean        - Clean all build artifacts
#   mk install      - Install to /bin (Plan 9 standard location)
#   mk test         - Run tests

</$objtype/mkfile

# Virtual targets (Plan 9 equivalent of .PHONY)
all:V: ir cli plan9

clean:V: clean-ir clean-cli clean-plan9

install:V: install-cli install-plan9

test:V: test-ir

# IR library (core functionality)
ir:V:
	cd ir && mk

clean-ir:V:
	cd ir && mk clean

test-ir:V:
	cd ir && mk test

# CLI tool (requires IR library)
cli:V: ir
	cd cli && mk

clean-cli:V:
	cd cli && mk clean

install-cli:V: cli
	cd cli && mk install

# Plan 9 backend (requires IR library)
plan9:V: ir
	cd backends/plan9 && mk

clean-plan9:V:
	cd backends/plan9 && mk clean

install-plan9:V: plan9
	cd backends/plan9 && mk install

# Help target
help:V:
	echo 'Kryon Plan 9 Build System'
	echo ''
	echo 'Targets:'
	echo '  mk              - Build all components (IR, CLI, Plan 9 backend)'
	echo '  mk clean        - Clean all build artifacts'
	echo '  mk install      - Install to /bin'
	echo '  mk test         - Run tests'
	echo '  mk ir           - Build IR library only'
	echo '  mk cli          - Build CLI tool only'
	echo '  mk plan9        - Build Plan 9 backend only'
	echo ''
	echo 'Individual components:'
	echo '  cd ir && mk     - Build IR library'
	echo '  cd cli && mk    - Build CLI tool'
	echo '  cd backends/plan9 && mk - Build Plan 9 backend'
