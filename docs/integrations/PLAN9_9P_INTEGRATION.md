# Plan 9 / 9P Integration Guide for Kryon

## Overview

While direct TaijiOS kernel syscall integration isn't possible (see `TAIJIOS_INTEGRATION_STATUS.md`), we can achieve Inferno/Plan 9 functionality through **9P client libraries**. This approach allows Kryon to access 9P filesystems over network or local sockets without requiring the emu runtime.

## Recommended Approach: Plan9Port lib9pclient

### Why lib9pclient?

1. **Complete Implementation** - Full 9P2000 client with POSIX-like API
2. **Well Documented** - Comprehensive man pages and examples
3. **Actively Maintained** - Part of Plan 9 from User Space project
4. **Proven** - Used in production tools like 9pfuse
5. **Standalone Ready** - Can extract only needed components

### What You Get

```c
// Connect to 9P server
CFid *fsmount(int fd, char *aname);

// File operations (similar to POSIX)
CFid *fsopen(CFsys *fs, char *path, int mode);
long fsread(CFid *fid, void *buf, long n);
long fswrite(CFid *fid, void *buf, long n);
void fsclose(CFid *fid);

// Directory operations
long fsdirread(CFid *fid, Dir **d);
Dir *fsdirstat(CFsys *fs, char *path);

// And more...
```

## Integration Architecture

```
┌─────────────────────────────────────────────┐
│           Kryon Application                 │
│  (Running standalone on Linux/Mac/Windows)  │
└─────────────────┬───────────────────────────┘
                  │
         ┌────────┴────────┐
         │  Kryon Services │
         │     Plugin      │
         └────────┬────────┘
                  │
         ┌────────┴────────┐
         │   lib9pclient   │ ← Plan9Port library
         │  (9P Client)    │
         └────────┬────────┘
                  │
                  │ TCP/Unix Socket
                  │ (9P2000 Protocol)
                  │
         ┌────────┴────────┐
         │   9P Server     │
         │  (Any impl.)    │
         └─────────────────┘
              │
    ┌─────────┴─────────┐
    │                   │
┌───┴────┐      ┌───────┴──────┐
│TaijiOS │      │ Plan 9       │
│  emu   │      │ 9front/etc.  │
└────────┘      └──────────────┘
```

## Implementation Steps

### Step 1: Install Plan9Port

```bash
# Clone Plan9Port
git clone https://github.com/9fans/plan9port $HOME/plan9

# Build it
cd $HOME/plan9
./INSTALL

# Add to PATH
export PLAN9=$HOME/plan9
export PATH=$PATH:$PLAN9/bin
```

### Step 2: Create 9P Client Service

Create `src/services/ninep_client.h`:

```c
/**
 * @file ninep_client.h
 * @brief 9P Network Filesystem Client Service
 *
 * Provides access to 9P servers over network/local sockets.
 * Uses Plan9Port's lib9pclient for protocol implementation.
 */

#ifndef KRYON_SERVICE_NINEP_CLIENT_H
#define KRYON_SERVICE_NINEP_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 9P connection handle
 */
typedef struct Kryon9PConnection Kryon9PConnection;

/**
 * @brief 9P file handle
 */
typedef struct Kryon9PFile Kryon9PFile;

/**
 * @brief 9P Client Service
 */
typedef struct Kryon9PClientService {
    /**
     * @brief Connect to 9P server
     * @param address Server address (e.g., "tcp!server!564" or "unix!/tmp/ns.user.:0/factotum")
     * @param aname Attach name (usually "" or username)
     * @return Connection handle or NULL on failure
     */
    Kryon9PConnection* (*connect)(const char *address, const char *aname);

    /**
     * @brief Disconnect from server
     */
    void (*disconnect)(Kryon9PConnection *conn);

    /**
     * @brief Open file
     * @param conn Connection handle
     * @param path Path to file
     * @param mode Open mode (OREAD, OWRITE, ORDWR)
     * @return File handle or NULL on failure
     */
    Kryon9PFile* (*open)(Kryon9PConnection *conn, const char *path, int mode);

    /**
     * @brief Close file
     */
    void (*close)(Kryon9PFile *file);

    /**
     * @brief Read from file
     */
    long (*read)(Kryon9PFile *file, void *buf, long n);

    /**
     * @brief Write to file
     */
    long (*write)(Kryon9PFile *file, const void *buf, long n);

    /**
     * @brief Seek in file
     */
    int64_t (*seek)(Kryon9PFile *file, int64_t offset, int whence);

    /**
     * @brief Read directory entries
     * @param conn Connection handle
     * @param path Directory path
     * @param entries Output array of entry names
     * @param count Output number of entries
     * @return true on success
     */
    bool (*readdir)(Kryon9PConnection *conn, const char *path,
                    char ***entries, int *count);

    /**
     * @brief Get file stats
     */
    bool (*stat)(Kryon9PConnection *conn, const char *path,
                 struct Kryon9PFileStat *stat);

} Kryon9PClientService;

/**
 * @brief File stat information
 */
typedef struct Kryon9PFileStat {
    uint64_t size;
    uint32_t mode;
    uint32_t mtime;
    uint32_t atime;
    char *name;
    char *uid;
    char *gid;
} Kryon9PFileStat;

#ifdef __cplusplus
}
#endif

#endif // KRYON_SERVICE_NINEP_CLIENT_H
```

