-- Kryon Unified DateTime Plugin
-- Provides consistent date/time API across web and native targets
-- Uses Plugin system for automatic target detection

local Plugin = require("kryon.plugin")

local DateTime = {}

-- ============================================================================
-- Web Implementation (using JavaScript Date)
-- ============================================================================

local function createWebImpl()
    local js = require("js")
    local window = js.global
    local JSDate = window.Date

    local impl = {}

    function impl.now()
        local d = js.new(JSDate)
        return {
            year = math.floor(d:getFullYear()),
            month = math.floor(d:getMonth()) + 1,  -- JS months are 0-based
            day = math.floor(d:getDate()),
            hour = math.floor(d:getHours()),
            minute = math.floor(d:getMinutes()),
            second = math.floor(d:getSeconds()),
            dayOfWeek = math.floor(d:getDay()) + 1,  -- JS days are 0-based (Sun=0)
            timestamp = math.floor(d:getTime() / 1000)
        }
    end

    function impl.format(date, pattern)
        -- Basic pattern support for web
        pattern = pattern or "%Y-%m-%d"
        local result = pattern

        -- Date table or timestamp
        local d = date
        if type(date) == "number" then
            local jsDate = js.new(JSDate, date * 1000)
            d = {
                year = math.floor(jsDate:getFullYear()),
                month = math.floor(jsDate:getMonth()) + 1,
                day = math.floor(jsDate:getDate()),
                hour = math.floor(jsDate:getHours()),
                minute = math.floor(jsDate:getMinutes()),
                second = math.floor(jsDate:getSeconds())
            }
        end

        -- Replace common patterns
        result = result:gsub("%%Y", string.format("%04d", d.year or 0))
        result = result:gsub("%%m", string.format("%02d", d.month or 0))
        result = result:gsub("%%d", string.format("%02d", d.day or 0))
        result = result:gsub("%%H", string.format("%02d", d.hour or 0))
        result = result:gsub("%%M", string.format("%02d", d.minute or 0))
        result = result:gsub("%%S", string.format("%02d", d.second or 0))

        -- Month names
        local monthNames = {
            "January", "February", "March", "April", "May", "June",
            "July", "August", "September", "October", "November", "December"
        }
        local shortMonths = {
            "Jan", "Feb", "Mar", "Apr", "May", "Jun",
            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
        }
        result = result:gsub("%%B", monthNames[d.month] or "")
        result = result:gsub("%%b", shortMonths[d.month] or "")

        return result
    end

    function impl.parse(str, pattern)
        -- Basic YYYY-MM-DD parsing for web
        local year, month, day = str:match("^(%d+)%-(%d+)%-(%d+)")
        if year then
            return {
                year = tonumber(year),
                month = tonumber(month),
                day = tonumber(day),
                hour = 0,
                minute = 0,
                second = 0
            }
        end
        return nil
    end

    function impl.diff(date1, date2)
        local t1 = date1.timestamp or 0
        local t2 = date2.timestamp or 0
        return t1 - t2
    end

    function impl.daysInMonth(year, month)
        -- Using JS Date to get days in month
        local d = js.new(JSDate, year, month, 0)  -- Day 0 of next month = last day of this month
        return math.floor(d:getDate())
    end

    function impl.firstDayOfMonth(year, month)
        local d = js.new(JSDate, year, month - 1, 1)
        return math.floor(d:getDay()) + 1  -- Convert to 1-based (Sun=1)
    end

    function impl.weekday(year, month, day)
        -- Returns weekday: 0=Sunday, 1=Monday, ..., 6=Saturday
        -- (matches kryon-plugins/datetime C library convention)
        local d = js.new(JSDate, year, month - 1, day)
        return math.floor(d:getDay())
    end

    function impl.makeDate(year, month, day)
        -- Returns date string in YYYY-MM-DD format
        return string.format("%04d-%02d-%02d", year, month, day)
    end

    function impl.isFuture(year, month, day)
        -- Check if date is in the future
        local now = impl.now()
        if year > now.year then return true end
        if year < now.year then return false end
        if month > now.month then return true end
        if month < now.month then return false end
        return day > now.day
    end

    return impl
