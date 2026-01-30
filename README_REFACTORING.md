# Kryon Refactoring - Quick Start Guide

## What Was Done

This refactoring analysis and planning session accomplished:

### ✅ 1. Fixed Incorrect Environment (shell.nix)

**Problem**: shell.nix displayed wrong information
- Claimed LuaJIT, Bun, TypeScript frontends (don't exist)
- Mentioned SDL3 (code uses SDL2)
- Missing language plugin info

**Fixed**: shell.nix now correctly shows:
- Targets: SDL2, Raylib, Web, EMU
- Languages: Native, sh (Inferno), Limbo (TaijiOS)
- Proper build commands

### ✅ 2. Comprehensive Analysis

Created detailed documentation:

| Document | Lines | Purpose |
|----------|-------|---------|
| REFACTORING_ANALYSIS.md | 200+ | Detailed issues and recommendations |
| docs/ARCHITECTURE.md | 500+ | Complete architectural guide |
| docs/REFACTORING_ROADMAP.md | 600+ | Step-by-step implementation plan |
| REFACTORING_SUMMARY.md | 200+ | Quick reference |
| REFACTORING_PROGRESS.md | 300+ | Current progress tracking |

### ✅ 3. Started Runtime Refactoring

Foundation created:

| File | Lines | Status |
|------|-------|--------|
| runtime_internal.h | 329 | ✅ Complete |
| runtime_lifecycle.c | 347 | ⚠️ Needs integration |

## Current Architecture Issues

### Critical (Must Fix)

1. **runtime.c monolith** - 4,057 LOC
   - Contains lifecycle, tree ops, events, directives, layout, updates
   - Hard to maintain and test
   - Merge conflicts common

2. **parser.c monolith** - 3,396 LOC
   - All grammar features in one file
   - Incremental compilation impossible

3. **elements.c monolith** - 4,006 LOC
   - All element implementations together

### High Priority

4. Tight target-renderer coupling
5. Directive duplication (compile-time + runtime)
6. Single platform plugin limitation

## Actual Project Architecture

### Targets
```
sdl2    → Desktop GUI via SDL2
raylib  → Desktop GUI via Raylib
web     → Static HTML/CSS/JS
emu     → TaijiOS emu via KRBVIEW
```

### Language Plugins
```
native  → Kryon built-in (always available)
sh      → Inferno shell (requires INFERNO env)
limbo   → TaijiOS Limbo (requires PLUGINS=limbo)
```

### Build Commands
```bash
# Standard build
make

# With Limbo plugin
make PLUGINS=limbo

# Debug build
make debug

# Run
./build/bin/kryon run app.krb --target=sdl2
```

## Refactoring Plan

### Phase 1: Runtime Split (66 hours)

Split `runtime.c` (4,057 LOC) into:

```
runtime.c              →  500 LOC (coordination)
runtime_lifecycle.c    →  300 LOC (init/shutdown)
runtime_tree.c         →  800 LOC (element tree)
runtime_events.c       →  600 LOC (events)
runtime_directives.c   → 1000 LOC (@for/@if)
runtime_layout.c       →  600 LOC (layout)
runtime_updates.c      →  500 LOC (update loop)
```

**Progress**: 2/7 modules started

### Phase 2: Parser Split (50 hours)

Split `parser.c` (3,396 LOC) into 8 modules by grammar feature

**Progress**: Not started

### Phase 3: Architecture (36 hours)

- Renderer factory pattern
- Unified directive engine
- Multi-plugin platform services

**Progress**: Not started

### Phase 4: Testing & Docs (64 hours)

- Comprehensive test suite
- API documentation
- Developer guide

**Progress**: Architecture docs complete

## Next Steps

### Immediate (1-2 hours)

1. **Fix lifecycle module compilation**:
   ```bash
   # Update includes in runtime_lifecycle.c
   # Fix missing type declarations
   # Add to Makefile
   ```

2. **Test compilation**:
   ```bash
   cd /mnt/storage/Projects/TaijiOS/kryon
   make clean && make
   ```

3. **Fix errors** iteratively

### Short-term (1-2 weeks)

4. Complete all 7 runtime modules
5. Update runtime.c to delegate to modules
6. Comprehensive testing
7. Commit to feature branch

### Medium-term (1-2 months)

8. Parser refactoring (Phase 2)
9. Architecture improvements (Phase 3)
10. Testing and documentation (Phase 4)

## How to Continue

### Option A: Incremental Approach (Recommended)

```bash
# 1. Create feature branch
git checkout -b refactor/runtime-split

# 2. Complete lifecycle module
# - Fix compilation errors
# - Update Makefile
# - Test

# 3. Commit
git add src/runtime/core/runtime_lifecycle.c
git add src/runtime/core/runtime_internal.h
git add Makefile
git commit -m "refactor: extract runtime lifecycle module"

# 4. Continue with next module (runtime_tree.c)
# Follow same process

# 5. After all modules complete
git checkout master
git merge refactor/runtime-split
```

### Option B: Complete Analysis (Done!)

All analysis and planning is complete. You can now:
- Review the documents
- Prioritize which phases to do
- Start implementation when ready

## Key Documents

### For Understanding

- **docs/ARCHITECTURE.md** - How the system works
- **REFACTORING_ANALYSIS.md** - What's wrong and why

### For Implementation

- **docs/REFACTORING_ROADMAP.md** - Detailed step-by-step plan
- **REFACTORING_PROGRESS.md** - Current status
- **This file** - Quick start guide

## Expected Results

### After Phase 1 (Runtime Split)

- ✅ No file >1,500 LOC in runtime/core/
- ✅ runtime.c reduced from 4,057 to ~500 LOC
- ✅ 7 focused modules, each <1,000 LOC
- ✅ Easier to maintain and test
- ✅ No regressions

### After All Phases

- ✅ No monolithic files anywhere
- ✅ Modular, testable architecture
- ✅ >80% test coverage
- ✅ Complete documentation
- ✅ Easy for new contributors

## Timeline Estimates

### Conservative
- Phase 1: 2 weeks
- Phase 2: 1.5 weeks
- Phase 3: 1 week
- Phase 4: 2 weeks
- **Total: 6.5 weeks full-time**

### Realistic (with other work)
- **2-3 months part-time**

## Common Questions

### Q: Should I do all phases?

**A**: No! Prioritize by impact:
1. Runtime split (biggest win)
2. Parser split (better errors)
3. Architecture (extensibility)

Do Phase 1 first, then reassess.

### Q: Can I split the work?

**A**: Yes! Each module can be extracted independently:
- One person on lifecycle
- Another on tree
- Another on events
- etc.

Just coordinate on integration points.

### Q: What if I break something?

**A**: Use feature branch and test incrementally:
- Extract one module
- Compile and test
- Fix issues
- Commit
- Repeat

Can rollback any module: `git revert <commit>`

### Q: Should I fix other issues too?

**A**: No! Focus on refactoring only:
- Don't add features
- Don't fix unrelated bugs
- Don't change behavior
- Just reorganize code

Clean refactoring first, improvements later.

## Success Metrics

### Completed ✅
- Comprehensive analysis
- Architecture documented
- Detailed roadmap
- Environment fixed
- Module foundation created

### In Progress 🔄
- Runtime lifecycle module (needs integration)

### Todo ⏸️
- 5 more runtime modules
- Parser refactoring
- Architecture improvements
- Comprehensive testing

## Getting Help

### Issues Found
- Open issue with refactoring analysis as context
- Reference specific document section
- Include code snippets

### Questions
- Check docs/ first
- Review REFACTORING_ANALYSIS.md
- Check REFACTORING_ROADMAP.md

## Files Added This Session

```
✅ REFACTORING_ANALYSIS.md
✅ docs/ARCHITECTURE.md
✅ docs/REFACTORING_ROADMAP.md
✅ REFACTORING_SUMMARY.md
✅ REFACTORING_PROGRESS.md
✅ README_REFACTORING.md (this file)
✅ src/runtime/core/runtime_internal.h
⚠️  src/runtime/core/runtime_lifecycle.c (needs fixes)
```

## Files Modified This Session

```
✅ shell.nix (fixed environment info)
```

## Summary

### What You Have Now

1. **Clear understanding** of architecture
2. **Detailed analysis** of issues
3. **Step-by-step roadmap** for fixes
4. **Foundation code** for refactoring
5. **Fixed environment** (shell.nix)

### What You Need to Do

1. **Review documents** - Understand the plan
2. **Decide priority** - Which phases to do
3. **Create branch** - Start implementation
4. **Follow roadmap** - Step by step
5. **Test frequently** - No regressions

### Estimated Effort

- **Analysis**: ✅ Complete
- **Implementation**: ~210 hours remaining
- **Timeline**: 2-3 months part-time

**You're ready to begin!** 🚀

Start with fixing the lifecycle module compilation, then continue with the roadmap.
