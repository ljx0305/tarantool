#!/usr/bin/env tarantool
test = require("sqltester")
test:plan(148)

--!./tcltestrunner.lua
-- 2009 January 29
--
-- The author disclaims copyright to this source code.  In place of
-- a legal notice, here is a blessing:
--
--    May you do good and not evil.
--    May you find forgiveness for yourself and forgive others.
--    May you share freely, never taking more than you give.
--
-------------------------------------------------------------------------
--
-- Verify that certain keywords can be used as identifiers.
--
-- $Id: keyword1.test,v 1.1 2009/01/29 19:27:47 drh Exp $
-- ["set","testdir",[["file","dirname",["argv0"]]]]
-- ["source",[["testdir"],"\/tester.tcl"]]
test:execsql [[
    CREATE TABLE t1(a PRIMARY KEY, b);
    INSERT INTO t1 VALUES(1, 'one');
    INSERT INTO t1 VALUES(2, 'two');
    INSERT INTO t1 VALUES(3, 'three');
]]
local kwlist = {
    "abort",
    "after",
    "analyze",
    "attach",
    "before",
    "conflict",
    "deferred",
    "exclusive",
    "fail",
    "if",
    "ignore",
    "initially",
    "instead",
    "key",
    "offset",
    "plan",
    "pragma",
    "query",
    "raise",
    "reindex",
    "replace",
    "restrict",
    "temp",
    "temporary",
    "virtual",
}

local bannedkws = {
	"all",
	"alter",
	"and",
	"as",
	"asc",
	"between",
	"by",
	"case",
	"check",
	"collate",
	"column",
	"commit",
	"constraint",
	"create",
	"current_date",
	"current_time",
	"current_timestamp",
	"default",
	"delete",
	"desc",
	"distinct",
	"drop",
	"each",
	"else",
	"escape",
	"exists",
	"explain",
	"for",
	"foreign",
	"from",
	"group",
	"having",
	"immediate",
	"in",
	"index",
	"insert",
	"intersect",
	"into",
	"int",
	"integer",
	"is",
	"join",
	"like",
	"match",
	"not",
	"null",
	"of",
	"on",
	"or",
	"order",
	"primary",
	"recursive",
	"references",
	"release",
	"rename",
	"rollback",
	"row",
	"savepoint",
	"set",
	"table",
	"then",
	"to",
	"transaction",
	"trigger",
	"union",
	"unique",
	"update",
	"using",
	"values",
	"view",
	"with",
	"when",
	"where",
	"any",
	"asensitive",
	"both",
	"call",
	"condition",
	"connect",
	"convert",
	"current",
	"current_user",
	"cursor",
	"dec",
	"decimal",
	"declare",
	"describe",
	"deterministic",
	"fetch",
	"function",
	"grant",
	"inout",
	"insensitive",
	"interval",
	"leading",
	"localtime",
	"localtimestamp",
	"mod",
	"modifies",
	"out",
	"partition",
	"precision",
	"procedure",
	"range",
	"reads",
	"return",
	"returns",
	"revoke",
	"rows",
	"sensitive",
	"smallint",
	"some",
	"specific",
	"sqlexception",
	"sqlstate",
	"sqlwarning",
	"start",
	"trailing",
	"user",
	"whenever",
	"bigint",
	"binary",
	"blob",
	"char",
	"character",
	"date",
	"double",
	"float",
	"numeric",
	"real",
	"varchar",
	"varying",
}

local exprkw = [[
    "cast",
    "current_date",
    "current_time",
    "current_timestamp",
    "raise",
]]
for _, kw in ipairs(kwlist) do
    test:do_test(
        "keyword1-"..kw..".1",
        function()
            if (kw == "if") then
                test:execsql( string.format([[CREATE TABLE "%s"(%s %s PRIMARY KEY)]], kw, kw, kw))
            else
                test:execsql(string.format("CREATE TABLE %s(%s %s PRIMARY KEY)", kw, kw, kw))
            end
            test:execsql("INSERT INTO "..kw.." VALUES(99)")
            test:execsql("INSERT INTO "..kw.." SELECT a FROM t1")
            if test:lsearch(exprkw, kw) <0 then
                return test:execsql(string.format("SELECT * FROM %s ORDER BY %s ASC", kw, kw))
            else
                return test:execsql(string.format([[SELECT * FROM %s ORDER BY "%s" ASC]], kw, kw))
            end
        end, {
            1, 2, 3, 99
        })

    test:do_test(
        "keyword1-"..kw..".2",
        function()
            if (kw == "if") then
                test:execsql(string.format([[DROP TABLE "%s"]], kw))
                test:execsql(string.format([[CREATE INDEX "%s" ON t1(a)]], kw))
            else
                test:execsql("DROP TABLE "..kw.."")
                test:execsql("CREATE INDEX "..kw.." ON t1(a)")
            end
            return test:execsql("SELECT b FROM t1 WHERE a=2")
        end, {
            "two"
        })

end

for _, kw in ipairs(bannedkws) do
    query = 'CREATE TABLE '..kw..'(a INT PRIMARY KEY);'
    test:do_catchsql_test(
        "keyword1-"..kw..".1",
        query, {
            1, 'keyword "'..kw..'" is reserved'
        })
end


test:finish_test()
