#!/usr/bin/env tarantool
test = require("sqltester")
test:plan(35)

local test_prefix = "identifier_case-"

local data = {
    { 1,  [[ table1 ]], {0} },
    { 2,  [[ Table1 ]], {"/already exists/"} },
    { 3,  [[ TABLE1 ]], {"/already exists/"} },
    { 4,  [[ "TABLE1" ]], {"/already exists/"} },
    { 5,  [[ "table1" ]], {0} },
    { 6,  [[ "Table1" ]], {0} }
}

for _, row in ipairs(data) do
    test:do_catchsql_test(
        test_prefix.."1.1."..row[1],
        string.format( [[
                CREATE TABLE %s (a int PRIMARY KEY);
                INSERT INTO %s values (%s);
                ]], row[2], row[2], row[1]),
        row[3])
end

data = {
    { 1, [[ table1 ]], {1}},
    { 2, [[ Table1 ]], {1}},
    { 3, [[ TABLE1 ]], {1}},
    { 4, [[ "TABLE1" ]], {1}},
    { 5, [[ "table1" ]], {5}},
    { 6, [[ "Table1" ]], {6}}
}

for _, row in ipairs(data) do
    test:do_execsql_test(
        test_prefix.."1.2."..row[1],
        string.format([[
            SELECT * FROM %s;
            ]], row[2]),
        row[3])
end

test:do_test(
    test_prefix.."1.3.1",
    function ()
        return #test:drop_all_tables()
    end,
    3)

data = {
    { 1,  [[ column ]], {0} },
    { 2,  [[ Column ]], {0} },
    { 3,  [[ COLUMN ]], {0} },
    { 4,  [[ "COLUMN" ]], {0} },
    { 5,  [[ "column" ]], {0} },
    { 6,  [[ "Column" ]], {0} },
    { 7,  [[ "columN" ]], {1, "/duplicate column name/"} }
}

for _, row in ipairs(data) do
    test:do_catchsql_test(
        test_prefix.."2.1."..row[1],
        string.format( [[
                CREATE TABLE table%s ("columN", %s, primary key("columN", %s));
                INSERT INTO table%s(%s, "columN") values (%s, %s);
                ]],
                row[1], row[2], row[2],
                row[1], row[2], row[1], row[1]+1),
        row[3])
end


data = {
    { 1,  [[ column ]], },
    { 2,  [[ Column ]], },
    { 3,  [[ COLUMN ]], },
    { 4,  [[ "COLUMN" ]], },
    { 5,  [[ "column" ]], },
    { 6,  [[ "Column" ]], }
}

for _, row in ipairs(data) do
    test:do_execsql_test(
        test_prefix.."2.2."..row[1],
        string.format([[
            SELECT %s FROM table%s;
            ]], row[2], row[1]),
        {row[1]})
end

test:do_test(
    test_prefix.."2.3.1",
    function ()
        return #test:drop_all_tables()
    end,
    6)

test:execsql([[create table table1(column, "column" primary key)]])
test:execsql([[insert into table1("column", "COLUMN") values(2,1)]])


data = {
    --tn  lookup_cln    lookup_val  set_cln       set_val result
    { 1,  [[ column ]], 1,          [[ column ]], 3 ,     {3,2} },
    { 2,  [[ Column ]], 3,          [[ column ]], 4 ,     {4,2} },
    { 3,  [[ column ]], 4,          [[ Column ]], 5 ,     {5,2} },
    { 4,  [["COLUMN"]], 5,          [[ column ]], 6 ,     {6,2} },
    { 5,  [[ column ]], 6,          [["COLUMN"]], 7 ,     {7,2} },
    { 6,  [["column"]], 2,          [["column"]], 3 ,     {7,3} },
    { 7,  [["column"]], 3,          [[ column ]], 8 ,     {8,3} },
    { 8,  [["column"]], 8,          [[ column ]], 1000 ,  {8,3} },
}

for _, row in ipairs(data) do
    local tn = row[1]
    local lookup_cln = row[2]
    local lookup_val = row[3]
    local set_cln = row[4]
    local set_val = row[5]
    local result = row[6]
    test:do_execsql_test(
        test_prefix.."3.1."..tn,
        string.format( [[
                UPDATE table1 set %s = %s where %s = %s;
                SELECT table1.column, "TABLE1"."column" FROM table1;
                ]],
            set_cln, set_val, lookup_cln, lookup_val),
        result)
end

test:finish_test()
