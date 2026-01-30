# Kryon Refactoring Summary

## What Was Analyzed

A comprehensive analysis of the Kryon codebase (~52,786 LOC) identified critical architectural issues and refactoring opportunities.

## Key Findings

### 1. Architecture Mismatch

**Problem**: The `shell.nix` environment displayed incorrect information about the project:
- Claimed LuaJIT, Bun, TypeScript frontends (don't exist)
- Mentioned SDL3 (code uses SDL2)
- Listed terminal/framebuffer renderers (not in codebase)
- Missed language plugin system entirely

**Solution**: Fixed `shell.nix` to accurately reflect:
- **Targets**: SDL2, Raylib, Web, EMU
- **Languages**: Native Kryon, sh (Inferno shell), Limbo (TaijiOS)
- **Build modes**: Standard Linux, with Inferno, with Limbo

### 2. Critical Code Quality Issues

#### Runtime Monolith (4,057 LOC)
**File**: `src/runtime/core/runtime.c`

**Problem**: Single file contains:
- Runtime lifecycle
- Element tree operations
- Event handling
- Directive expansion
- Layout calculation
- Update loop

**Impact**: Unmaintainable, hard to test, merge conflicts

**Solution**: Split into 7 focused modules (~500-800 LOC each)

#### Parser Monolith (3,396 LOC)
**File**: `src/compiler/parser/parser.c`

**Problem**: Single parser handles all grammar features

**Impact**: Incremental compilation impossible, unclear error messages

**Solution**: Split into 8 modules by grammar feature

### 3. Architectural Issues

#### Tight Target-Renderer Coupling
- 1:1 mapping between targets and renderers
- No fallback logic
- Hard to add new targets

**Solution**: Factory pattern with fallback chain

#### Directive Duplication
- @for/@if expanded in TWO places (compile-time + runtime)
- Duplicate logic, inconsistent behavior

**Solution**: Unified directive expansion engine

#### Single Platform Plugin
- Only one platform service plugin at a time
- Can't mix features (e.g., Inferno + POSIX)

**Solution**: Multi-plugin registry

## Actual Project Architecture

### Compilation Pipeline
```
.kry (source) → Lexer → Parser → Codegen → .krb (binary)
```

### Runtime Execution
```
.krb → Runtime Engine → Renderer (Target) → Output
              ↓
       Language Plugins
```

### Targets
- **sdl2**: Desktop GUI via SDL2
- **raylib**: Desktop GUI via Raylib
- **web**: Static HTML/CSS/JS
- **emu**: TaijiOS emu via KRBVIEW

### Language Plugins
- **Native**: Kryon built-in (always available)
- **sh**: Inferno shell scripts (requires INFERNO env)
- **Limbo**: TaijiOS Limbo modules (requires PLUGINS=limbo)

## Documents Created

### 1. REFACTORING_ANALYSIS.md
Comprehensive 200+ line analysis covering:
- Architectural overview
- Critical issues requiring refactoring
- Moderate and minor issues
- Recommended refactoring phases
- Risk assessment
- Success criteria

### 2. docs/ARCHITECTURE.md
Complete architectural guide explaining:
- Compilation pipeline
- Target system
- Language plugin architecture
- Runtime architecture
- Build system
- Directory structure
- Design principles

### 3. docs/REFACTORING_ROADMAP.md
Step-by-step refactoring plan with:
- Phase 1: Runtime split (66 hours, ~2 weeks)
- Phase 2: Parser split (50 hours, ~1.5 weeks)
- Phase 3: Architecture improvements (36 hours, ~1 week)
- Phase 4: Testing and docs (64 hours, ~2 weeks)
- **Total**: ~216 hours (~6.5 weeks)

Includes:
- Detailed module headers
- Code examples for each step
- Dependency graphs
- Testing strategy
- Risk mitigation
- Success metrics

## Changes Made

### shell.nix
**Before**: Incorrect information about LuaJIT, Bun, SDL3, etc.

**After**: Accurate display of:
- Supported targets (SDL2, Raylib, Web, EMU)
- Language plugins (Native, sh, Limbo)
- Available renderers
- Build commands
- Runtime commands

## Refactoring Priorities

### Immediate (Critical)
1. **Fix shell.nix** ✅ DONE
2. **Split runtime.c** into 7 modules
3. **Add runtime tests**

### Short-term (High)
4. **Split parser.c** into 8 modules
5. **Improve error messages**
6. **Add parser tests**

### Medium-term (Medium)
7. **Renderer factory** pattern
8. **Unified directive** expansion
9. **Multi-plugin** platform services
10. **Element type** registry

### Long-term (Low)
11. **Comprehensive tests** (>80% coverage)
12. **API documentation**
13. **Architecture diagrams**
14. **Developer guide**

## Recommended Next Steps

1. **Review the three documents**:
   - REFACTORING_ANALYSIS.md (what's wrong)
   - docs/ARCHITECTURE.md (how it works)
   - docs/REFACTORING_ROADMAP.md (how to fix it)

2. **Start with runtime.c split**:
   - Highest impact
   - Well-scoped
   - Clear module boundaries
   - Follow roadmap Phase 1

3. **Use incremental approach**:
   - Extract one module at a time
   - Add tests before extraction
   - Verify tests pass after
   - Commit frequently

4. **Track progress**:
   - Each module extraction is separate commit
   - Can rollback if issues arise
   - Feature branch protects main

## Success Metrics

Refactoring successful when:
- ✅ No file >1,500 LOC
- ✅ All tests passing
- ✅ >80% test coverage
- ✅ Build time ≤ current
- ✅ Documentation complete
- ✅ No regressions

## Timeline Estimate

**Conservative estimate**: 6.5 weeks full-time

**Realistic with other work**: 2-3 months

**Phases can be done independently** - prioritize by impact:
1. Runtime split (biggest win)
2. Parser split (enables better errors)
3. Architecture (enables extensibility)
4. Testing/docs (ensures quality)

## File Size Impact

### Current State
- `runtime.c`: 4,057 LOC ⚠️
- `parser.c`: 3,396 LOC ⚠️
- `elements.c`: 4,006 LOC ⚠️

### Target State (Post-Refactoring)
- Largest module: <1,500 LOC
- Average module: 500-800 LOC
- Total module count: +20-30 focused modules

## Questions?

**About architecture**: See `docs/ARCHITECTURE.md`

**About what's wrong**: See `REFACTORING_ANALYSIS.md`

**About how to fix**: See `docs/REFACTORING_ROADMAP.md`

**About this summary**: This document!

## Quick Start Refactoring

```bash
# 1. Review documents
cat REFACTORING_ANALYSIS.md
cat docs/ARCHITECTURE.md
cat docs/REFACTORING_ROADMAP.md

# 2. Create branch
git checkout -b refactor/runtime-split

# 3. Start with runtime_lifecycle
# Follow Phase 1, Step 1.2 in REFACTORING_ROADMAP.md

# 4. Build and test
make clean && make

# 5. Commit
git commit -m "refactor: extract runtime lifecycle module"

# 6. Continue with next module
```

## Files Modified

- `shell.nix` - Fixed to show correct architecture
- `REFACTORING_ANALYSIS.md` - NEW (detailed analysis)
- `docs/ARCHITECTURE.md` - NEW (architectural guide)
- `docs/REFACTORING_ROADMAP.md` - NEW (step-by-step plan)
- `REFACTORING_SUMMARY.md` - NEW (this file)

## No Code Changes Yet

This analysis and planning phase made **no changes to source code**.

The refactoring work itself will be done in separate branches following the roadmap.

## Benefits of This Analysis

1. **Clear understanding** of current architecture
2. **Identified critical issues** requiring attention
3. **Detailed roadmap** for fixing issues
4. **Realistic timeline** estimates
5. **Risk mitigation** strategies
6. **Success criteria** defined
7. **Team can prioritize** based on impact

## Ready to Start

Everything is in place to begin refactoring:
- ✅ Architecture documented
- ✅ Issues identified
- ✅ Roadmap created
- ✅ Shell environment fixed
- ✅ Success criteria defined

Next: Create feature branch and begin runtime.c split following Phase 1 of the roadmap.
