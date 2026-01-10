-- Simple desktop storage fallback for standalone binaries
-- Writes to ~/.local/share/{app_name}/storage.json

local Storage = {}
Storage.app_name = "kryon_app"
Storage.data = {}
Storage.storage_file = nil
Storage.loaded = false

local function get_storage_dir()
    local home = os.getenv("HOME") or os.getenv("USERPROFILE")
    if not home then return nil end

    local xdg_data = os.getenv("XDG_DATA_HOME")
    if xdg_data and xdg_data ~= "" then
        return xdg_data .. "/" .. Storage.app_name
    end

    return home .. "/.local/share/" .. Storage.app_name
end

local function get_storage_file()
    local dir = get_storage_dir()
    if not dir then return nil end

    -- Create directory if needed
    os.execute("mkdir -p '" .. dir .. "' 2>/dev/null")

    return dir .. "/storage.json"
end

local function load_from_disk()
    if Storage.loaded then return end

    Storage.storage_file = get_storage_file()
    if not Storage.storage_file then
        Storage.loaded = true
        return
    end

    local file = io.open(Storage.storage_file, "r")
    if file then
        local content = file:read("*a")
        file:close()

        if content and content ~= "" then
            local ok, data = pcall(function()
                return load("return " .. content)()
            end)
            if ok and type(data) == "table" then
                Storage.data = data
            end
        end
    end

    Storage.loaded = true
end

local function save_to_disk()
    if not Storage.storage_file then
        Storage.storage_file = get_storage_file()
        if not Storage.storage_file then return false end
    end

    -- Convert table to JSON-like format
    local content = "{"
    local first = true
    for k, v in pairs(Storage.data) do
        if type(v) == "string" then
            if not first then content = content .. ", " end
            content = content .. string.format("[%q] = %q", k, v)
            first = false
        end
    end
    content = content .. "}"

    local file = io.open(Storage.storage_file, "w")
    if file then
        file:write(content)
        file:close()
        return true
    end

    return false
end

function Storage.init(name)
    if name then
        Storage.app_name = name
        -- Reset storage file to force recalculation with new app_name
        Storage.storage_file = nil
        Storage.loaded = false
    end
    load_from_disk()
    return true
end

function Storage.setItem(key, value)
    if type(value) ~= "string" then
        value = tostring(value)
    end
    Storage.data[key] = value
    save_to_disk()
end

function Storage.getItem(key)
    load_from_disk()
    return Storage.data[key] or nil
end

function Storage.removeItem(key)
    Storage.data[key] = nil
    save_to_disk()
end

function Storage.clear()
    Storage.data = {}
    save_to_disk()
end

function Storage.save()
    return save_to_disk()
end

-- ============================================================================
-- Collections API (for storing complex data structures like arrays)
-- ============================================================================

Storage.Collections = {}

--- Load a collection from storage
--- @param name string Collection name
--- @param default any Default value if not found
--- @return table Collection data
function Storage.Collections.load(name, default)
    load_from_disk()
    local key = "collection_" .. name
    local jsonStr = Storage.data[key]

    if jsonStr then
        -- Parse Lua table representation back to table
        local ok, result = pcall(function()
            return load("return " .. jsonStr)()
        end)
        if ok and type(result) == "table" then
            return result
        end
    end

    return default or {}
end

--- Save a collection to storage
--- @param name string Collection name
--- @param data table Collection data (array of habit objects)
function Storage.Collections.save(name, data)
    if type(data) ~= "table" then
        return false
    end

    local key = "collection_" .. name

    -- Serialize table to Lua string representation
    local parts = {}
    for i, value in ipairs(data) do
        if type(value) == "table" then
            local itemParts = {}
            -- Serialize habit object properties
            for k, v in pairs(value) do
                if type(k) == "string" then
                    if type(v) == "string" then
                        table.insert(itemParts, k .. "=" .. string.format("%q", v))
                    elseif type(v) == "number" then
                        table.insert(itemParts, k .. "=" .. tostring(v))
                    elseif type(v) == "boolean" then
                        table.insert(itemParts, k .. "=" .. tostring(v))
                    elseif type(v) == "table" then
                        -- Serialize nested table (e.g., tracked dates)
                        local nested = "{"
                        local first = true
                        for nk, nv in pairs(v) do
                            if not first then nested = nested .. ", " end
                            if type(nk) == "string" then
                                nested = nested .. "[" .. string.format("%q", nk) .. "]"
                            end
                            if type(nv) == "string" then
                                nested = nested .. "=" .. string.format("%q", nv)
                            elseif type(nv) == "number" then
                                nested = nested .. "=" .. tostring(nv)
                            elseif type(nv) == "boolean" then
                                nested = nested .. "=" .. tostring(nv)
                            end
                            first = false
                        end
                        nested = nested .. "}"
                        table.insert(itemParts, k .. "=" .. nested)
                    end
                end
            end
            table.insert(parts, "{" .. table.concat(itemParts, ", ") .. "}")
        end
    end

    local content = "{" .. table.concat(parts, ", ") .. "}"
    Storage.data[key] = content
    return save_to_disk()
end

-- Initialize with default app name
Storage.init()

return Storage
