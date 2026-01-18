#!/bin/bash
# Update IR library files to use ir_platform.h

set -e

echo "Updating IR library files to use ir_platform.h..."

# Find all IR C files (excluding third-party)
find ir -name "*.c" -not -path "*/third_party/*" | while read file; do
    # Check if file has standard includes
    if grep -q "#include <stdio.h>\|#include <stdlib.h>\|#include <string.h>" "$file"; then
        echo "Updating: $file"

        # Create backup
        cp "$file" "$file.bak"

        # Remove standard C includes and add ir_platform.h
        sed -i '/#include <stdio.h>/d' "$file"
        sed -i '/#include <stdlib.h>/d' "$file"
        sed -i '/#include <string.h>/d' "$file"
        sed -i '/#include <stdint.h>/d' "$file"
        sed -i '/#include <stdbool.h>/d' "$file"

        # Add ir_platform.h after first include (usually a local header)
        sed -i '0,/^#include "/{s/^#include "/&/; s/$/\n#include "ir_platform.h"/}' "$file" 2>/dev/null || true
    fi
done

echo "Done! Backup files saved as *.bak"
echo "Review changes, then run: find ir -name '*.bak' -delete"
