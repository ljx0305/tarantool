#!/usr/bin/env tarantool

local test = require('tap').test('log')
test:plan(7)

--
-- Check that Tarantool creates ADMIN session for #! script
--
local filename = "1.log"
local message = "Hello, World!"
box.cfg{
    log=filename,
    memtx_memory=107374182,
}
local log = require('log')
local io = require('io')
local json = require('json')
local file = io.open(filename)
while file:read() do
end
log.info(message)
local line = file:read()
test:is(line:sub(-message:len()), message, "message")
s, err = pcall(json.decode, line)
test:ok(not s, "plain")
--
-- gh-700: Crash on calling log.info() with formatting characters
--
log.info("gh-700: %%s %%f %%d")
test:is(file:read():match('I>%s+(.*)'), "gh-700: %%s %%f %%d", "formatting")

log.info("gh-2340: %s %D")
test:is(file:read():match('I>%s+(.*)'), "gh-2340: %s %D", "formatting without arguments")


function help() log.info("gh-2340: %s %s", 'help') end

xpcall(help, function(err)
    test:ok(err:match("bad argument #3"), "found error string")
    test:ok(err:match("logger.test.lua:36:"), "found error place")
end)

file:close()
log.rotate()

test:ok(log.pid() >= 0, "pid()")

-- logger uses 'debug', try to set it to nil
debug = nil
log.info("debug is nil")
debug = require('debug')

test:ok(log.info(true) == nil, 'check tarantool crash (gh-2516)')

box.cfg{log=filename,
    memtx_memory=107374182,
    log_format = "json"}

local file = io.open(filename)
while file:read() do
end

log.error("error")

local line = file:read()
message = json.decode(line)
test:is(type(message), 'table', "json valid in log.error")
test:is(message.level, "ERROR", "check type error")
test:is(message.message, "error", "check error message")

log.info({key="value"})
local line = file:read()
message = json.decode(line)
test:is(type(message), 'table', "json valid in log.info")
test:is(message.level, "INFO", "check type info")
test:is(message.message, nil, "check message is nil")
test:is(message.key, "value", "custom table encoded")

log.info("json")
local line = file:read()
message = json.decode(line)
test:is(message.message, "json", "check message with internal key word")
file:close()

test:check()
os.exit()
