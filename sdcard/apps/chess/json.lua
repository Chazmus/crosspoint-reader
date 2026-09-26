-- Simple pure-Lua JSON decoder for CrossPoint
local json = {}

local function parse_value(str, i)
    local c = str:sub(i, i)
    while c == ' ' or c == '\t' or c == '\n' or c == '\r' do
        i = i + 1
        c = str:sub(i, i)
    end
    if c == '{' then
        local obj = {}
        i = i + 1
        while true do
            while str:sub(i, i):match('%s') do i = i + 1 end
            if str:sub(i, i) == '}' then return obj, i + 1 end
            local key, next_i = parse_value(str, i)
            i = next_i
            while str:sub(i, i):match('%s') do i = i + 1 end
            if str:sub(i, i) == ':' then i = i + 1 end
            local val, next_i2 = parse_value(str, i)
            obj[key] = val
            i = next_i2
            while str:sub(i, i):match('%s') do i = i + 1 end
            if str:sub(i, i) == ',' then i = i + 1
            elseif str:sub(i, i) == '}' then return obj, i + 1
            else return obj, i end
        end
    elseif c == '[' then
        local arr = {}
        i = i + 1
        while true do
            while str:sub(i, i):match('%s') do i = i + 1 end
            if str:sub(i, i) == ']' then return arr, i + 1 end
            local val, next_i = parse_value(str, i)
            table.insert(arr, val)
            i = next_i
            while str:sub(i, i):match('%s') do i = i + 1 end
            if str:sub(i, i) == ',' then i = i + 1
            elseif str:sub(i, i) == ']' then return arr, i + 1
            else return arr, i end
        end
    elseif c == '"' then
        local j = i + 1
        while j <= #str do
            if str:sub(j, j) == '\\' then j = j + 2
            elseif str:sub(j, j) == '"' then
                return str:sub(i + 1, j - 1), j + 1
            else j = j + 1 end
        end
        return str:sub(i + 1), #str + 1
    elseif c == 't' and str:sub(i, i + 3) == 'true' then return true, i + 4
    elseif c == 'f' and str:sub(i, i + 4) == 'false' then return false, i + 5
    elseif c == 'n' and str:sub(i, i + 3) == 'null' then return nil, i + 4
    else
        local j = i
        while j <= #str and str:sub(j, j):match('[-0-9.+eE]') do j = j + 1 end
        return tonumber(str:sub(i, j - 1)) or str:sub(i, j - 1), j
    end
end

function json.decode(str)
    if not str or str == "" then return nil end
    local val, _ = parse_value(str, 1)
    return val
end

return json
