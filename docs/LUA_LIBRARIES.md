# Using Standard Lua Libraries with Kryon

Kryon now supports using standard Lua libraries and external Lua modules through the `require()` function. This allows you to leverage the existing Lua ecosystem without needing custom Kryon-specific extensions.

## What Works Out of the Box

### Standard Lua Libraries

All standard Lua libraries are automatically available:

- `require('os')` - Operating system interface
- `require('io')` - Input/output facilities  
- `require('math')` - Mathematical functions
- `require('string')` - String manipulation
- `require('table')` - Table manipulation
- `require('debug')` - Debug interface
- `require('package')` - Module system
- `require('coroutine')` - Coroutine support

### Custom Lua Modules

You can create and use custom Lua modules by placing `.lua` files in your project directory:

```lua
-- mymodule.lua
local M = {}

function M.hello()
    return "Hello from custom module!"
end

function M.calculate(a, b)
    local math = require('math')
    return math.sqrt(a^2 + b^2)
end

return M
```

Then use it in your Kryon scripts:

```lua
local mymodule = require('mymodule')
print(mymodule.hello())
print("Distance:", mymodule.calculate(3, 4))
```

## Example: Using Standard Libraries

Here's a simple example showing standard library usage:

```kryon
Container {
    Button {
        text: "Test Libraries"
        
        function onClick() {
            -- Use OS library
            local os = require('os')
            print("Current time:", os.time())
            print("Date:", os.date("%Y-%m-%d"))
            
            -- Use math library
            local math = require('math')
            print("Pi:", math.pi)
            print("Random:", math.random(1, 100))
            
            -- Use string library
            local string = require('string')
            print("Uppercase:", string.upper("hello world"))
            
            -- Use table library
            local table = require('table')
            local fruits = {"apple", "banana", "cherry"}
            print("Joined:", table.concat(fruits, ", "))
        }
    }
}
```

## Installing External Libraries

### Using Luarocks (if available)

If you have Luarocks installed, you can install external libraries:

```bash
# Install SQLite support
luarocks install lsqlite3

# Install JSON support  
luarocks install dkjson

# Install HTTP client
luarocks install http

# Install filesystem utilities
luarocks install luafilesystem
```

### Using System Package Manager

Many Linux distributions provide Lua libraries through their package managers:

```bash
# Ubuntu/Debian
sudo apt-get install lua-sql-sqlite3 lua-cjson lua-filesystem

# Fedora/CentOS
sudo dnf install lua-sql-sqlite3 lua-cjson lua-filesystem

# Arch Linux  
sudo pacman -S lua-sql-sqlite3 lua-cjson lua-filesystem
```

## Example: Database Usage with lsqlite3

Once you have lsqlite3 installed, you can use it like this:

```kryon
Container {
    Button {
        text: "Test Database"
        
        function onClick() {
            -- Load SQLite library
            local sqlite3 = require('lsqlite3')
            
            -- Create/open database
            local db = sqlite3.open('myapp.db')
            
            -- Create table
            db:exec([[
                CREATE TABLE IF NOT EXISTS users (
                    id INTEGER PRIMARY KEY,
                    name TEXT NOT NULL,
                    email TEXT UNIQUE
                )
            ]])
            
            -- Insert data
            db:exec("INSERT INTO users (name, email) VALUES ('John Doe', 'john@example.com')")
            
            -- Query data
            for row in db:nrows("SELECT * FROM users") do
                print("User:", row.name, row.email)
            end
            
            -- Close database
            db:close()
            
            print("Database operations completed!")
        }
    }
}
```

## Lua Module Search Paths

Kryon automatically configures Lua to search for modules in these locations:

**Lua Scripts (package.path):**
- `./?.lua` - Current directory
- `./?/init.lua` - Module directories  
- `/usr/local/share/lua/5.4/?.lua` - System-wide modules
- `/usr/share/lua/5.4/?.lua` - Distribution modules
- `~/.luarocks/share/lua/5.4/?.lua` - User-installed modules

**C Libraries (package.cpath):**
- `./?.so` - Current directory
- `/usr/local/lib/lua/5.4/?.so` - System-wide libraries
- `/usr/lib/lua/5.4/?.so` - Distribution libraries  
- `~/.luarocks/lib/lua/5.4/?.so` - User-installed libraries

## Best Practices

1. **Check module availability** before using:
   ```lua
   local status, sqlite3 = pcall(require, 'lsqlite3')
   if not status then
       print("SQLite not available:", sqlite3)
       return
   end
   ```

2. **Handle errors gracefully**:
   ```lua
   local function safe_require(module)
       local status, result = pcall(require, module)
       if status then
           return result
       else
           print("Failed to load module:", module)
           return nil
       end
   end
   
   local json = safe_require('dkjson')
   if json then
       -- Use json module
   end
   ```

3. **Keep dependencies minimal** - Only use what you need

4. **Document dependencies** - List required modules in your project README

## Common Libraries

Here are some popular Lua libraries that work well with Kryon:

- **lsqlite3** - SQLite database interface
- **dkjson** or **cjson** - JSON parsing and generation  
- **luasocket** - Network programming
- **luafilesystem** (lfs) - File system operations
- **luaposix** - POSIX system calls
- **penlight** - General utilities library
- **lustache** - Mustache templating
- **lpeg** - Pattern matching

## Summary

Kryon's approach to external libraries is simple and familiar:

- ✅ Use standard `require()` function
- ✅ Standard Lua libraries work immediately  
- ✅ Custom modules supported
- ✅ Compatible with existing Lua ecosystem
- ✅ No need for special Kryon-specific extensions
- ✅ Standard installation methods (luarocks, package managers)

This approach leverages the mature Lua ecosystem while keeping Kryon simple and focused on UI development.