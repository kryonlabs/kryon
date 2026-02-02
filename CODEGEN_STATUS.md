# Kryon Codegen Status

## Understanding the Matrix

This matrix shows which language+toolkit combinations are **technically valid**:

- **✅ Working** - Code exists and runs
- **🟡 Limited** - Works but loses information/has bugs
- **🔴 Not Implemented** - Valid combo but not built yet
- **✗ Invalid** - Technically impossible combination

## Language + Toolkit Matrix

| Language | Tk | Draw | DOM | AndroidViews | Terminal | SDL3 | Raylib |
|----------|----|----|-----|--------------|----------|------|--------|
| Tcl | 🟡 | ✗ | ✗ | ✗ | ✅ | ✗ | ✗ |
| Limbo | 🔴 | ✗ | ✗ | ✗ | ✅ | ✗ | ✗ |
| C | 🔴 | ✗ | ✗ | ✗ | ✅ | 🔴 | 🔴 |
| Kotlin | ✗ | ✗ | ✗ | 🔴 | ✗ | ✗ | ✗ |
| Lua | ✗ | ✗ | ✗ | ✗ | ✅ | 🔴 | ✗ |

**Special Output Formats**:
- **Web** 🟡 = HTML/CSS/JS bundle for browsers (40-60% preservation, presentation layer)
- **Markdown** ✅ = Direct Markdown export (95%+ preservation)

**IMPORTANT**: The "Web" target is NOT "JavaScript as a language". Web is a special composite format that generates HTML (structure), CSS (styling), and JavaScript (interactivity) for browser deployment. JavaScript as a general-purpose language (for Node.js, React Native, etc.) is NOT a current target.

**Status Details**:
- **Tcl+Tk** 🟡 = Works but only 30% information preservation (scripts lost)
- **Web (HTML/CSS/JS)** 🟡 = Works but 40-60% preservation (presentation layer only)
- **Markdown** ✅ = Full round-trip support (95%+ preservation)
- **Limbo+Tk** 🔴 = Not implemented yet (one-way only)
- **C+SDL3, C+Raylib** 🔴 = Parser in testing, not working
- **Kotlin+Android** 🔴 = Not implemented
- **Lua+SDL3** 🔴 = Not implemented

## Valid Build Targets

All targets require **`language+toolkit@platform`** format:

```
# Working (with limitations)
tcl+tk@desktop          → Tcl/Tk (30% preservation)
web@web                 → HTML/CSS/JS bundle (40-60% preservation)
markdown@any            → Markdown documentation (95%+ preservation)
limbo+terminal@terminal → Limbo terminal apps
c+terminal@terminal    → C terminal apps
lua+terminal@terminal  → Lua terminal apps

# Not implemented (but technically valid)
limbo+tk@taiji         → Limbo/Tk on TaijiOS
c+tk@desktop          → C with Tk on desktop
c+sdl3@desktop         → C with SDL3 on desktop
c+raylib@desktop       → C with Raylib on desktop
lua+sdl3@desktop       → Lua with SDL3 on desktop
kotlin+android@mobile  → Kotlin on mobile
```

**NOTE**: "web" is a special target (not a language+toolkit combination) that outputs HTML/CSS/JS for browsers. It is NOT "JavaScript as a general-purpose language".

## Current CLI Commands

```bash
# Build and run (transpile KRY source to target)
kryon run --target=tcl+tk@desktop main.kry
kryon run --target=javascript+dom@web main.kry

# Query system
kryon targets                    # List all valid combinations
kryon capabilities              # Show technical compatibility
kryon lang                      # List languages
kryon platform                  # List platforms
kryon toolkit                   # List toolkits
```

## Platform Aliases

- `taiji` → `taijios`
- `inferno` → `taijios`

## Round-Trip Results

| Target | Preservation | Status |
|--------|---------------|--------|
| KRY → KRY | 95%+ | ✅ Production |
| Tcl+Tk | ~30% | 🟡 Limited (scripts lost) |
| JavaScript+DOM | ~60% | 🟡 Limited (presentation) |
| C (SDL3/Raylib) | TBD | 🔴 In Progress |
