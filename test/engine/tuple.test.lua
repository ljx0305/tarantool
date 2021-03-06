test_run = require('test_run').new()
engine = test_run:get_cfg('engine')
test_run:cmd("push filter 'Failed to allocate [0-9]+' to 'Failed to allocate <NUM>'")
test_run:cmd("push filter '"..engine.."_max_tuple_size' to '<ENGINE>_max_tuple_size'")

-- https://github.com/tarantool/tarantool/issues/2667
-- Allow to insert tuples bigger than `max_tuple_size'
s = box.schema.space.create('test', { engine = engine })
_ = s:create_index('primary')

engine_max_tuple_size = engine ..'_max_tuple_size'
engine_tuple_size = engine == 'memtx' and 16 or 32

-- check max_tuple_size limit
max_tuple_size = box.cfg[engine_max_tuple_size]
_ = s:replace({1, string.rep('x', max_tuple_size)})

-- check max_tuple_size dynamic configuration
box.cfg { [engine_max_tuple_size] = 2 * max_tuple_size }
_ = s:replace({1, string.rep('x', max_tuple_size)})

-- check tuple sie
box.cfg { [engine_max_tuple_size] = engine_tuple_size + 2 }
_ = s:replace({1})

-- check large tuples allocated on malloc
box.cfg { [engine_max_tuple_size] = 32 * 1024 * 1024 }
_ = s:replace({1, string.rep('x', 32 * 1024 * 1024 - engine_tuple_size - 8)})

-- decrease max_tuple_size limit
box.cfg { [engine_max_tuple_size] = 1 * 1024 * 1024 }
_ = s:replace({1, string.rep('x', 2 * 1024 * 1024 )})
_ = s:replace({1, string.rep('x', 1 * 1024 * 1024 - engine_tuple_size - 8)})

-- gh-2698 Tarantool crashed on 4M tuple
max_item_size = 0
test_run:cmd("setopt delimiter ';'")
for _, v in pairs(box.slab.stats()) do
    max_item_size = math.max(max_item_size, v.item_size)
end;
test_run:cmd("setopt delimiter ''");
box.cfg { [engine_max_tuple_size] = max_item_size + engine_tuple_size + 8 }
_ = box.space.test:replace{1, 1, string.rep('a', max_item_size)}

-- reset to original value
box.cfg { [engine_max_tuple_size] = max_tuple_size }

s:drop();
collectgarbage('collect') -- collect all large tuples
box.snapshot() -- discard xlogs with large tuples

test_run:cmd("clear filter")
engine = nil
test_run = nil