### Step 3: Implement Using lib9pclient

Create `src/plugins/plan9port/ninep_client.c`:

```c
#include "services/ninep_client.h"
#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <9pclient.h>

struct Kryon9PConnection {
    CFsys *fs;
    int fd;
};

struct Kryon9PFile {
    CFid *fid;
};

static Kryon9PConnection* ninep_connect(const char *address, const char *aname) {
    // Parse address and connect
    // This is simplified - actual implementation would handle
    // "tcp!host!port" and "unix!/path" formats

    int fd = dial(address, NULL, NULL, NULL);
    if (fd < 0) {
        return NULL;
    }

    CFsys *fs = fsmount(fd, aname ? aname : "");
    if (fs == NULL) {
        close(fd);
        return NULL;
    }

    Kryon9PConnection *conn = malloc(sizeof(Kryon9PConnection));
    conn->fs = fs;
    conn->fd = fd;
    return conn;
}

static void ninep_disconnect(Kryon9PConnection *conn) {
    if (!conn) return;

    fsunmount(conn->fs);
    close(conn->fd);
    free(conn);
}

static Kryon9PFile* ninep_open(Kryon9PConnection *conn, const char *path, int mode) {
    if (!conn || !path) return NULL;

    CFid *fid = fsopen(conn->fs, (char*)path, mode);
    if (!fid) return NULL;

    Kryon9PFile *file = malloc(sizeof(Kryon9PFile));
    file->fid = fid;
    return file;
}

static void ninep_close(Kryon9PFile *file) {
    if (!file) return;
    fsclose(file->fid);
    free(file);
}

static long ninep_read(Kryon9PFile *file, void *buf, long n) {
    if (!file) return -1;
    return fsread(file->fid, buf, n);
}

static long ninep_write(Kryon9PFile *file, const void *buf, long n) {
    if (!file) return -1;
    return fswrite(file->fid, (void*)buf, n);
}

static int64_t ninep_seek(Kryon9PFile *file, int64_t offset, int whence) {
    if (!file) return -1;
    return fsseek(file->fid, offset, whence);
}

static bool ninep_readdir(Kryon9PConnection *conn, const char *path,
                          char ***entries, int *count) {
    if (!conn || !path || !entries || !count) return false;

    CFid *dirfid = fsopen(conn->fs, (char*)path, OREAD);
    if (!dirfid) return false;

    Dir *dirs;
    long n = fsdirread(dirfid, &dirs);
    fsclose(dirfid);

    if (n < 0) return false;

    char **names = malloc(n * sizeof(char*));
    for (long i = 0; i < n; i++) {
        names[i] = strdup(dirs[i].name);
    }
    free(dirs);

    *entries = names;
    *count = n;
    return true;
}

// Service interface
static Kryon9PClientService ninep_client_service = {
    .connect = ninep_connect,
    .disconnect = ninep_disconnect,
    .open = ninep_open,
    .close = ninep_close,
    .read = ninep_read,
    .write = ninep_write,
    .seek = ninep_seek,
    .readdir = ninep_readdir,
    // ... stat and other functions
};

Kryon9PClientService* kryon_get_ninep_client_service(void) {
    return &ninep_client_service;
}
```

### Step 4: Update Build System

Modify `Makefile` to add Plan9Port plugin:

