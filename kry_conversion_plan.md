# KRY Conversion Plan for Habits Application

## Overview
Convert the Habits application from pure Lua to KRY format with embedded Lua logic for cross-platform compatibility.

## Current State Analysis

### Source Files to Convert
1. `main.lua` - Main entry point, state management, UI orchestration
2. `components/color_palette.lua` - Color theme data (simple functions)
3. `components/calendar.lua` - Calendar data generation (utility functions)
4. `components/color_picker.lua` - Modal color picker component
5. `components/habit_panel.lua` - Individual habit panel with calendar
6. `components/tabs.lua` - Tab management and panel building

### KRY Syntax Patterns (from existing examples)

**Component Structure:**
```kry
ComponentName {
  property = value
  childComponent {
    property = value
  }
}
```

**Property Types:**
- Basic: `width = 800`, `background = "#1a1a1a"`
- Layout: `display = flex`, `flexDirection = row`, `gap = 8`
- Objects: `border = {width: 1, color: "#000000", radius: 8}`
- Arrays: `padding = [8, 16]`

**Available Components:**
- Layout: `Column`, `Row`, `Container`
- Text: `Text`
- Interactive: `Button`, `TabGroup`, `TabBar`, `TabContent`, `TabPanel`
- Special: `Modal`, `ForEach`

### Lua Embedding Strategy
Logic will be embedded using Lua function calls within KRY properties and handlers.

---

## Multi-Phase Conversion Plan

### Phase 1: Infrastructure & Simple Modules
**Goal:** Set up KRY file structure and convert simple utility modules

#### Step 1.1: Create KRY Directory Structure
- Create `kry_components/` directory alongside `components/`
- Set up parallel structure for KRY files

**Verification:** Directory exists, files can be created

#### Step 1.2: Convert `color_palette.lua`
- **Complexity:** Low (static data + simple functions)
- **Content:**
  - Static color table
  - `getDefaultColor()` function
  - `getRandomColor()` function
- **Output:** `kry_components/color_palette.kry`

**Verification:**
- KRY file compiles without errors
- Color data accessible from main KRY file

#### Step 1.3: Convert `calendar.lua`
- **Complexity:** Low (pure utility functions, no UI)
- **Content:**
  - `generateCalendarData()` - creates calendar grid
  - `generateCalendarRows()` - organizes days into rows
  - `getDayStyle()` - applies styling based on completion
  - `isDateInFuture()` - validates date

**Verification:**
- All functions work when called from KRY context
- Calendar data generation produces correct output

---

### Phase 2: Simple UI Components
**Goal:** Convert basic UI components with minimal state dependencies

#### Step 2.1: Convert `color_picker.lua`
- **Complexity:** Medium (modal UI with state)
- **Content:**
  - Color swatch buttons
  - Modal interface
  - State management for show/hide

**Approach:**
- Use Lua functions for event handlers
- Embed state management logic

**Verification:**
- Modal opens/closes correctly
- Color selection works
- State updates properly

#### Step 2.2: Convert `tabs.lua`
- **Complexity:** Medium (dynamic list generation)
- **Content:**
  - Tab bar with habit list
  - "+" button for new habits
  - Panel building logic

**Approach:**
- Use `ForEach` component for dynamic tabs
- Lua functions for add habit logic

**Verification:**
- Tabs render for each habit
- Add button creates new habit
- Tab switching works

---

### Phase 3: Complex Components
**Goal:** Convert components with complex state and reactivity

#### Step 3.1: Convert `habit_panel.lua`
- **Complexity:** High (largest component, dynamic bindings)
- **Content:**
  - Editable habit names
  - Month navigation
  - Calendar grid with completion tracking
  - Dynamic binding for reactive updates

**Approach:**
- Use Lua functions for dynamic properties
- Embed calendar generation logic
- Handle edit mode state with Lua

**Verification:**
- Habit name editing works
- Month navigation updates calendar
- Completion tracking visualizes correctly
- Reactive updates trigger properly

---

### Phase 4: Main Application
**Goal:** Convert main entry point and integrate all KRY components

#### Step 4.1: Convert `main.lua` State Management
- **Complexity:** High (centralized state, multiple handlers)
- **Content:**
  - Reactive state initialization
  - Event handler functions
  - Persistence logic

**Approach:**
- Create Lua module for state management
- Keep business logic in Lua
- Use KRY for UI structure

**Verification:**
- State changes trigger UI updates
- Persistence saves/loads correctly
- Event handlers work as expected

#### Step 4.2: Convert `main.lua` UI Structure
- **Complexity:** High (main app layout)
- **Content:**
  - App window setup (800x750)
  - Tab group integration
  - Panel rendering
  - Color picker modal

**Approach:**
- Use KRY for layout structure
- Reference KRY components
- Embed Lua for dynamic behavior

**Verification:**
- Window renders at correct size
- All components display
- Modal overlay works
- Complete app functions end-to-end

---

### Phase 5: Build Pipeline Integration
**Goal:** Integrate KRY files into existing build system

#### Step 5.1: Update Build Scripts
- Modify `01_compile_kir.sh` to process KRY files
- Update `03_codegen_kry.sh` for new structure
- Ensure KRY files are recognized alongside Lua

**Verification:**
- Build script processes KRY files
- No errors in build pipeline
- Output generated correctly

#### Step 5.2: Testing & Validation
- Run `./scripts/build_all.sh`
- Run `./scripts/verify.sh`
- Test application on desktop
- Test web build if applicable

**Verification:**
- All build steps pass
- Application runs without errors
- UI matches original functionality
- No regressions in behavior

---

## KRY File Structure Template

Each KRY file will follow this structure:

```kry
-- Lua module imports (if needed)
local utils = require("kry_components/calendar_utils")

-- Component definition
ComponentName {
  -- Static properties
  width = 800
  height = 400
  background = "#1a1a1a"

  -- Dynamic properties using Lua
  text = utils.getDynamicText()

  -- Child components
  Row {
    Button {
      text = "Click Me"
      onClick = utils.handleClick()
    }
  }

  -- Embedded Lua for complex logic
  -- Lua functions defined in separate module
}
```

## Lua Logic Modules

Alongside each `.kry` file, create a `_logic.lua` file:

```
kry_components/
  color_palette.kry       # UI structure
  color_palette_logic.lua # Business logic
```

## Success Criteria

✅ Each phase completes successfully before moving to next
✅ No functionality regressions
✅ Build pipeline processes KRY files correctly
✅ Application works on desktop target
✅ Code is maintainable and well-structured

## Order of Conversion (Recommended)

1. **color_palette** (easiest - no UI)
2. **calendar** (easy - utility functions)
3. **color_picker** (medium - simple modal)
4. **tabs** (medium - dynamic list)
5. **habit_panel** (hard - complex component)
6. **main** (hardest - orchestrates everything)

---

## Next Steps

1. Review and approve this plan
2. Begin Phase 1, Step 1.1
3. Verify each step before proceeding
4. Document any deviations or issues encountered
