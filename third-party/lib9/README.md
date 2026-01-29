# lib9 Compatibility Layer for Kryon

This directory contains a compatibility layer implementation of Plan 9's lib9 library, enabling Kryon to build on standard Linux systems without requiring a full TaijiOS/Inferno installation.

## Source

The implementation is derived from TaijiOS lib9, which itself originates from:
- Plan 9 from Bell Labs
- Inferno OS (Vita Nuova Holdings)
- TaijiOS (modern continuation)

**TaijiOS Source**: Snapshot from commit `969c11d3` (Jan 22, 2026)
**Original Location**: `/home/wao/Projects/TaijiOS/lib9/`

## License

Licensed under the MIT/Lucent Public License. See `NOTICE` file for full copyright information.

## Structure

```
third-party/lib9/
├── include/          # Public headers
│   ├── lib9.h           # Main lib9 interface (copied from TaijiOS)
│   ├── lib9_types.h     # Type definitions
│   ├── lib9_print.h     # Format/print declarations
│   ├── lib9_string.h    # String utilities
│   └── lib9_utf8.h      # UTF-8/Rune support
├── src/
│   ├── fmt/          # Format and print subsystem
│   ├── utf/          # UTF-8 encoding/decoding
│   └── util/         # Utility functions
├── NOTICE            # Copyright and license
└── README.md         # This file
```

## Included Components

### Format/Print Subsystem (`src/fmt/`)
Core Plan 9 formatted I/O:
- `fmt.c`, `dofmt.c`, `dorfmt.c` - Format engine
- `fmtfd.c`, `fmtstr.c` - Output backends
- `print.c`, `fprint.c`, `vfprint.c` - Print functions
- `snprint.c`, `vsnprint.c` - String formatting
- `seprint.c`, `vseprint.c` - Safe string formatting
- `smprint.c`, `vsmprint.c` - Allocating print
- `fmtrune.c`, `fmtquote.c` - Rune/quote support
- `fltfmt.c` - Floating-point formatting
- `errfmt.c` - Error format handler
- `fmtlock.c` - Thread safety
- Rune variants: `runeseprint.c`, `runesnprint.c`, etc.

### UTF-8 Subsystem (`src/utf/`)
Unicode and UTF-8 support:
- `rune.c` - UTF-8 ↔ Rune conversion
- `utflen.c`, `utfnlen.c` - String length
- `utfrune.c`, `utfrrune.c` - Character search
- `utfecpy.c` - Safe string copy
- `runetype.c` - Character classification
- `runestrlen.c`, `runestrchr.c` - Rune string ops

### Utilities (`src/util/`)
Helper functions:
- `strecpy.c` - Safe string copy
- `cistrcmp.c`, `cistrncmp.c`, `cistrstr.c` - Case-insensitive compare
- `tokenize.c`, `getfields.c` - String parsing
- `strdup.c` - String duplication
- `exits.c` - Exit wrapper
- `sysfatal.c` - Fatal error handler
- `errstr-posix.c`, `rerrstr.c` - Error strings
- `mallocz.c` - Zero-initialized malloc

## Usage

### In Kryon Source Code

```c
#include "lib9.h"  // Works unchanged via wrapper

// Use Plan 9 functions
fprint(2, "Error: %s\n", msg);
char *result = smprint("Value: %d", num);
exits(nil);
```

### Build Modes

**Standalone Linux Build** (default):
```bash
make
```
Uses compatibility layer, no TaijiOS needed.

**TaijiOS Native Build**:
```bash
export INFERNO=/path/to/TaijiOS
make INFERNO=$INFERNO
```
Links against real lib9 from TaijiOS.

## Modifications from TaijiOS

1. **lib9.h**: Changed `#include "math.h"` → `#include <math.h>` (line 15)
2. **lib9.h**: Removed EMU-specific `Proc` typedef (not needed for standalone)
3. **Directory structure**: Reorganized into include/ and src/ subdirs
4. **Build integration**: Added Makefile rules for conditional compilation

## Implementation Notes

- **No locking**: QLock/RWLock not implemented (not needed for Kryon's use case)
- **POSIX error strings**: Uses `errstr-posix.c` for `%r` format support
- **Standard malloc**: Uses libc malloc/free (mallocz wrapper provided)
- **Thread safety**: Format locking uses simple stubs for single-threaded use

## Compatibility

The compatibility layer provides:
- ✅ All print/format functions (`fprint`, `snprint`, `seprint`, etc.)
- ✅ UTF-8 encoding/decoding (`chartorune`, `runetochar`, etc.)
- ✅ String utilities (`strecpy`, `cistrcmp`, `tokenize`)
- ✅ Rune character classification (`isalpharune`, `tolowerrune`, etc.)
- ✅ Error handling (`exits`, `sysfatal`, `errstr`)
- ⚠️ No thread synchronization primitives (QLock, RWLock)
- ⚠️ No Plan 9 file operations (use POSIX directly)

## Future Maintenance

If TaijiOS lib9 is updated and you need to sync:

```bash
# Copy updated files
cp /path/to/TaijiOS/lib9/<file>.c third-party/lib9/src/<subsystem>/

# Verify no new dependencies
grep -r "^#include" third-party/lib9/src/

# Rebuild
make clean && make
```

## References

- [Plan 9 from Bell Labs](https://9p.io/plan9/)
- [Inferno OS](http://www.vitanuova.com/inferno/)
- [TaijiOS](https://github.com/Harvey-OS/TaijiOS) (hypothetical URL)
- [Plan 9 C Library](https://9p.io/magic/man2html/2/fmtinstall)