end

-- ============================================================================
-- Native Implementation (using standard Lua or FFI datetime)
-- ============================================================================

local function createNativeImpl()
    -- Try to load native datetime module first
    local success, nativeDT = pcall(require, "datetime")
    if success then
        return nativeDT
    end

    -- Fallback to standard Lua os.date/os.time
    local impl = {}

    function impl.now()
        local t = os.date("*t")
        return {
            year = t.year,
            month = t.month,
            day = t.day,
            hour = t.hour,
            minute = t.min,
            second = t.sec,
            dayOfWeek = t.wday,  -- Lua: 1=Sunday
            timestamp = os.time()
        }
    end

    function impl.format(date, pattern)
        pattern = pattern or "%Y-%m-%d"
        if type(date) == "number" then
            return os.date(pattern, date)
        elseif type(date) == "table" then
            local timestamp = os.time({
                year = date.year,
                month = date.month,
                day = date.day,
                hour = date.hour or 0,
                min = date.minute or 0,
                sec = date.second or 0
            })
            return os.date(pattern, timestamp)
        end
        return ""
    end

    function impl.parse(str, pattern)
        -- Basic YYYY-MM-DD parsing
        local year, month, day = str:match("^(%d+)%-(%d+)%-(%d+)")
        if year then
            return {
                year = tonumber(year),
                month = tonumber(month),
                day = tonumber(day),
                hour = 0,
                minute = 0,
                second = 0
            }
        end
        return nil
    end

    function impl.diff(date1, date2)
        local t1 = date1.timestamp or os.time(date1)
        local t2 = date2.timestamp or os.time(date2)
        return t1 - t2
    end

    function impl.daysInMonth(year, month)
        -- Get last day of month
        local nextMonth = month + 1
        local nextYear = year
        if nextMonth > 12 then
            nextMonth = 1
            nextYear = year + 1
        end
        local timestamp = os.time({year = nextYear, month = nextMonth, day = 0})
        return os.date("*t", timestamp).day
    end

    function impl.firstDayOfMonth(year, month)
        local timestamp = os.time({year = year, month = month, day = 1})
        return os.date("*t", timestamp).wday  -- 1=Sunday
    end

    function impl.weekday(year, month, day)
        -- Returns weekday: 0=Sunday, 1=Monday, ..., 6=Saturday
        -- (matches kryon-plugins/datetime C library convention)
        local timestamp = os.time({year = year, month = month, day = day})
        return os.date("*t", timestamp).wday - 1  -- Convert from 1-based to 0-based
    end

    function impl.makeDate(year, month, day)
        -- Returns date string in YYYY-MM-DD format
        return string.format("%04d-%02d-%02d", year, month, day)
    end

    function impl.isFuture(year, month, day)
        -- Check if date is in the future
        local now = impl.now()
        if year > now.year then return true end
        if year < now.year then return false end
        if month > now.month then return true end
        if month < now.month then return false end
        return day > now.day
    end

    return impl
end

-- ============================================================================
-- Initialize and Export
-- ============================================================================

local _impl = nil

local function getImpl()
    if _impl then return _impl end

    local target = Plugin.detectTarget()
    print("[DateTime] Detected target: " .. tostring(target))

    if target == "web" then
        -- Web target: use JavaScript Date API (no fallback to native)
        -- If this fails, let the error propagate - mixing platforms causes timezone issues
        _impl = createWebImpl()
        print("[DateTime] Using web implementation (JavaScript Date API)")
    else
        -- Native target: use FFI datetime library or standard Lua os.date
        _impl = createNativeImpl()
        print("[DateTime] Using native implementation")
    end

    return _impl
end

-- Forward all calls to implementation
setmetatable(DateTime, {
    __index = function(_, k)
        return getImpl()[k]
    end
})

return DateTime
