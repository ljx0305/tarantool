-- init.lua -- internal file

local ffi = require('ffi')
ffi.cdef[[
struct type_info;
struct method_info;
struct error;
struct error_place;

enum ctype {
    CTYPE_VOID = 0,
    CTYPE_INT,
    CTYPE_CONST_CHAR_PTR
};

struct type_info {
    const char *name;
    const struct type_info *parent;
    const struct method_info *methods;
};

enum {
    DIAG_ERRMSG_MAX = 512,
    DIAG_FILENAME_MAX = 256,
    DIAG_MAX_TRACEBACK = 32
};

struct error_place {
	int _line;
	char _filename[DIAG_FILENAME_MAX];
};

typedef void (*error_f)(struct error *e);

struct error {
    error_f _destroy;
    error_f _raise;
    error_f _log;
    const struct type_info *_type;
    int _refs;
    /** Line number. */
    unsigned _line;
    /* Source file name. */
    char _file[DIAG_FILENAME_MAX];
    /* Error description. */
    char _errmsg[DIAG_ERRMSG_MAX];
    struct error_place _places[DIAG_MAX_TRACEBACK];
    int depth_traceback;
};

enum { METHOD_ARG_MAX = 8 };

struct method_info {
    const struct type_info *owner;
    const char *name;
    enum ctype rtype;
    enum ctype atype[METHOD_ARG_MAX];
    int nargs;
    bool isconst;

    union {
        /* Add extra space to get proper struct size in C */
        void *_spacer[2];
    };
};

char *
exception_get_string(struct error *e, const struct method_info *method);
int
exception_get_int(struct error *e, const struct method_info *method);
int
lua_error_gettraceback(struct lua_State *L, struct error *e);

double
tarantool_uptime(void);
typedef int32_t pid_t;
pid_t getpid(void);
]]

local fio = require("fio")
local internal = require('errors.internal')

local REFLECTION_CACHE = {}

local function reflection_enumerate(err)
    local key = tostring(err._type)
    local result = REFLECTION_CACHE[key]
    if result ~= nil then
        return result
    end
    result = {}
    -- See type_foreach_method() in reflection.h
    local t = err._type
    while t ~= nil do
        local m = t.methods
        while m.name ~= nil do
            result[ffi.string(m.name)] = m
            m = m + 1
        end
        t = t.parent
    end
    REFLECTION_CACHE[key] = result
    return result
end

local function reflection_get(err, method)
    if method.nargs ~= 0 then
        return nil -- NYI
    end
    if method.rtype == ffi.C.CTYPE_INT then
        return tonumber(ffi.C.exception_get_int(err, method))
    elseif method.rtype == ffi.C.CTYPE_CONST_CHAR_PTR then
        local str = ffi.C.exception_get_string(err, method)
        if str == nil then
            return nil
        end
        return ffi.string(str)
    end
end

local function error_type(err)
    return ffi.string(err._type.name)
end

local function error_trace(err)
    if err.depth_traceback == 0 then
        return {}
    end
    return internal.traceback(err)
end

local function error_message(err)
    return ffi.string(err._errmsg)
end


local error_fields = {
    ["type"]        = error_type;
    ["message"]     = error_message;
    ["trace"]       = error_trace;
}

local function error_unpack(err)
    if not ffi.istype('struct error', err) then
        error("Usage: error:unpack()")
    end
    local result = {}
    for key, getter in pairs(error_fields)  do
        result[key] = getter(err)
    end
    for key, getter in pairs(reflection_enumerate(err)) do
        local value = reflection_get(err, getter)
        if value ~= nil then
            result[key] = value
        end
    end
    return result
end

local function error_raise(err)
    if not ffi.istype('struct error', err) then
        error("Usage: error:raise()")
    end
    error(err)
end

local function error_match(err, ...)
    if not ffi.istype('struct error', err) then
        error("Usage: error:match()")
    end
    return string.match(error_message(err), ...)
end

local function error_serialize(err)
    -- Return an error message only in admin console to keep compatibility
    return error_message(err)
end

local error_methods = {
    ["unpack"] = error_unpack;
    ["raise"] = error_raise;
    ["match"] = error_match; -- Tarantool 1.6 backward compatibility
    ["__serialize"] = error_serialize;
}

local function error_index(err, key)
    local getter = error_fields[key]
    if getter ~= nil then
        return getter(err)
    end
    getter = reflection_enumerate(err)[key]
    if getter ~= nil and getter.nargs == 0 then
        local val = reflection_get(err, getter)
        if val ~= nil then
            return val
        end
    end
    return error_methods[key]
end

local error_mt = {
    __index = error_index;
    __tostring = error_message;
};

ffi.metatype('struct error', error_mt);

dostring = function(s, ...)
    local chunk, message = loadstring(s)
    if chunk == nil then
        error(message, 2)
    end
    return chunk(...)
end

local function uptime()
    return tonumber(ffi.C.tarantool_uptime());
end

local function pid()
    return tonumber(ffi.C.getpid())
end

local function mksymname(name)
    local mark = string.find(name, "-")
    if mark then name = string.sub(name, mark + 1) end
    return "luaopen_" .. string.gsub(name, "%.", "_")
end

local function try_load(file, name)
    local loaded, err = loadfile(file)
    if loaded == nil then
        name = mksymname(name)
        loaded, err = package.loadlib(file, name)
        if err ~= nil then
            return nil, err
        end
    end
    return loaded
end

local function cwd_loader(name)
    if not name then
        return "empty name of module"
    end
    local file, err = search_cwd(name)
    if not file then
        return err
    end
    local loaded, err = try_load(file, name)
    if err == nil then
        return loaded
    else
        return err
    end
end

local function rocks_loader(name)
    if not name then
        return "empty name of module"
    end
    local file, err = search_rocks(name)
    if not file then
        return err
    end
    local loaded, err = try_load(file, name)
    if err == nil then
        return loaded
    else
        return err
    end
end

function search_cwd(name)
    local path = "./?.lua;./?/init.lua"
    if jit.os == "OSX" then
        path = path .. ";./?.dylib"
    end
    path = path .. ";./?.so"
    local file, err = package.searchpath(name, path)
    if file == nil then
        return nil, err
    end
    return file
end

function search_rocks(name)
    local pathes_search = {
        "/.rocks/share/tarantool/?.lua;",
        "/.rocks/share/tarantool/?/init.lua;",
    }
    if jit.os == "OSX" then
        table.insert(pathes_search, "/.rocks/lib/tarantool/?.dylib")
    end
    table.insert(pathes_search, "/.rocks/lib/tarantool/?.so")

    local cwd = fio.cwd()
    local index = string.len(cwd) + 1
    local strerr = ""
    while index ~= nil do
        cwd = string.sub(cwd, 1, index - 1)
        for i, path in ipairs(pathes_search) do
            local file, err = package.searchpath(name, cwd .. path)
            if err == nil then
                return file
            end
            strerr = strerr .. err
        end
        index = string.find(cwd, "/[^/]*$")
    end
    return nil, strerr
end

local function search(name)
    if not name then
        return "empty name of module"
    end
    local file = search_cwd(name)
    if file ~= nil then
        return file
    end
    file = search_rocks(name)
    if file ~= nil then
        return file
    end
    file = package.searchpath(name, package.path)
    if file ~= nil then
        return file
    end
    file = package.searchpath(name, package.cpath)
    if file ~= nil then
        return file
    end
    return nil
end

table.insert(package.loaders, 2, cwd_loader)
table.insert(package.loaders, 3, rocks_loader)
package.search = search
return {
    uptime = uptime;
    pid = pid;
}
