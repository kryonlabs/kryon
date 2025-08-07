# Kryon-C File Hierarchy and Architecture

- do not make .c or other files in examples only .kry files!

## Important Instructions
Do what has been asked; nothing more, nothing less.
NEVER create files unless they're absolutely necessary for achieving your goal.
ALWAYS prefer editing an existing file to creating a new one.
NEVER proactively create documentation files (*.md) or README files. Only create documentation files if explicitly requested by the User.

## Clean Header File Hierarchy

### **Core System Headers**

#### **1. internal/memory.h** - Memory Management (Base Layer)
- **Defines**: Memory management types and functions
- **Dependencies**: Standard libraries only
- **Status**: ✅ Clean, no conflicts

#### **2. internal/events.h** - Event System (Base Layer)
- **Defines**: 
  - `KryonEventType` enum (unified event types)
  - `KryonEvent` struct (unified event structure)
  - `KryonEventHandler` function pointer type
- **Dependencies**: Standard libraries only
- **Status**: ✅ Clean, this is the authoritative event system

#### **3. internal/runtime.h** - Runtime System
- **Defines**:
  - `KryonElement` (actual element definition)
  - `ElementEventHandler` struct (wraps KryonEventHandler with metadata)
  - `KryonRuntime`, `KryonState` types
- **Dependencies**: 
  - `internal/memory.h` ✅
  - `internal/events.h` ✅ 
  - `internal/krb_format.h` ✅
- **Status**: ✅ Fixed conflicts, now uses events.h properly

#### **4. internal/renderer_interface.h** - Renderer Interface
- **Defines**:
  - Core geometry: `KryonVec2`, `KryonVec4`, `KryonColor`, `KryonRect`
  - Renderer types: `KryonRenderer`, `KryonRenderResult`, `KryonRendererVTable`
  - Widget rendering structures
- **Dependencies**: 
  - `internal/events.h` ✅
- **Status**: ✅ Clean, authoritative for renderer types

#### **5. internal/krb_format.h** - KRB Binary Format
- **Defines**: KRB file format structures
- **Dependencies**: Standard libraries only
- **Status**: ✅ Clean

## **Current Working Solutions**

### **Correct Include Order**
1. `internal/memory.h` (base)
2. `internal/events.h` (base)  
3. `internal/krb_format.h` 
4. `internal/renderer_interface.h` (needs events.h)
5. `internal/runtime.h` (needs all above)

### **Event System** ✅ RESOLVED
- **Single Source**: Use `internal/events.h` only
- **Types**: `KryonEventType`, `KryonEvent`, `KryonEventHandler`
- **Runtime Wrapper**: `ElementEventHandler` in runtime.h wraps KryonEventHandler

### **Renderer System** ✅ RESOLVED  
- **Single Source**: Use `internal/renderer_interface.h`
- **Types**: `KryonRenderer`, `KryonRenderResult`, `KryonColor`
- **Function**: `kryon_raylib_renderer_create(void* surface)` in `/src/renderers/raylib/raylib_renderer.c`
- **Note**: Raylib renderer exists but not linked in CMake build

### **Element System** ✅ RESOLVED
- **Definition**: `KryonElement` in `internal/runtime.h`
- **Management**: Element functions in `/src/runtime/elements/element_manager.c`
- **Event Handlers**: Use `ElementEventHandler**` array

### **KRB Format** ✅ WORKING
- **Simple Format**: header (magic + version + flags) + element_count + elements[]
- **Magic**: 0x4B52594E ("KRYN")  
- **Loader**: `/src/runtime/core/krb_loader.c`

### **Compilation Pipeline** ✅ WORKING
1. **Source**: `.kry` files
2. **Parse**: Smart compiler auto-creates App element with @metadata properties
3. **Compile**: Generates simple KRB format
4. **Runtime**: Loads KRB and creates element tree

### **File Locations**
- **Runtime Core**: `/src/runtime/core/runtime.c`, `/src/runtime/core/krb_loader.c`
- **Element Management**: `/src/runtime/elements/element_manager.c`  
- **Compilation**: `/src/cli/compile/compile_command.c`
- **Execution**: `/src/cli/run/run_command.c`
- **Raylib Renderer**: `/src/renderers/raylib/raylib_renderer.c` (exists but not linked)

### **Build Commands**
- **Main Build**: `cd build && make -j$(nproc)`
- **Status**: ✅ Compiles successfully (header conflicts resolved)
- **Issue**: Raylib renderer not linked in CMake build

### **Current Test Status**
- **KRB Loading**: ✅ Working (loads App element with 1 child)
- **Element Tree**: ✅ Working (App → Container → Text)
- **Smart Compiler**: ✅ Working (auto-creates App from metadata)
- **Rendering**: ❌ Raylib renderer not linked in build system

## **Next Priority Tasks**

1. **Link raylib renderer** in CMake build system
2. **Verify full element tree loading** (children and properties)
3. **Test rendering pipeline** with working raylib
4. **Clean up unused conflicting headers** in public API