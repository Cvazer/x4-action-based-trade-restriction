-- The messages the separate sets travel in between the list editor and the DLL, as JSON.
--
-- One list's entry:
--   {"list":3,"sell":{"enabled":true,"whitelist":false,"factions":["terran"]},
--    "buy":{"enabled":true,"whitelist":true,"factions":["argon","paranid"]}}
-- The table the DLL reports is a JSON array of such entries. The game's UI offers no JSON library, so
-- the encoder and decoder here cover what the two shapes use: objects, arrays, strings, integers and
-- booleans.

ActionBasedTradeRestriction = ActionBasedTradeRestriction or {}
local wire = {}
ActionBasedTradeRestriction.wire = wire

local escapes = { ['"'] = '\\"', ["\\"] = "\\\\", ["\b"] = "\\b", ["\f"] = "\\f", ["\n"] = "\\n", ["\r"] = "\\r", ["\t"] = "\\t" }

local function encode_string(text)
	return '"' .. string.gsub(text, '[%c"\\]', function (c)
		return escapes[c] or string.format("\\u%04x", string.byte(c))
	end) .. '"'
end

local function encode_set(set)
	local factions = {}
	for i, faction in ipairs(set.factions) do factions[i] = encode_string(faction) end
	return '{"enabled":' .. tostring(set.enabled and true or false) .. ',"whitelist":' .. tostring(set.whitelist and true or false)
		.. ',"factions":[' .. table.concat(factions, ",") .. "]}"
end

function wire.encode_entry(listid, entry)
	local parts = { '{"list":' .. string.format("%d", listid) }
	for _, which in ipairs({ "sell", "buy" }) do
		local set = entry[which] or { enabled = false, whitelist = false, factions = {} }
		parts[#parts + 1] = '"' .. which .. '":' .. encode_set(set)
	end
	return table.concat(parts, ",") .. "}"
end

-- A JSON value read from `text` at `pos`: the value and the position after it, or nil on malformed text.
local read_value

local function skip(text, pos)
	return string.find(text, "[^ \t\r\n]", pos) or #text + 1
end

local function read_string(text, pos)
	local out = {}
	local i = pos + 1
	while i <= #text do
		local c = string.sub(text, i, i)
		if c == '"' then return table.concat(out), i + 1 end
		if c == "\\" then
			local e = string.sub(text, i + 1, i + 1)
			local simple = { ['"'] = '"', ["\\"] = "\\", ["/"] = "/", b = "\b", f = "\f", n = "\n", r = "\r", t = "\t" }
			if simple[e] then
				out[#out + 1] = simple[e]
				i = i + 2
			elseif e == "u" then
				local code = tonumber(string.sub(text, i + 2, i + 5), 16)
				if not code or code > 127 then return nil end
				out[#out + 1] = string.char(code)
				i = i + 6
			else
				return nil
			end
		else
			out[#out + 1] = c
			i = i + 1
		end
	end
	return nil
end

local function read_array(text, pos)
	local result = {}
	pos = skip(text, pos + 1)
	if string.sub(text, pos, pos) == "]" then return result, pos + 1 end
	while true do
		local value
		value, pos = read_value(text, pos)
		if pos == nil then return nil end
		result[#result + 1] = value
		pos = skip(text, pos)
		local c = string.sub(text, pos, pos)
		if c == "]" then return result, pos + 1 end
		if c ~= "," then return nil end
		pos = skip(text, pos + 1)
	end
end

local function read_object(text, pos)
	local result = {}
	pos = skip(text, pos + 1)
	if string.sub(text, pos, pos) == "}" then return result, pos + 1 end
	while true do
		if string.sub(text, pos, pos) ~= '"' then return nil end
		local key
		key, pos = read_string(text, pos)
		if pos == nil then return nil end
		pos = skip(text, pos)
		if string.sub(text, pos, pos) ~= ":" then return nil end
		local value
		value, pos = read_value(text, skip(text, pos + 1))
		if pos == nil then return nil end
		result[key] = value
		pos = skip(text, pos)
		local c = string.sub(text, pos, pos)
		if c == "}" then return result, pos + 1 end
		if c ~= "," then return nil end
		pos = skip(text, pos + 1)
	end
end

read_value = function (text, pos)
	pos = skip(text, pos)
	local c = string.sub(text, pos, pos)
	if c == "{" then return read_object(text, pos) end
	if c == "[" then return read_array(text, pos) end
	if c == '"' then return read_string(text, pos) end
	if string.sub(text, pos, pos + 3) == "true" then return true, pos + 4 end
	if string.sub(text, pos, pos + 4) == "false" then return false, pos + 5 end
	local number = string.match(text, "^-?%d+", pos)
	if number then return tonumber(number), pos + #number end
	return nil
end

local function decode_set(value)
	local set = { enabled = false, whitelist = false, factions = {} }
	if type(value) ~= "table" then return set end
	set.enabled = value.enabled == true
	set.whitelist = value.whitelist == true
	for _, faction in ipairs(value.factions or {}) do
		if type(faction) == "string" then set.factions[#set.factions + 1] = faction end
	end
	return set
end

-- The table as { [id] = { sell = set, buy = set } }, each set { enabled, whitelist, factions }. Text that
-- is not a JSON array of entries reads as an empty table.
function wire.decode(text)
	local result = {}
	if not text or text == "" then return result end
	local entries = read_value(text, 1)
	if type(entries) ~= "table" then return result end
	for _, entry in ipairs(entries) do
		if type(entry) == "table" and type(entry.list) == "number" then
			result[entry.list] = { sell = decode_set(entry.sell), buy = decode_set(entry.buy) }
		end
	end
	return result
end