```makefile
# Plan9Port 9P Client Plugin
ifdef PLAN9
    PLUGINS += plan9port
    PLUGIN_SRC += src/plugins/plan9port/ninep_client.c
    PLUGIN_CFLAGS += -DKRYON_PLUGIN_PLAN9PORT
    PLUGIN_INCLUDES += -I$(PLAN9)/include
    PLUGIN_LDFLAGS += -L$(PLAN9)/lib -l9pclient -lthread -l9
    $(info Building with Plan9Port 9P client plugin)
endif
```

Build with:
```bash
PLAN9=$HOME/plan9 make
```

## Usage Examples

### Example 1: Connect to TaijiOS emu

```c
#include "services/ninep_client.h"

// Get the service
Kryon9PClientService *svc = kryon_get_service(KRYON_SERVICE_NINEP_CLIENT);
if (!svc) {
    printf("9P client not available\n");
    return;
}

// Connect to TaijiOS emu (assuming it's exporting on port 564)
Kryon9PConnection *conn = svc->connect("tcp!localhost!564", "");
if (!conn) {
    printf("Failed to connect\n");
    return;
}

// Read from console device
Kryon9PFile *cons = svc->open(conn, "#c/cons", OREAD);
if (cons) {
    char buf[256];
    long n = svc->read(cons, buf, sizeof(buf)-1);
    if (n > 0) {
        buf[n] = '\0';
        printf("Read from console: %s\n", buf);
    }
    svc->close(cons);
}

// List processes
char **entries;
int count;
if (svc->readdir(conn, "/prog", &entries, &count)) {
    printf("Processes:\n");
    for (int i = 0; i < count; i++) {
        printf("  %s\n", entries[i]);
        free(entries[i]);
    }
    free(entries);
}

svc->disconnect(conn);
```

### Example 2: Access Remote Plan 9 Server

```c
// Connect to Plan 9 cpu server
Kryon9PConnection *conn = svc->connect("tcp!plan9.server!564", "username");

// Read a file
Kryon9PFile *file = svc->open(conn, "/usr/username/data.txt", OREAD);
if (file) {
    char data[1024];
    long n = svc->read(file, data, sizeof(data));
    // Process data...
    svc->close(file);
}

svc->disconnect(conn);
```

### Example 3: Use in Kryon Application

```kryon
# Kryon can detect and use the 9P service
app MyApp {
    button "Connect to Inferno" {
        click: connectToInferno()
    }

    text id: "status" {}
}

fn connectToInferno() {
    # C bridge would call the 9P client service
    let conn = ninep_connect("tcp!localhost!564", "")
    if conn {
        set_text("status", "Connected!")

        # List processes
        let procs = ninep_readdir(conn, "/prog")
        for proc in procs {
            println("Process: " + proc)
        }
    }
}
```

## Alternative: Lighter Integration with libixp

If Plan9Port is too heavy, use libixp:

```bash
# Install libixp
git clone https://github.com/0intro/libixp
cd libixp
make
sudo make install
```

Update Makefile:
```makefile
PLUGIN_LDFLAGS += -lixp
```

API is similar but more minimal:
```c
#include <ixp.h>

IxpClient *client = ixp_mount("unix!/tmp/ns.:0/factotum");
// Use IxpClient API...
```

## Deployment Scenarios

### Scenario 1: Kryon ↔ TaijiOS Development

1. Run TaijiOS emu with 9P export:
   ```bash
   cd ~/Projects/TaijiOS
   emu -r /root
   # Inside emu:
   listen tcp!*!564
   ```

2. Run Kryon application that connects:
   ```bash
   PLAN9=$HOME/plan9 ./kryon run myapp.kry
   ```

### Scenario 2: Distributed System

```
┌──────────────┐     9P/TCP      ┌──────────────┐
│ Kryon GUI    │ ←────────────→  │  Plan 9      │
│ (Desktop)    │    Port 564     │  Server      │
└──────────────┘                 └──────────────┘
                                        │
                                  File Storage
                                  Process Control
                                  Services
```

### Scenario 3: Hybrid Architecture

```
┌─────────────────────────────────────┐
│     Kryon Application               │
│  ┌──────────┐      ┌─────────────┐ │
│  │ Local    │      │ 9P Client   │ │
│  │ Storage  │      │ (Network)   │ │
│  └──────────┘      └──────┬──────┘ │
└────────────────────────────┼────────┘
                             │
                      ┌──────┴──────┐
                      │  9P Server  │
                      │ (Inferno)   │
                      └─────────────┘
                            │
                    Shared Resources
                    Device Access
```

