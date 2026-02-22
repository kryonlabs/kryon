# Error Handling

## Table of Contents

1. [Error Codes](#error-codes)
2. [Error Message Format](#error-message-format)
3. [Error Propagation](#error-propagation)
4. [Recovery Strategies](#recovery-strategies)
5. [Partial Failure Handling](#partial-failure-handling)

---

## Error Codes

Kryon uses standard POSIX error codes (1-95) and Kryon-specific codes (100+).

### POSIX Errors

| Code | Name          | Common Causes in Kryon                     |
|------|---------------|---------------------------------------------|
| 1    | EPERM         | Attempting operation without permissions    |
| 2    | ENOENT        | Path doesn't exist (window/widget deleted)  |
| 5    | EIO           | Renderer crashed, framebuffer lost         |
| 9    | EBADF         | Invalid FID (already clunked)               |
| 13   | EACCES        | Permission denied (read-only file)          |
| 14   | EFAULT        | Invalid pointer (implementation bug)        |
| 16   | EBUSY         | Widget locked by another operation          |
| 17   | EEXIST        | Widget ID already exists                    |
| 19   | ENODEV        | Renderer not available                      |
| 20   | ENOTDIR       | Path component not a directory              |
| 21   | EISDIR        | Attempted read on directory                 |
| 22   | EINVAL        | Invalid argument (malformed command)        |
| 24   | EMFILE        | Too many open FIDs                          |
| 28   | ENOSPC        | Memory allocation failed                    |
| 30   | EROFS         | Write to read-only file                     |
| 32   | EPIPE         | Event pipe closed (renderer crashed)        |
| 60   | ENAMETOOLONG  | Path exceeds 4096 characters                |
| 95   | ENOTSUP       | Operation not implemented                   |

### Kryon-Specific Errors (100+)

| Code | Name             | Common Causes                             |
|------|------------------|-------------------------------------------|
| 100  | EINVALIDWIDGET   | Unknown widget type name                   |
| 101  | EINVALIDPROP     | Property doesn't exist for widget type     |
| 102  | EREADONLY        | Write to read-only property                |
| 103  | EOUTOFRANGE      | Value outside valid range                  |
| 104  | EDUPLICATE       | Widget ID already in use                   |
| 105  | EHWND            | Window doesn't exist                      |
| 106  | ENOTIMPLEMENTED  | Feature not yet implemented                |
| 107  | EINVALIDLAYOUT   | Layout configuration error                 |
| 108  | EINVALIDPARENT   | Parent doesn't exist or can't have kids    |
| 109  | ECYCLIC          | Widget tree would contain cycle            |
| 110  | EMAXWINDOWS      | Maximum window limit reached               |
| 111  | EMAXWIDGETS      | Maximum widgets per window reached         |
| 112  | EMAXDEPTH        | Maximum nesting depth exceeded             |
| 113  | EINVALIDSTATE    | Invalid window state string                |
| 114  | ENOTMODAL        | Operation requires modal window            |
| 115  | EHASPARENT       | Window already has a parent                |
| 116  | EBADRECT         | Invalid rectangle format or negative size  |
| 117  | EBADCOLOR        | Invalid color format (not #RRGGBB)         |
| 118  | EBADFONTSPEC     | Invalid font specification                 |
| 119  | EOVFLOW          | Event queue overflow (dropped events)      |

### Error Code Ranges

| Range    | Category              | Example                   |
|----------|-----------------------|---------------------------|
| 1-99     | Standard POSIX        | ENOENT, EACCES            |
| 100-149  | Widget errors         | EINVALIDWIDGET, EINVALIDPROP |
| 150-199  | Layout errors         | EINVALIDLAYOUT, ECYCLIC   |
| 200-249  | Window errors         | EHWND, EMAXWINDOWS        |
| 250-299  | Resource limits       | EMAXWIDGETS, EOVFLOW      |
| 300-999  | Reserved for future   | -                         |

---

## Error Message Format

### Rerror Message

```
size[4] Rerror[1] tag[2] ename[s]
```

**ename**: Human-readable error string

**Format**: `<error text>` (no error code in message)

**Examples**:
```
"No such file or directory"
"Permission denied"
"Invalid widget type: 'foobar'"
"Value out of range: 150 (valid: 0-100)"
```

### Error Message Guidelines

**Do**:
- Use clear, simple language
- Include relevant values (e.g., actual value, valid range)
- Reference the operation that failed

**Don't**:
- Include implementation details
- Expose internal paths or structure
- Include stack traces (security risk)

### Good Error Messages

```
# Bad
"Error 100"

# Good
"Invalid widget type: 'magicbutton' (valid: button, textinput, label)"

# Bad
"EINVAL"

# Good
"Invalid rectangle format: '10 20' (expected: 'x y width height')"
```

---

## Error Propagation

### Widget → Window → Root

Errors propagate up the hierarchy:

```
Widget Error
    │
    ▼
Window Error (if widget error affects window)
    │
    ▼
Global Error (if window error affects server)
```

### Example: Widget Creation Failure

**Scenario**: Creating widget with invalid parent

```bash
echo "create_widget 1 button 999 text=Click" > /ctl
```

**Flow**:
1. **Controller**: Sends `create_widget` command
2. **Server** (root level): Validates window exists → OK
3. **Server** (window level): Looks up parent widget 999 → Not found
4. **Error propagates**: `ENOENT` (No such widget)
5. **Response**: `Rerror` with "No such widget: 999"

### Example: Property Write Failure

**Scenario**: Writing invalid value to property

```bash
echo "invalid" > /windows/1/widgets/5/value
```

**Flow**:
1. **Controller**: Opens `/windows/1/widgets/5/value` for write
2. **Server**: Validates widget exists → OK
3. **Controller**: Writes "invalid"
4. **Server**: Validates property (value must be int) → Error
5. **Response**: `Rerror` with "Invalid value format"

### Error Context

Errors include context where possible:

```
# Path-based error
"No such file: /windows/99/widgets/5/value"
  └─ Shows exactly which path failed

# Property-based error
"Invalid value for 'width': -10 (must be > 0)"
  └─ Shows property name and why it failed

# Range-based error
"Value out of range: 150 (valid: 0-100)"
  └─ Shows actual and valid range
```

---

## Recovery Strategies

### Controller-Side Recovery

**Retry with Backoff**:
```lua
-- Retry transient errors
local max_retries = 3
local retry_delay = 1000  -- 1 second

for attempt = 1, max_retries do
    local ok, err = kryon.set_property(widget_id, "text", "Hello")
    if ok then
        break
    elseif err == "EBUSY" or err == "EIO" then
        -- Retry after delay
        os.sleep(retry_delay * attempt)
    else
        -- Non-retryable error
        error("Failed: " .. err)
    end
end
```

**Exponential Backoff**:
```lua
local delay = 100  -- Start at 100ms
while true do
    local ok, err = kryon.wait_event()
    if ok then break end
    if err == "EOVFLOW" then
        -- Event queue full, wait and retry
        os.sleep(delay)
        delay = delay * 2  -- Exponential backoff
    else
        error(err)
    end
end
```

### Server-Side Recovery

**Widget Destruction**:
- If widget becomes corrupted (invalid state), destroy it
- Return error to controller
- Remove widget from filesystem

**Window Recovery**:
- If window crashes (renderer failure), recreate window
- Attempt to restore widget tree
- Notify controller of recovery

**Server Restart**:
- Last resort: Shut down server
- Controller detects disconnect
- Controller reconnects and rebuilds UI

### Idempotent Operations

**Safe to Retry**:
- `focus_window <id>` → Same result if repeated
- `refresh` → Idempotent
- Property writes (if value unchanged)

**Not Safe to Retry**:
- `create_window` → Creates duplicate window
- `create_widget` → Creates duplicate widget
- `destroy_window` → Error on second attempt

### Transaction Support (Future)

**Atomic Multi-Property Updates**:
```bash
# Future syntax
echo "transaction" > /ctl
echo "text=Hello" > /windows/1/widgets/5/text
echo "value=42" > /windows/1/widgets/5/value
echo "commit" > /ctl
# Either both succeed or both fail
```

---

## Partial Failure Handling

### Widget Tree Failure

**Scenario**: Some widgets fail to initialize

**Strategy**:
- Initialize widgets one by one
- Continue on failure (best-effort)
- Report which widgets failed
- Mark failed widgets as error state

**Example**:
```bash
# Create 10 widgets, widget 7 fails
echo "create_widget 1 button 0 text=Button1" > /ctl  # OK
# ...
echo "create_widget 1 button 0 text=Button7" > /ctl  # FAILS
echo "create_widget 1 button 0 text=Button8" > /ctl  # OK (continues)
# ...

# Response: "Created 9 widgets, failed: widget 7 (ENOENT)"
```

### Event Delivery Failure

**Scenario**: Event pipe full, can't deliver event

**Strategy**:
- Block writes (don't drop events)
- Backpressure to event source
- Controller must drain pipe

**Alternative (Configurable)**:
- Drop oldest events
- Log dropped events
- Notify controller

### Property Update Failure

**Scenario**: Property write fails during batch update

**Strategy**:
- Stop batch update on first error
- Rollback completed updates (if possible)
- Return error to controller

**Future**: Transactions for atomicity

### Renderer Crash Recovery

**Scenario**: Renderer process crashes

**Detection**: Server loses connection to renderer

**Recovery**:
1. Server detects renderer death
2. Mark all windows as "not rendered"
3. Attempt to restart renderer
4. If restart succeeds, redraw all windows
5. If restart fails, notify controllers

**Controller Action**:
- Read `/windows/*/state` to detect renderer failure
- Optionally reconnect or restart server

---

## Error Handling Best Practices

### For Controllers

1. **Always Check Errors**:
   ```lua
   local ok, err = kryon.set_property(id, "text", "Hello")
   if not ok then
       log("Error: " .. err)
       return
   end
   ```

2. **Handle Expected Errors**:
   ```lua
   local ok, err = kryon.create_window()
   if err == "EMAXWINDOWS" then
       log("Too many windows, reusing existing")
       -- Reuse existing window
   elseif err then
       error(err)
   end
   ```

3. **Use Timeouts**:
   ```lua
   -- Don't block forever
   local ev = kryon.wait_event(timeout=5000)
   if not ev then
       log("Timeout waiting for event")
       return
   end
   ```

4. **Validate Before Sending**:
   ```lua
   -- Validate locally before sending to server
   if value < 0 or value > 100 then
       error("Value out of range: " .. value)
   end
   kryon.set_property(id, "value", value)
   ```

### For Server Implementations

1. **Validate All Input**:
   - Check property types
   - Check ranges
   - Check permissions

2. **Fail Fast**:
   - Detect errors early
   - Don't propagate invalid state

3. **Provide Context**:
   - Include path in error messages
   - Show valid ranges
   - Explain what went wrong

4. **Log Errors**:
   - Record errors for debugging
   - Don't expose internal state in messages

---

## Testing Error Handling

### Unit Tests

- Test each error code
- Test error message format
- Test error propagation

### Integration Tests

- Test controller error handling
- Test retry logic
- Test timeout handling

### Chaos Testing

- Randomly kill renderer
- Randomly inject errors
- Test recovery

---

## See Also

- [9P Specification](01-9p-specification.md) - Rerror format
- [Permissions](03-permissions.md) - Access control errors
- [Filesystem Namespace](02-filesystem-namespace.md) - Path resolution errors
