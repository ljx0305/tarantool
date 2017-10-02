#!/usr/bin/env tarantool
test = require("sqltester")
test:plan(11)

--!./tcltestrunner.lua
-- 2010 December 6
--
-- The author disclaims copyright to this source code.  In place of
-- a legal notice, here is a blessing:
--
--    May you do good and not evil.
--    May you find forgiveness for yourself and forgive others.
--    May you share freely, never taking more than you give.
--
-------------------------------------------------------------------------
-- This file implements regression tests for SQLite library. Specifically,
-- it tests that ticket [80ba201079ea608071d22a57856b940ea3ac53ce] is
-- resolved.  That ticket is about an incorrect result that appears when
-- an index is added.  The root cause is that a constant is being used
-- without initialization when the OR optimization applies in the WHERE clause.
--
-- ["set","testdir",[["file","dirname",["argv0"]]]]
-- ["source",[["testdir"],"\/tester.tcl"]]

testprefix = "tkt-80ba201079"
test:do_test(
    "tkt-80ba2-100",
    function()
        return test:execsql [[
            CREATE TABLE t1(a primary key);
            INSERT INTO t1 VALUES('A');
            CREATE TABLE t2(b primary key);
            INSERT INTO t2 VALUES('B');
            CREATE TABLE t3(c primary key);
            INSERT INTO t3 VALUES('C');
            SELECT * FROM t1, t2
             WHERE (a='A' AND b='X')
                OR (a='A' AND EXISTS (SELECT * FROM t3 WHERE c='C'));
        ]]
    end, {
        -- <tkt-80ba2-100>
        "A", "B"
        -- </tkt-80ba2-100>
    })

test:do_test(
    "tkt-80ba2-101",
    function()
        return test:execsql [[
            CREATE INDEX i1 ON t1(a);
            SELECT * FROM t1, t2
             WHERE (a='A' AND b='X')
                OR (a='A' AND EXISTS (SELECT * FROM t3 WHERE c='C'));
        ]]
    end, {
        -- <tkt-80ba2-101>
        "A", "B"
        -- </tkt-80ba2-101>
    })

-- do_test tkt-80ba2-102 {
--   optimization_control db factor-constants 0
--   db cache flush
--   db eval {
--     SELECT * FROM t1, t2
--      WHERE (a='A' AND b='X')
--         OR (a='A' AND EXISTS (SELECT * FROM t3 WHERE c='C'));
--   }
-- } {A B}
-- optimization_control db all 1
-- # Verify that the optimization_control command is actually working
-- #
-- do_test tkt-80ba2-150 {
--   optimization_control db factor-constants 1
--   db cache flush
--   set x1 [db eval {EXPLAIN 
--     SELECT * FROM t1, t2
--      WHERE (a='A' AND b='X')
--         OR (a='A' AND EXISTS (SELECT * FROM t3 WHERE c='C'));}]
--   optimization_control db factor-constants 0
--   db cache flush
--   set x2 [db eval {EXPLAIN 
--     SELECT * FROM t1, t2
--      WHERE (a='A' AND b='X')
--         OR (a='A' AND EXISTS (SELECT * FROM t3 WHERE c='C'));}]
--   expr {$x1==$x2}
-- } {0}
test:do_test(
    "tkt-80ba2-200",
    function()
        return test:execsql [[
            CREATE TABLE entry_types (
                                id     integer primary key,
                                name   text
                            );
            INSERT INTO "entry_types" VALUES(100,'cli_command');
            INSERT INTO "entry_types" VALUES(300,'object_change');
            CREATE TABLE object_changes (
                                change_id    integer primary key,
                                system_id    int,
                                obj_id       int,
                                obj_context  text,
                                change_type  int,
                                command_id   int
                            );
            INSERT INTO "object_changes" VALUES(1551,1,114608,'exported_pools',1,2114);
            INSERT INTO "object_changes" VALUES(2048,1,114608,'exported_pools',2,2319);
            CREATE TABLE timeline (
                                rowid        integer primary key,
                                timestamp_t    text,
                                system_id    int,
                                entry_type   int,
                                entry_id     int
                            );
            INSERT INTO "timeline" VALUES(6735,'2010-11-21 17:08:27.000',1,300,2048);
            INSERT INTO "timeline" VALUES(6825,'2010-11-21 17:09:21.000',1,300,2114);
            SELECT entry_type,
                   entry_types.name,
                   entry_id
              FROM timeline JOIN entry_types ON entry_type = entry_types.id
             WHERE (entry_types.name = 'cli_command' AND entry_id=2114)
                OR (entry_types.name = 'object_change'
                     AND entry_id IN (SELECT change_id
                                      FROM object_changes
                                       WHERE obj_context = 'exported_pools'));
        ]]
    end, {
        -- <tkt-80ba2-200>
        300, "object_change", 2048
        -- </tkt-80ba2-200>
    })

