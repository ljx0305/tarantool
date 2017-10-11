-- log.lua
--
local ffi = require('ffi')
ffi.cdef[[
    typedef void (*sayfunc_t)(int level, const char *filename, int line,
               const char *error, const char *format, ...);
    void
    say_set_log_level(int new_level);

    void
    say_set_log_format(const char *format_name);

    extern sayfunc_t _say;
    extern void say_logrotate(int);

    enum say_level {
        S_FATAL,
        S_SYSERROR,
        S_ERROR,
        S_CRIT,
        S_WARN,
        S_INFO,
        S_VERBOSE,
        S_DEBUG
    };

    enum say_format {
        SF_PLAIN,
        SF_JSON
    };
    pid_t log_pid;
    extern int log_level;
    extern int log_format;
]]

local S_WARN = ffi.C.S_WARN
local S_INFO = ffi.C.S_INFO
local S_VERBOSE = ffi.C.S_VERBOSE
local S_DEBUG = ffi.C.S_DEBUG
local S_ERROR = ffi.C.S_ERROR

local json = require("json")

local function say(level, fmt, ...)
    if ffi.C.log_level < level then
        -- don't waste cycles on debug.getinfo()
        return
    end
    local format = "%s"
    if select('#', ...) ~= 0 then
        local stat
        stat, fmt = pcall(string.format, fmt, ...)
        if not stat then
            error(fmt, 3)
        end
    elseif ffi.C.log_format == ffi.C.SF_JSON and type(fmt) == 'table' then
        fmt = json.encode(fmt)

        -- we need to pass parameter signaling that fmt
        -- is already encoded json
        format = "json"
    elseif type(fmt) ~= 'string' then
        fmt = tostring(fmt)
    end

    local debug = require('debug')
    local frame = debug.getinfo(3, "Sl")
    local line, file = 0, 'eval'
    if type(frame) == 'table' then
        line = frame.currentline or 0
        file = frame.short_src or frame.src or 'eval'
    end

    ffi.C._say(level, file, line, nil, format, fmt)
end

local function say_closure(lvl)
    return function (fmt, ...)
        say(lvl, fmt, ...)
    end
end

local function log_rotate()
    ffi.C.say_logrotate(0)
end

local function log_level(level)
    return ffi.C.say_set_log_level(level)
end

local function log_format(format_name)
    return ffi.C.say_set_log_format(format_name)
end

local function log_pid()
    return tonumber(ffi.C.log_pid)
end

local compat_warning_said = false
local compat_v16 = {
    logger_pid = function()
        if not compat_warning_said then
            compat_warning_said = true
            say(S_WARN, 'logger_pid() is deprecated, please use pid() instead')
        end
        return log_pid()
    end;
}

return setmetatable({
    warn = say_closure(S_WARN);
    info = say_closure(S_INFO);
    verbose = say_closure(S_VERBOSE);
    debug = say_closure(S_DEBUG);
    error = say_closure(S_ERROR);
    rotate = log_rotate;
    pid = log_pid;
    level = log_level;
    log_format = log_format;
}, {
    __index = compat_v16;
})