## Benefits of This Approach

### ✅ Works Today
- No waiting for TaijiOS changes
- Standard C compilation
- Portable across platforms

### ✅ Real Integration
- Actual 9P protocol communication
- Access to real Inferno/Plan 9 resources
- Not just stubs

### ✅ Flexible
- Works over network or local sockets
- Can connect to any 9P server
- Inferno, Plan 9, 9front, or custom implementations

### ✅ Maintainable
- Well-documented libraries
- Active communities
- Clear upgrade paths

## Performance Considerations

### Network Latency
- 9P over TCP adds network overhead
- Use Unix sockets for local communication
- Consider caching frequently accessed files

### Protocol Efficiency
- 9P2000 is designed for low latency
- Efficient binary protocol
- Streaming support for large files

### Optimization Tips
```c
// 1. Reuse connections
static Kryon9PConnection *cached_conn = NULL;

// 2. Buffer reads/writes
char buffer[8192];
while ((n = svc->read(file, buffer, sizeof(buffer))) > 0) {
    process(buffer, n);
}

// 3. Use async I/O where supported
// (Some 9P client libs support non-blocking operations)
```

## Security Considerations

### Authentication
```c
// Plan9Port supports authentication
Kryon9PConnection *conn = svc->connect("tcp!server!564", "username");
// Factotum or other auth method will be used
```

### Encryption
- Use SSH tunneling for encrypted connections:
  ```bash
  ssh -L 9564:localhost:564 server
  # Then connect to localhost:9564
  ```

### Permissions
- 9P servers enforce file permissions
- Kryon respects server-side access control
- No privilege escalation possible

## Testing

### Test 1: Basic Connection
```bash
# Start test 9P server (using Plan9Port)
9pserve tcp!*!5640 &

# Test with 9p command
9p -a 'tcp!localhost!5640' ls /

# Test with Kryon
./kryon run test_9p.kry
```

### Test 2: TaijiOS Integration
```bash
# Terminal 1: Start TaijiOS
cd ~/Projects/TaijiOS
emu

# Terminal 2: Connect from Kryon
./kryon run inferno_test.kry
```

## Troubleshooting

### Connection Refused
```bash
# Check 9P server is listening
netstat -an | grep 564

# Try with 9p utility first
9p -a 'tcp!server!564' ls /
```

### Protocol Mismatch
```c
// Specify 9P version
fsversion(fs, "9P2000");  // or "9P2000.u"
```

### Permission Denied
```c
// Check authentication
// Use factotum or provide credentials
```

## Next Steps

1. **Install Plan9Port**
   ```bash
   git clone https://github.com/9fans/plan9port $HOME/plan9
   cd $HOME/plan9 && ./INSTALL
   ```

2. **Create Plugin Structure**
   ```bash
   mkdir -p src/plugins/plan9port
   touch src/plugins/plan9port/{plugin.c,ninep_client.c}
   ```

3. **Implement Service**
   - Follow the examples above
   - Start with basic connect/open/read/close
   - Add features incrementally

4. **Test with TaijiOS**
   - Export 9P from emu
   - Connect from Kryon
   - Verify file operations work

5. **Document & Deploy**
   - Update README.md
   - Create user examples
   - Add to CI/CD if applicable

## Resources

- [Plan9Port Documentation](https://9fans.github.io/plan9port/)
- [9pclient(3) Man Page](https://9fans.github.io/plan9port/man/man3/9pclient.html)
- [Intro to 9P](http://9p.cat-v.org/documentation/)
- [9P Implementations List](http://9p.cat-v.org/implementations)
- [libixp Repository](https://github.com/0intro/libixp)
- [9pfuse Example](https://github.com/aperezdc/9pfuse)

## Conclusion

This approach provides **real** Plan 9 / Inferno integration without requiring kernel-level access. It's practical, portable, and can be implemented today.

The 9P protocol is exactly what Inferno/Plan 9 systems use internally, so connecting via 9P gives you the same capabilities as if you were running on the native system - just over a network protocol instead of kernel syscalls.

**Status:** Ready for implementation
**Estimated Effort:** 2-3 days for basic functionality
**Benefits:** Real integration, portable, maintainable
