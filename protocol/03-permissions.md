# Permissions and Security

## Table of Contents

1. [Permission Bits](#permission-bits)
2. [Default Permissions](#default-permissions)
3. [Access Control Model](#access-control-model)
4. [Multi-Tenant Isolation](#multi-tenant-isolation)
5. [Rate Limiting](#rate-limiting)

---

## Permission Bits

Kryon uses Unix-style permission bits (octal mode).

### Mode Format

```
rwxrwxrwx
│││││││││└─ Others (all clients)
│││││││├─ Group (optional groups)
││││││├─ User (owner)
││││││└── Execute/Search
││││├──── Write
│││└───── Read
││└────── Execute/Search
│├─────── Write
└──────── Read

Special bits:
setuid (04000), setgid (02000), sticky (01000)
```

### Octal Values

| Octal | Binary | Meaning           |
|-------|--------|-------------------|
| 0     | 000    | No permissions    |
| 1     | 001    | Execute only      |
| 2     | 010    | Write only        |
| 3     | 011    | Write + Execute   |
| 4     | 100    | Read only         |
| 5     | 101    | Read + Execute    |
| 6     | 110    | Read + Write      |
| 7     | 111    | Read + Write + Execute |

### Common Permissions

| Octal | Symbolic | Meaning                              |
|-------|----------|--------------------------------------|
| 000   | --------- | No access (not used in Kryon)       |
| 444   | r--r--r-- | Read-only for all                   |
| 644   | rw-r--r-- | Read-write for owner, read for others |
| 755   | rwxr-xr-x | Full access for owner, read/execute for others |
| 222   | -w--w--w- | Write-only for all (rare)           |

---

## Default Permissions

### Root Directory Files

| Path              | Permissions | Type      | Description                              |
|-------------------|-------------|-----------|------------------------------------------|
| `/`               | 0755        | directory | Root directory                           |
| `/version`        | 0444        | file      | Read-only version string                 |
| `/ctl`            | 0222        | pipe      | Write-only command pipe                  |
| `/events`         | 0444        | pipe      | Read-only event pipe                     |
| `/windows/`       | 0755        | directory | Window directory                         |
| `/themes/`        | 0755        | directory | Theme resources (optional)               |

### Window Files

| Path                         | Permissions | Type      | Description                              |
|------------------------------|-------------|-----------|------------------------------------------|
| `/windows/<id>/`             | 0755        | directory | Window directory                         |
| `/windows/<id>/type`         | 0444        | file      | Read-only type identifier                |
| `/windows/<id>/title`        | 0644        | file      | Window title (read/write)                |
| `/windows/<id>/rect`         | 0644        | file      | Window geometry (read/write)             |
| `/windows/<id>/visible`      | 0644        | file      | Visibility (read/write)                  |
| `/windows/<id>/focused`      | 0444        | file      | Focus state (read-only)                  |
| `/windows/<id>/state`        | 0644        | file      | Window state (read/write)                |
| `/windows/<id>/resizable`    | 0644        | file      | Resizable flag (read/write)              |
| `/windows/<id>/decorations`  | 0644        | file      | Decorations flag (read/write)            |
| `/windows/<id>/always_on_top`| 0644        | file      | Always-on-top flag (read/write)          |
| `/windows/<id>/modal`        | 0444        | file      | Modal flag (read-only)                   |
| `/windows/<id>/parent_window`| 0444        | file      | Parent window ID (read-only)             |
| `/windows/<id>/event`        | 0444        | pipe      | Window event pipe (read-only)            |
| `/windows/<id>/widgets/`     | 0755        | directory | Widget directory                         |

### Widget Files

| Path                                       | Permissions | Type      | Description                              |
|--------------------------------------------|-------------|-----------|------------------------------------------|
| `/windows/<wid>/widgets/<id>/`             | 0755        | directory | Widget directory                         |
| `/windows/<wid>/widgets/<id>/type`         | 0444        | file      | Widget type (read-only)                  |
| `/windows/<wid>/widgets/<id>/rect`         | 0644        | file      | Geometry (read/write)                    |
| `/windows/<wid>/widgets/<id>/visible`      | 0644        | file      | Visibility (read/write)                  |
| `/windows/<wid>/widgets/<id>/enabled`      | 0644        | file      | Enabled state (read/write)               |
| `/windows/<wid>/widgets/<id>/focused`      | 0444        | file      | Focus state (read-only)                  |
| `/windows/<wid>/widgets/<id>/event`        | 0444        | pipe      | Widget event pipe (read-only)            |
| `/windows/<wid>/widgets/<id>/<property>`   | varies      | file      | Widget-specific properties              |

### Property File Permissions

| Property Type | Permissions | Reason                                   |
|---------------|-------------|-------------------------------------------|
| Read-only props | 0444      | Server-managed (e.g., `type`, `focused`)  |
| Read/write props | 0644     | User-managed (e.g., `text`, `value`)      |
| Event pipes   | 0444        | Read-only (controller reads events)       |
| Control pipes | 0222        | Write-only (controller writes commands)   |

---

## Access Control Model

### User/Group/Other

9P supports traditional Unix permissions:

```
user (owner)  → First 3 bits
group         → Middle 3 bits
others        → Last 3 bits
```

### Default User Model

**Kryon assumes**:
- **Single-user system** (no multi-user)
- **Owner**: User who started kryonsrv
- **Group**: User's primary group
- **Others**: All other users (same as owner in single-user)

**File creation**:
- Owner: Current user (from Tattach)
- Group: User's primary group
- Mode: Requested permissions & umask

### Permission Checks

For each operation, server checks:

1. **Twalk/Topen**:
   - Read access: Check `r` bit for user
   - Write access: Check `w` bit for user

2. **Twrite**:
   - Check `w` bit for user
   - Return EACCES if not writable

3. **Tread**:
   - Check `r` bit for user
   - Return EACCES if not readable

4. **Tcreate**:
   - Check `w` bit on parent directory
   - Check `x` bit on all path components

5. **Tremove**:
   - Check `w` bit on parent directory
   - Check `x` bit on all path components

### Example Permission Denials

```bash
# Try to read write-only file
cat /ctl
# Error: permission denied

# Try to write read-only file
echo "new value" > /windows/1/focused
# Error: permission denied

# Try to create widget without write access
echo "create_widget 99 button 0" > /ctl
# Error: no such window (window 99 doesn't exist)
```

---

## Multi-Tenant Isolation

### Current Implementation

**Kryon 1.0**: No multi-user isolation
- All clients share same namespace
- No per-user separation
- Trust model: Local, single-user

### Future Multi-Tenancy

**Planned Features**:
1. **Per-User Namespaces**:
   - Each user gets `/windows/~username/` subtree
   - Users can't see other users' windows

2. **Authentication**:
   - Use Tauth for authentication
   - Username/password or client certificates

3. **Authorization**:
   - Per-widget permissions
   - Group-based access control

4. **Sandboxes**:
   - Restricted controllers (read-only access)
   - Audited event delivery

---

## Rate Limiting

### Event Pipe Rate Limiting

**Problem**: Malicious controller could spam events

**Solution**: Rate limit event generation per-widget

**Limits** (configurable):
- **Events per second**: 1000 (default)
- **Burst size**: 100 events
- **Backpressure**: Block writes when queue full

**Implementation**:
```c
// Token bucket algorithm
tokens = max_tokens;
interval = 1000ms / rate;  // Time per token

// On event generation
if (tokens > 0) {
    generate_event();
    tokens--;
} else {
    wait(refill_time);
}
```

### Command Rate Limiting

**Problem**: Malicious controller could spam `/ctl` commands

**Solution**: Rate limit destructive operations

**Limited Commands**:
- `create_window`: 10/sec
- `destroy_window`: 10/sec
- `create_widget`: 100/sec
- `destroy_widget`: 100/sec

**Unlimited Commands**:
- `focus_window`: No limit (user-initiated)
- `refresh`: No limit (idempotent)

### File Write Rate Limiting

**Problem**: Rapid property updates could flood renderer

**Solution**: Coalesce updates

**Strategy**:
- Buffer writes for 16ms (one frame at 60fps)
- Merge multiple writes to same property
- Flush buffer on timeout or explicit sync

**Example**:
```bash
# Rapid writes
echo "1" > /windows/1/widgets/5/visible
echo "0" > /windows/1/widgets/5/visible
echo "1" > /windows/1/widgets/5/visible

# Only last write processed (coalesced)
```

---

## Security Considerations

### Threat Model

**Assumptions**:
- **Trusted Controller**: Runs as same user as server
- **Local Environment**: No network exposure
- **No Privilege Escalation**: All processes same user

**Risks**:
1. **Denial of Service**:
   - Spamming events/commands
   - Creating thousands of windows/widgets
   - **Mitigation**: Rate limiting, resource limits

2. **Information Disclosure**:
   - Reading other users' windows (future multi-tenant)
   - **Mitigation**: Per-user namespaces

3. **State Corruption**:
   - Writing invalid values to properties
   - **Mitigation**: Validation, range checking

### Resource Limits

**Recommended Limits**:
- **Max windows**: 1024
- **Max widgets per window**: 65536
- **Max nesting depth**: 256
- **Max event queue size**: 1000 events
- **Max file size**: 1 MB per property file

### Hardening Checklist

- [ ] Rate limiting implemented
- [ ] Resource limits enforced
- [ ] Property validation on all writes
- [ ] No buffer overflows in string handling
- [ ] No integer overflows in size calculations
- [ ] Proper error handling (no info leakage)
- [ ] Audit logging (optional, for debugging)

---

## Future Security Features

### Authentication

**Tauth Support**:
```
Tauth fid afid uname aname
Rauth aqid
```

**Authentication Methods**:
- Username/password (simple)
- Client certificates (TLS)
- Unix domain sockets (SO_PEERCRED)

### Authorization

**Per-Widget Permissions**:
```
/windows/1/widgets/5/
├── .permissions    # Access control list
│   ├── read: user1, user2
│   ├── write: user1
│   └── execute: user1, user2
```

### Sandboxing

**Restricted Controllers**:
- Read-only access to UI state
- No widget creation/destruction
- Audited event delivery

**Use Cases**:
- Monitoring dashboards
- Debugging tools
- Logging/audit systems

---

## See Also

- [9P Specification](01-9p-specification.md) - Wire protocol
- [Error Handling](04-error-handling.md) - Error codes
- [Filesystem Namespace](02-filesystem-namespace.md) - Directory layout
