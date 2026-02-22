# 9P Protocol Specification

## Table of Contents

1. [Wire Format](#wire-format)
2. [Message Types](#message-types)
3. [Kryon-Specific Behaviors](#kryon-specific-behaviors)
4. [Error Codes](#error-codes)
5. [Concurrency Model](#concurrency-model)

---

## Wire Format

All 9P messages follow this structure:

```
┌──────────┬───────┬─────┬─────────┐
│  Size    │ Type  │ Tag │  Data   │
│  4 bytes │ 1 byte│2 bts│ variable│
└──────────┴───────┴─────┴─────────┘

Size:  Total message size including this field (little-endian)
Type:  Message type (see below)
Tag:   Transaction ID (for matching request/response)
Data:  Message-specific data
```

### Size Field

- **Format**: 32-bit unsigned integer, little-endian
- **Value**: Total bytes in message, including the 4-byte size field
- **Max**: 2^24 bytes (16 MB) per message

### Type Field

Valid message types (see [Message Types](#message-types) below)

### Tag Field

- **NOTAG** (0xFFFF): No tag expected (for messages that don't need response)
- **Valid Tags**: 0 to 0xFFFE
- **Allocation**: Client allocates tags, server echoes in response

---

## Message Types

### Standard 9P2000 Messages

Kryon uses the 9P2000 variant with standard message types:

#### Client → Server (T-messages)

| Type | Name      | Description                           |
|------|-----------|---------------------------------------|
| 6    | Tattach   | Attach to root directory              |
| 8    | Tauth     | Authenticate (future)                 |
| 10   | Tcreate   | Create file/directory                 |
| 12   | Tread     | Read from file                        |
| 14   | Twrite    | Write to file                         |
| 16   | Tclunk    | Release FID                           |
| 18   | Trename   | Rename file (future)                  |
| 20   | Tremove   | Remove file/directory                 |
| 22   | Tstat     | Get file metadata                     |
| 24   | Twstat    | Modify file metadata                  |
| 26   | Tflush    | Abort in-progress request             |
| 28   | Twalk     | Walk directory path                   |
| 30   | Topen     | Open existing file                    |
| 32   | Tversion  | Negotiate protocol version            |

#### Server → Client (R-messages)

| Type | Name      | Description                           |
|------|-----------|---------------------------------------|
| 7    | Rattach   | Attach response                       |
| 9    | Rauth     | Auth response                         |
| 11   | Rcreate   | Create response                       |
| 13   | Rread     | Read response                         |
| 15   | Rwrite    | Write response                        |
| 17   | Rclunk    | Clunk response                        |
| 19   | Rrename   | Rename response                       |
| 21   | Rremove   | Remove response                       |
| 23   | Rstat     | Stat response                         |
| 25   | Rwstat    | Wstat response                        |
| 27   | Rflush    | Flush response                        |
| 28   | Rwalk     | Walk response                         |
| 31   | Ropen     | Open response                         |
| 33   | Rversion  | Version negotiation response          |
| 34   | Rerror    | Error response                        |

---

## Kryon-Specific Behaviors

### Tversion / Rversion

**Client Request**:
```
size[4] Tversion[1] tag[2] msize[4] version[s]
```

**Response**:
```
size[4] Rversion[1] tag[2] msize[4] version[s]
```

**Kryon Behavior**:
- Client must request `"9P2000"` version
- Server responds with `"9P2000"` (string)
- `msize`: Maximum message size (recommended: 65536)

### Tattach / Rattach

**Client Request**:
```
size[4] Tattach[1] tag[2] fid[4] afid[4] uname[s] aname[s]
```

**Parameters**:
- `fid`: FID for root directory
- `afid`: Auth FID (use NOFID = 0xFFFFFFFF for no auth)
- `uname`: User name (can be empty for local connections)
- `aname`: Attach name (should be empty for Kryon)

**Response**:
```
size[4] Rattach[1] tag[2] qid[13]
```

**Kryon Behavior**:
- Server grants access to root directory
- Root QID type: `QTDIR` (0x80)
- Root QID version: Incremented on structural changes

### Twalk / Rwalk

**Client Request**:
```
size[4] Twalk[1] tag[2] fid[4] newfid[4] nwname[2] wname[nwname][s]
```

**Parameters**:
- `fid`: Current directory FID
- `newfid`: FID for destination (can be same as `fid` to "clone")
- `nwname`: Number of path components
- `wname`: Path components (e.g., `["windows", "1", "widgets", "5"]`)

**Response**:
```
size[4] Rwalk[1] tag[2] nwqid[2] wqid[nwqid][13]
```

**Kryon Behavior**:
- Each path component returns a QID
- Partial walks succeed (returns fewer QIDs if intermediate components don't exist)
- Walking to `/windows/999` returns error if window doesn't exist

### Topen / Ropen

**Client Request**:
```
size[4] Topen[1] tag[2] fid[4] mode[1]
```

**Modes**:
- `OREAD` (0): Read-only
- `OWRITE` (1): Write-only
- `ORDWR` (2): Read/write
- `OEXEC` (3): Execute (not used in Kryon)
- `OTRUNC` (16): Truncate on open (for write-only files)
- `ORCLOSE` (64): Remove on close (not used in Kryon)

**Response**:
```
size[4] Ropen[1] tag[2] qid[13] iounit[4]
```

**Kryon Behavior**:
- `iounit`: 0 (no recommended I/O size, or use msize-24)
- Event pipes: Block until event available
- Property files: Immediate read of current value

### Tcreate / Rcreate

**Client Request**:
```
size[4] Tcreate[1] tag[2] fid[4] name[s] perm[4] mode[1] extension[s]
```

**Response**:
```
size[4] Rcreate[1] tag[2] qid[13] iounit[4]
```

**Kryon Behavior**:
- **Not used directly**: Widget creation via `/ctl` command pipe
- Server rejects `Tcreate` for most paths (use `create_widget` command instead)
- Future: May allow `Tcreate` for user-defined files

### Tread / Rread

**Client Request**:
```
size[4] Tread[1] tag[2] fid[4] offset[8] count[4]
```

**Parameters**:
- `offset`: Byte offset in file
- `count`: Maximum bytes to read

**Response**:
```
size[4] Rread[1] tag[2] count[4] data[count]
```

**Kryon Behaviors by File Type**:

**Property Files** (`/windows/*/widgets/*/text`, `value`, `rect`, etc.):
- `offset`: Should be 0 for properties (rewind on read)
- Returns current value as string
- Examples: `"800x600"`, `"Hello, World!"`, `"42"`

**Event Pipes** (`/windows/*/event`, `/windows/*/widgets/*/event`):
- `offset`: Ignored (pipes don't support seek)
- **Blocking**: Waits until event available
- Returns one complete event per read (newline-terminated)
- Examples: `"clicked x=150 y=32 button=1"`, `"changed value=42"`

**Control Pipe** (`/ctl`):
- Returns nothing (write-only)
- Always error on read

**Version File** (`/version`):
- Returns `"Kryon 1.0\n"` (or current version)

### Twrite / Rwrite

**Client Request**:
```
size[4] Twrite[1] tag[2] fid[4] offset[8] count[4] data[count]
```

**Parameters**:
- `offset`: Byte offset (usually 0)
- `count`: Bytes to write
- `data`: Data to write

**Response**:
```
size[4] Rwrite[1] tag[2] count[4]
```

**Kryon Behaviors by File Type**:

**Property Files**:
- `offset`: Should be 0 (replace entire value)
- Validates data before accepting
- Returns count on success
- Triggers redraw if property affects appearance

**Event Pipes**:
- Always error (read-only)

**Control Pipe** (`/ctl`):
- Executes commands (one per line or complete message)
- Returns count on success
- Commands: `create_window`, `destroy_window`, `create_widget`, etc.

### Tclunk / Rclunk

**Client Request**:
```
size[4] Tclunk[1] tag[2] fid[4]
```

**Response**:
```
size[4] Rclunk[1] tag[2]
```

**Kryon Behavior**:
- Releases FID
- If last writer to property file, flushes pending updates
- If event pipe, stops event generation

### Tremove / Rremove

**Client Request**:
```
size[4] Tremove[1] tag[2] fid[4]
```

**Response**:
```
size[4] Rremove[1] tag[2]
```

**Kryon Behavior**:
- **Not used directly**: Widget/window removal via `/ctl` commands
- Server rejects `Tremove` for most paths
- Future: May allow `Tremove` for user-created resources

### Tstat / Rstat, Twstat / Rwstat

**Tstat Request**:
```
size[4] Tstat[1] tag[2] fid[4]
```

**Response**:
```
size[4] Rstat[1] tag[2] stat[n]
```

**Stat Structure**:
```
size[2] type[2] dev[4] qid[13] mode[4] atime[4] mtime[4] length[8] name[s] uid[s] gid[s] muid[s]
```

**Kryon Behavior**:
- `type`: Always 0 (no device)
- `dev`: Always 0
- `qid`: Unique ID (path + version)
- `mode`: File permissions (see [Permissions](03-permissions.md))
- `atime`: Current time (access time not tracked)
- `mtime`: Last modification time
- `length`: File size in bytes
- `name`: Filename
- `uid`, `gid`, `muid`: Owner/group (usually empty for local)

---

## Error Codes

Kryon uses standard POSIX error codes plus custom codes for UI-specific errors.

### Standard POSIX Errors

| Code | Name          | Description                                   |
|------|---------------|-----------------------------------------------|
| 1    | EPERM         | Operation not permitted                       |
| 2    | ENOENT        | No such file or directory                     |
| 5    | EIO           | I/O error                                     |
| 9    | EBADF         | Bad file descriptor                           |
| 13   | EACCES        | Permission denied                             |
| 14   | EFAULT        | Bad address                                   |
| 16   | EBUSY         | Device or resource busy                       |
| 17   | EEXIST        | File exists                                   |
| 19   | ENODEV        | No such device                                |
| 20   | ENOTDIR       | Not a directory                               |
| 21   | EISDIR        | Is a directory                                |
| 22   | EINVAL        | Invalid argument                              |
| 24   | EMFILE        | Too many open files                           |
| 28   | ENOSPC        | No space left on device                       |
| 30   | EROFS         | Read-only filesystem                          |
| 32   | EPIPE         | Broken pipe                                   |
| 60   | ENAMETOOLONG  | File name too long                            |
| 95   | ENOTSUP       | Operation not supported                       |

### Kryon-Specific Errors (100+)

| Code | Name             | Description                                   |
|------|------------------|-----------------------------------------------|
| 100  | EINVALIDWIDGET   | Invalid widget type name                      |
| 101  | EINVALIDPROP     | Invalid property name for widget type         |
| 102  | EREADONLY        | Property is read-only                         |
| 103  | EOUTOFRANGE      | Value out of valid range                      |
| 104  | EDUPLICATE       | Duplicate widget ID                           |
| 105  | EHWND            | Invalid window handle                         |
| 106  | ENOTIMPLEMENTED  | Feature not yet implemented                   |
| 107  | EINVALIDLAYOUT   | Invalid layout configuration                  |
| 108  | EINVALIDPARENT   | Parent widget doesn't exist or can't have children |
| 109  | ECYCLIC          | Cyclic dependency in widget tree              |
| 110  | EMAXWINDOWS      | Maximum window limit reached                  |
| 111  | EMAXWIDGETS      | Maximum widgets per window reached            |
| 112  | EMAXDEPTH        | Maximum nesting depth reached                 |
| 113  | EINVALIDSTATE    | Invalid window state (not normal/min/max/fullscreen) |
| 114  | ENOTMODAL        | Operation only valid for modal windows        |
| 115  | EHASPARENT       | Window already has a parent                   |
| 116  | EBADRECT         | Invalid rectangle format or values            |
| 117  | EBADCOLOR        | Invalid color format                          |
| 118  | EBADFONTSPEC     | Invalid font specification                    |
| 119  | EOVFLOW          | Event queue overflow                          |

### Rerror Message Format

```
size[4] Rerror[1] tag[2] ename[s]
```

**ename**: Human-readable error message (e.g., `"No such file or directory"`)

---

## Concurrency Model

### FID Lifecycle

```
Unused → Open → Active → Clunked → Released
          ↑        │
          └────────┘ (reuse)
```

1. **Twalk**: Creates new FID in Open state
2. **Topen/Tcreate**: Transitions to Active state
3. **Tclunk**: Transitions to Clunked state
4. **Implicit Release**: After Rclunk, FID can be reused

### Lock Ordering

To prevent deadlocks, acquire locks in this order:

1. Root directory lock
2. Window directory lock (if accessing `/windows/<id>`)
3. Widget directory lock (if accessing `/windows/<id>/widgets/<wid>`)
4. Property file lock (if accessing specific property)

**Never**:
- Hold multiple property locks simultaneously
- Hold a lock while making 9P call to another server
- Acquire locks out of order

### Atomic Operation Guarantees

- **Single Property Write**: Atomic (within single Twrite)
- **Widget Creation**: Atomic (create_widget creates entire widget)
- **Window Creation**: Atomic (create_window creates entire directory)
- **Multi-Property Updates**: NOT atomic (use transactions in future)

### Event Delivery Ordering

Events are delivered in order of occurrence:

1. Events queued per-widget
2. Readers receive events in FIFO order
3. Global event pipe receives events after widget event pipes
4. No event dropping (blocking writes if queue full)

---

## Implementation Notes

### Message Size Limits

- **Recommended msize**: 65536 bytes (64 KB)
- **Maximum msize**: 16777216 bytes (16 MB)
- **Property values**: Should fit in msize (avoid multi-MB properties)

### String Handling

- **Encoding**: UTF-8 for all text
- **Length**: 2-byte length prefix for counted strings
- **Null Termination**: Not used (length-prefixed)

### QID Versioning

- **Directories**: Version increments when children added/removed
- **Properties**: Version increments when value changes
- **Event Pipes**: Version always 0 (stream, not snapshot)

### Backpressure

- **Event Pipes**: Use non-blocking writes
- **Queue Full**: Drop oldest events or block controller
- **Recommended**: Controller should always drain event pipe

---

## See Also

- [Filesystem Namespace](02-filesystem-namespace.md) - Directory layout
- [Error Handling](04-error-handling.md) - Detailed error scenarios
- [Multi-Window Architecture](05-multi-window.md) - Window management