test:do_test(
    "tkt-80ba2-201",
    function()
        return test:execsql [[
            CREATE INDEX timeline_entry_id_idx on timeline(entry_id);
            SELECT entry_type,
                   entry_types.name,
                   entry_id
              FROM timeline JOIN entry_types ON entry_type = entry_types.id
             WHERE (entry_types.name = 'cli_command' AND entry_id=2114)
                OR (entry_types.name = 'object_change'
                     AND entry_id IN (SELECT change_id
                                      FROM object_changes
                                       WHERE obj_context = 'exported_pools'));
        ]]
    end, {
        -- <tkt-80ba2-201>
        300, "object_change", 2048
        -- </tkt-80ba2-201>
    })

-- do_test tkt-80ba2-202 {
--   optimization_control db factor-constants 0
--   db cache flush
--   db eval {
--     SELECT entry_type,
--            entry_types.name,
--            entry_id
--       FROM timeline JOIN entry_types ON entry_type = entry_types.id
--      WHERE (entry_types.name = 'cli_command' AND entry_id=2114)
--         OR (entry_types.name = 'object_change'
--              AND entry_id IN (SELECT change_id
--                               FROM object_changes
--                                WHERE obj_context = 'exported_pools'));
--   }
-- } {300 object_change 2048}
---------------------------------------------------------------------------
--
test:execsql([[
    DROP TABLE t1;
    DROP TABLE t2;
    DROP TABLE t3;
]])
test:do_execsql_test(
    301,
    [[
        CREATE TABLE t1(a primary key, b, c);
        CREATE INDEX i1 ON t1(a);
        CREATE INDEX i2 ON t1(b);
        CREATE TABLE t2(d primary key, e);

        INSERT INTO t1 VALUES('A', 'B', 'C');
        INSERT INTO t2 VALUES('D', 'E');
    ]])

test:do_execsql_test(
    302,
    [[
        SELECT * FROM t1, t2 WHERE
          (a='A' AND d='E') OR
          (b='B' AND c IN ('C', 'D', 'E'))
    ]], {
        -- <302>
        "A", "B", "C", "D", "E"
        -- </302>
    })

test:do_execsql_test(
    303,
    [[
        SELECT * FROM t1, t2 WHERE
          (a='A' AND d='E') OR
          (b='B' AND c IN (SELECT c FROM t1))
    ]], {
        -- <303>
        "A", "B", "C", "D", "E"
        -- </303>
    })

test:do_execsql_test(
    304,
    [[
        SELECT * FROM t1, t2 WHERE
          (a='A' AND d='E') OR
          (b='B' AND c IN (SELECT 'B' UNION SELECT 'C' UNION SELECT 'D'))
    ]], {
        -- <304>
        "A", "B", "C", "D", "E"
        -- </304>
    })



test:do_execsql_test(
    305,
    [[
        SELECT * FROM t1, t2 WHERE
          (b='B' AND c IN ('C', 'D', 'E')) OR
          (a='A' AND d='E')
    ]], {
        -- <305>
        "A", "B", "C", "D", "E"
        -- </305>
    })

test:do_execsql_test(
    306,
    [[
        SELECT * FROM t1, t2 WHERE
          (b='B' AND c IN (SELECT c FROM t1)) OR
          (a='A' AND d='E')
    ]], {
        -- <306>
        "A", "B", "C", "D", "E"
        -- </306>
    })

test:do_execsql_test(
    307,
    [[
        SELECT * FROM t1, t2 WHERE
          (b='B' AND c IN (SELECT 'B' UNION SELECT 'C' UNION SELECT 'D')) OR
          (a='A' AND d='E')
    ]], {
        -- <307>
        "A", "B", "C", "D", "E"
        -- </307>
    })



test:finish_test()

