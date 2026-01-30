# Kryon UI Framework Build System (TaijiOS mkfile)
#
# This mkfile provides TaijiOS-compatible build configuration
# Preserves multi-target capability (web, Limbo, SDL3, Android)
#
# Usage:
#   mk              - Build all components
#   mk clean        - Clean all build artifacts
#   mk install      - Install to TaijiOS/Linux/amd64/bin
#   mk nuke         - Remove all generated files

<../mkconfig

# Build order is critical:
# 1. Third-party dependencies (cJSON, tomlc99)
# 2. IR library (core functionality)
# 3. Code generators (all targets)
# 4. DIS bytecode compiler
# 5. CLI tool (links everything together)

all:V: third_party ir codegens dis cli

# Third-party libraries (cJSON, tomlc99)
third_party:V:
	echo "Building third-party libraries..."
	(cd third_party && mk install)
	echo "✓ Third-party libraries built"

# IR library (core intermediate representation)
ir:V: third_party
	echo "Building IR library..."
	(cd ir && mk install)
	echo "✓ IR library built"

# All code generators (preserving multi-target support)
codegens:V: ir
	echo "Building code generators (all targets)..."
	(cd codegens && mk install)
	echo "✓ Code generators built (limbo, web, c, android, kry, markdown)"

# DIS bytecode compiler (for direct .dis generation)
dis:V: ir
	echo "Building DIS bytecode compiler..."
	(cd dis && mk install)
	echo "✓ DIS compiler built"

# CLI tool (main kryon binary)
cli:V: ir codegens dis
	echo "Building CLI tool..."
	(cd cli && mk install)
	echo "✓ CLI tool built and installed to $ROOT/$OBJDIR/bin/kryon"

# Clean all build artifacts
clean:V:
	echo "Cleaning build artifacts..."
	(cd third_party && mk clean)
	(cd ir && mk clean)
	(cd codegens && mk clean)
	(cd dis && mk clean)
	(cd cli && mk clean)
	echo "✓ Clean complete"

# Install (already done by component installs, but provided for completeness)
install:V: all
	echo "✓ Kryon installed to $ROOT/$OBJDIR/bin/kryon"

# Nuke everything (including installed files)
nuke:V:
	echo "Removing all generated files..."
	cd third_party && mk nuke
	cd ir && mk nuke
	cd codegens && mk nuke
	cd dis && mk nuke
	cd cli && mk nuke
	rm -f $ROOT/$OBJDIR/bin/kryon
	echo "✓ Nuke complete"

# Help target
help:V:
	echo 'Kryon TaijiOS Build System'
	echo ''
	echo 'Targets:'
	echo '  mk              - Build all components (third-party → IR → codegens → DIS → CLI)'
	echo '  mk clean        - Clean all build artifacts'
	echo '  mk install      - Install kryon to $ROOT/$OBJDIR/bin'
	echo '  mk nuke         - Remove all generated files including installed binary'
	echo ''
	echo 'Individual components:'
	echo '  mk third_party  - Build third-party dependencies (cJSON, tomlc99)'
	echo '  mk ir           - Build IR library'
	echo '  mk codegens     - Build all code generators (multi-target)'
	echo '  mk dis          - Build DIS bytecode compiler'
	echo '  mk cli          - Build CLI tool'
	echo ''
	echo 'Multi-target support:'
	echo '  All targets preserved: limbo, web, c, android, kry, markdown'
	echo '  Target selected at runtime via --target flag'
	echo '  Default target: limbo (when TAIJI_PATH is set)'
