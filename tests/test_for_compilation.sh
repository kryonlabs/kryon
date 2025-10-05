#!/bin/bash

# @for Directive Compilation Test Suite
# This script tests the @for directive by compiling various .kry files
# and verifying that they produce valid KRB files

set -e  # Exit on any error

TEST_DIR="tests/for_test_files"
BUILD_DIR="build"
KRYON_BIN="$BUILD_DIR/bin/kryon"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Helper functions
print_test_header() {
    echo -e "${BLUE}🧪 $1${NC}"
}

print_success() {
    echo -e "${GREEN}✅ $1${NC}"
}

print_failure() {
    echo -e "${RED}❌ $1${NC}"
}

print_info() {
    echo -e "${YELLOW}ℹ️  $1${NC}"
}

# Create test directory
mkdir -p "$TEST_DIR"

# Test 1: Basic @for directive
test_basic_for() {
    print_test_header "Test 1: Basic @for directive"
    TESTS_RUN=$((TESTS_RUN + 1))
    
    cat > "$TEST_DIR/basic_for.kry" << 'EOF'
@metadata {
    title = "Basic For Test"
}

@var fruits = ["apple", "banana", "cherry"]

Column {
    @for fruit in fruits {
        Text {
            text = fruit
        }
    }
}
EOF

    if $KRYON_BIN compile "$TEST_DIR/basic_for.kry" -o "$TEST_DIR/basic_for.krb" > /dev/null 2>&1; then
        if [ -f "$TEST_DIR/basic_for.krb" ]; then
            print_success "Basic @for compilation successful"
            TESTS_PASSED=$((TESTS_PASSED + 1))
        else
            print_failure "Basic @for compilation - KRB file not created"
            TESTS_FAILED=$((TESTS_FAILED + 1))
        fi
    else
        print_failure "Basic @for compilation failed"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

# Test 2: Empty array @for
test_empty_array_for() {
    print_test_header "Test 2: Empty array @for"
    TESTS_RUN=$((TESTS_RUN + 1))
    
    cat > "$TEST_DIR/empty_for.kry" << 'EOF'
@metadata {
    title = "Empty Array Test"
}

@var empty_list = []

Column {
    Text { text = "Before" }
    @for item in empty_list {
        Text { text = item }
    }
    Text { text = "After" }
}
EOF

    if $KRYON_BIN compile "$TEST_DIR/empty_for.kry" -o "$TEST_DIR/empty_for.krb" > /dev/null 2>&1; then
        if [ -f "$TEST_DIR/empty_for.krb" ]; then
            print_success "Empty array @for compilation successful"
            TESTS_PASSED=$((TESTS_PASSED + 1))
        else
            print_failure "Empty array @for compilation - KRB file not created"
            TESTS_FAILED=$((TESTS_FAILED + 1))
        fi
    else
        print_failure "Empty array @for compilation failed"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

# Test 3: Complex @for with multiple properties
test_complex_for() {
    print_test_header "Test 3: Complex @for with multiple properties"
    TESTS_RUN=$((TESTS_RUN + 1))
    
    cat > "$TEST_DIR/complex_for.kry" << 'EOF'
@metadata {
    title = "Complex For Test"
}

@var colors = ["red", "green", "blue"]

Column {
    gap = 10
    @for color in colors {
        Container {
            padding = 15
            background = "#FFFFFF"
            Text {
                text = color
                fontSize = 16
                color = "#333333"
            }
        }
    }
}
EOF

    if $KRYON_BIN compile "$TEST_DIR/complex_for.kry" -o "$TEST_DIR/complex_for.krb" > /dev/null 2>&1; then
        if [ -f "$TEST_DIR/complex_for.krb" ]; then
            print_success "Complex @for compilation successful"
            TESTS_PASSED=$((TESTS_PASSED + 1))
        else
            print_failure "Complex @for compilation - KRB file not created"
            TESTS_FAILED=$((TESTS_FAILED + 1))
        fi
    else
        print_failure "Complex @for compilation failed"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

# Test 4: Nested @for directives  
test_nested_for() {
    print_test_header "Test 4: Nested @for directives"
    TESTS_RUN=$((TESTS_RUN + 1))
    
    cat > "$TEST_DIR/nested_for.kry" << 'EOF'
@metadata {
    title = "Nested For Test"
}

@var rows = ["A", "B"]
@var cols = ["1", "2"]

Column {
    @for row in rows {
        Row {
            Text { text = row }
            @for col in cols {
                Text { text = col }
            }
        }
    }
}
EOF

    if $KRYON_BIN compile "$TEST_DIR/nested_for.kry" -o "$TEST_DIR/nested_for.krb" > /dev/null 2>&1; then
        if [ -f "$TEST_DIR/nested_for.krb" ]; then
            print_success "Nested @for compilation successful" 
            TESTS_PASSED=$((TESTS_PASSED + 1))
        else
            print_failure "Nested @for compilation - KRB file not created"
            TESTS_FAILED=$((TESTS_FAILED + 1))
        fi
    else
        print_failure "Nested @for compilation failed"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

# Test 5: @for with various UI elements
test_for_with_elements() {
    print_test_header "Test 5: @for with various UI elements"
    TESTS_RUN=$((TESTS_RUN + 1))
    
    cat > "$TEST_DIR/elements_for.kry" << 'EOF'
@metadata {
    title = "Elements For Test"
}

@var items = ["Button", "Input", "Text"]

Column {
    @for item in items {
        Button {
            text = item
            width = 120
            height = 40
        }
    }
    @for item in items {
        Text {
            text = item
            fontSize = 14
        }
    }
}
EOF

    if $KRYON_BIN compile "$TEST_DIR/elements_for.kry" -o "$TEST_DIR/elements_for.krb" > /dev/null 2>&1; then
        if [ -f "$TEST_DIR/elements_for.krb" ]; then
            print_success "Elements @for compilation successful"
            TESTS_PASSED=$((TESTS_PASSED + 1))
        else
            print_failure "Elements @for compilation - KRB file not created" 
            TESTS_FAILED=$((TESTS_FAILED + 1))
        fi
    else
        print_failure "Elements @for compilation failed"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

# Test 6: Single item @for (the working case)
test_single_item_for() {
    print_test_header "Test 6: Single item @for (known working case)"
    TESTS_RUN=$((TESTS_RUN + 1))
    
    cat > "$TEST_DIR/single_for.kry" << 'EOF'
@metadata {
    title = "Single Item Test"  
}

@var single = ["apple"]

Column {
    Text { text = "Item:" }
    @for item in single {
        Text { text = item }
    }
}
EOF

    if $KRYON_BIN compile "$TEST_DIR/single_for.kry" -o "$TEST_DIR/single_for.krb" > /dev/null 2>&1; then
        if [ -f "$TEST_DIR/single_for.krb" ]; then
            print_success "Single item @for compilation successful"
            TESTS_PASSED=$((TESTS_PASSED + 1))
        else
            print_failure "Single item @for compilation - KRB file not created"
            TESTS_FAILED=$((TESTS_FAILED + 1))
        fi
    else
        print_failure "Single item @for compilation failed"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

# Main test execution
main() {
    echo -e "${BLUE}🧪 @for Directive Compilation Test Suite${NC}"
    echo "=========================================="
    echo

    # Check if kryon binary exists
    if [ ! -f "$KRYON_BIN" ]; then
        print_failure "Kryon binary not found at $KRYON_BIN"
        print_info "Please run ./scripts/build.sh first"
        exit 1
    fi

    print_info "Using Kryon binary: $KRYON_BIN"
    echo

    # Run all tests
    test_basic_for
    test_empty_array_for
    test_complex_for
    test_nested_for
    test_for_with_elements
    test_single_item_for

    echo
    echo "📊 Test Results Summary:"
    echo "========================"
    echo "Tests run: $TESTS_RUN"
    echo -e "Tests passed: ${GREEN}$TESTS_PASSED${NC}"
    echo -e "Tests failed: ${RED}$TESTS_FAILED${NC}"
    echo

    if [ $TESTS_FAILED -eq 0 ]; then
        echo -e "${GREEN}🎉 All @for directive compilation tests passed!${NC}"
        echo
        print_info "The @for directive successfully compiles all test patterns."
        print_info "This confirms the @for implementation is working correctly"
        print_info "for compilation and KRB file generation."
    else
        echo -e "${RED}❌ Some @for directive tests failed.${NC}"
        echo -e "${YELLOW}⚠️  Check compiler output for details.${NC}"
    fi

    # Cleanup
    print_info "Cleaning up test files..."
    rm -rf "$TEST_DIR"

    # Return appropriate exit code
    if [ $TESTS_FAILED -eq 0 ]; then
        exit 0
    else
        exit 1
    fi
}

# Run the test suite
main "$@"
