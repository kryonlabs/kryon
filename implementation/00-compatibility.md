# Implementation Compatibility

## Table of Contents

1. [Cross-Platform Requirements](#cross-platform-requirements)
2. [Behavioral Specifications](#behavioral-specifications)
3. [Rendering Differences](#rendering-differences)
4. [Performance Expectations](#performance-expectations)
5. [Testing Strategy](#testing-strategy)
6. [Compliance Checklist](#compliance-checklist)

---

## Cross-Platform Requirements

### Mandatory Behaviors

All Kryon implementations MUST:

1. **Speak 9P2000**: Standard 9P2000 protocol
2. **Expose Filesystem**: Same directory structure under `/`
3. **Support All Widgets**: All 89 widgets must be available
4. **Generate Events**: All events as specified
5. **Validate Properties**: Same validation rules
6. **Handle Errors**: Same error codes

### Platform-Specific Flexibility

Implementations MAY:

1. **Rendering**: Use any rendering technology (libdraw, MirageFB, Canvas, etc.)
2. **Event Loop**: Single-threaded, multi-threaded, or cooperative
3. **Windowing**: Platform window manager integration
4. **Fonts**: Platform font loading
5. **Threading**: Any threading model

### Constraints

Implementations MUST NOT:

1. **Modify Protocol**: No extensions to 9P protocol
2. **Change Filesystem**: Different directory structure
3. **Omit Widgets**: All widgets must be implemented
4. **Change Event Format**: Event format must match spec
5. **Different Types**: Property types must match

---

## Behavioral Specifications

### Property Write Behavior

**All Implementations**:

1. **Atomic**: Single property write is atomic
2. **Validated**: All writes validated before acceptance
3. **Event Generation**: Valid writes generate `property_changed` event
4. **Error Response**: Invalid writes return Rerror

**Example**:
```bash
# This sequence must be identical on all platforms
echo "invalid" > /windows/1/widgets/5/value
# Response: Rerror with EOUTOFRANGE
# No property change
# No event generated
```

### Event Delivery Behavior

**All Implementations**:

1. **Ordered**: Events delivered in occurrence order
2. **No Dropping**: Events never dropped (except throttled mouse_move)
3. **Blocking**: Event pipes block until event available
4. **Complete**: Each read returns one complete event

**Example**:
```bash
# Must read events in this exact order
cat /windows/1/widgets/5/event
# Output: mouse_enter x=10 y=10
# (blocks until next event)
# Output: clicked x=50 y=20 button=1
```

### Widget Lifecycle Behavior

**All Implementations**:

1. **Creation**: `create_widget` creates all files atomically
2. **Destruction**: `destroy_widget` removes directory and children
3. **Visibility**: `visible=0` stops rendering and events
4. **Enabled**: `enabled=0` grays out and blocks input events

### Layout Behavior

**All Implementations**:

1. **Identical Layout**: Same input → same widget positions
2. **Size Negotiation**: Same min/max/pref sizes
3. **Overflow**: Same overflow handling
4. **Alignment**: Same alignment behavior

**Test Case**:
```
Input: Container (layout: column, gap: 10, padding: 10)
       Children: 3 labels (height: 20 each)
Output: Labels at y=10, y=40, y=70
```

---

## Rendering Differences

### Allowed Differences

Platforms may differ in:

1. **Font Rendering**: Anti-aliasing, hinting, fallback fonts
2. **Color Precision**: 8-bit vs 10-bit color
3. **Border Width**: +/- 1 pixel tolerance
4. **Shadow/Elevation**: Platform-specific appearance
5. **Animation**: Different timing/framerate

### Prohibited Differences

Platforms must NOT differ in:

1. **Widget Position**: Exact positions (within rounding)
2. **Widget Size**: Exact sizes (within rounding)
3. **Text Content**: Exact text strings
4. **Visibility**: Visible/hidden state
5. **Colors**: Exact RGB values (within display gamut)

### Tolerances

**Position/Size**: +/- 1 pixel (rounding differences)
**Colors**: Delta-E < 2.0 (imperceptible difference)
**Timing**: +/- 16ms (one frame at 60fps)

---

## Performance Expectations

### Minimum Performance

All implementations must meet:

1. **Create Widget**: < 10ms
2. **Property Write**: < 1ms
3. **Event Delivery**: < 1ms latency
4. **Render Frame**: < 16ms (60 FPS)
5. **Window Creation**: < 100ms

### Memory Limits

**Recommended**:
- Per widget: < 1 KB (excluding buffers)
- Per window: < 10 MB (including all widgets)
- Max windows: 1024
- Max widgets per window: 65536

### Scalability

Implementations must handle:

1. **1000 Widgets**: Smooth 60 FPS
2. **10 Windows**: No performance degradation
3. **100 Events/sec**: No event loss
4. **Continuous Property Updates**: No memory leaks

---

## Testing Strategy

### Unit Tests

**Required**:
- Property validation
- Error code generation
- Event parsing
- Type checking

### Integration Tests

**Required**:
- 9P protocol compliance
- Widget creation/destruction
- Event delivery
- Multi-window scenarios

### Rendering Tests

**Required**:
- Screenshot comparison (with tolerance)
- Layout verification
- Animation timing

### Platform-Specific Tests

**9front**:
- libdraw integration
- /dev/draw compatibility
- Process communication

**MirageOS**:
- mirage-framebuffer integration
- Unikernel resource limits
- Lwt threading

### Conformance Tests

**Shared Test Suite**:
```
tests/
├── protocol/       # 9P protocol tests
├── widgets/        # Widget behavior tests
├── events/         # Event delivery tests
├── layout/         # Layout algorithm tests
└── compliance/     # Platform compliance tests
```

---

## Compliance Checklist

### Protocol Compliance

- [ ] Implements 9P2000
- [ ] Supports all message types (Tversion, Tattach, Twalk, Topen, Tread, Twrite, etc.)
- [ ] Returns correct error codes
- [ ] Filesystem matches specification
- [ ] Control pipe accepts all commands
- [ ] Event pipes deliver events correctly

### Widget Compliance

- [ ] All 89 widgets implemented
- [ ] All mandatory properties present
- [ ] All widget-specific properties present
- [ ] All events generated
- [ ] Property validation matches spec
- [ ] Layout behavior matches spec

### Language Compliance

- [ ] Parser accepts full grammar
- [ ] Type checker enforces types
- [ ] Code generator produces valid 9P operations
- [ ] Standard library functions work
- [ ] Component system works
- [ ] Module system works

### Performance Compliance

- [ ] Meets minimum performance targets
- [ ] No memory leaks
- [ ] No event loss
- [ ] Handles 1000+ widgets
- [ ] Handles 10+ windows

### Documentation Compliance

- [ ] API documented
- [ ] Examples provided
- [ ] Platform differences documented
- [ ] Limitations documented

---

## Versioning

### Semantic Versioning

**Format**: `MAJOR.MINOR.PATCH`

- **MAJOR**: Breaking changes to protocol or widget set
- **MINOR**: New widgets, new features
- **PATCH**: Bug fixes, performance improvements

### Compatibility Matrix

| Server Version | Controller Version | Compatible |
|----------------|-------------------|------------|
| 1.0            | 1.0               | Yes        |
| 1.0            | 0.9               | No         |
| 1.1            | 1.0               | Yes        |
| 1.1            | 1.1               | Yes        |
| 2.0            | 1.x               | No         |

### Feature Detection

Controllers should detect version:

```bash
cat /version
# Output: "Kryon 1.0"
```

---

## Future Compatibility

### Planned Additions

**Future versions may add**:
- New widgets
- New properties
- New events
- New language features

**Backward Compatibility**:
- Old widgets never removed
- Old properties never removed
- Old events never removed
- Old syntax always supported

### Deprecation Process

1. **Announce**: Document as deprecated
2. **Wait**: At least one major version
3. **Remove**: Only in major version bump

---

## See Also

- [9P Specification](../protocol/01-9p-specification.md) - Protocol details
- [Widget Catalog](../widgets/04-widget-catalog.md) - Widget list
- [Error Handling](../protocol/04-error-handling.md) - Error codes
