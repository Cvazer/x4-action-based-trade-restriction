-- The separate sets, drawn into the game's own list editor page.
--
-- A trade list's page gains a sell restriction and a buy restriction, each with its switch, its
-- allow-or-refuse switch, its factions and a picker. The picker is the game's own faction panel.

local ffi = require("ffi")
local C = ffi.C

ffi.cdef[[
	uint32_t GetAllFactions(const char** result, uint32_t resultlen, bool includehidden);
	uint32_t GetNumAllFactions(bool includehidden);
]]

local store = ActionBasedTradeRestriction.store
local editor = {
	picking = nil,   -- { listid = .., which = .. } while one of the pickers is open
}
ActionBasedTradeRestriction.editor = editor

-- Every faction the player can name, by id, as the page shows it: "Name (id)".
function editor.faction_names()
	local names = {}
	local count = C.GetNumAllFactions(false)
	local buf = ffi.new("const char*[?]", count > 0 and count or 1)
	count = C.GetAllFactions(buf, count, false)
	for i = 0, count - 1 do
		local id = ffi.string(buf[i])
		local name = GetFactionData(id, "name")
		names[id] = name and (tostring(name) .. " (" .. id .. ")") or id
	end
	return names
end

-- The page keeps its edits in a working copy, `editedBlacklist`, which the game draws the page from.
function editor.edited_entry(page)
	return page.editedBlacklist
end

-- The working copy of the list being edited, when that list is a trade blacklist in the global orders.
function editor.editing_trade_list(page)
	if page.mode ~= "globalorders" then return nil end
	local mode = page.empireData and page.empireData.mode
	if not (mode and mode[1] == "empire_list") then return nil end
	-- The list's type lands in the working copy, so it is read there and not from `mode[3]`, which is
	-- still the empty table while a list is being created.
	local entry = editor.edited_entry(page)
	if not entry then return nil end
	-- A trade rule is already applied per activity by the game itself, so the sets belong to a blacklist.
	if mode[2] ~= "blacklist" then return nil end
	-- They appear once the list's type is a trade list, and not on a travel or fight list.
	if entry.type == "objectactivity" then return entry end
	return nil
end

-- The game draws the page into a frame it lays out itself. Drawing it through a frame that records the
-- tables it creates finds the centre table, where the page's own controls live.
function editor.capturing(frame)
	local created = {}
	local proxy = setmetatable({}, {
		__index = function (_, key)
			if key == "addTable" then
				return function (_, ...)
					local grid = frame:addTable(...)
					created[#created + 1] = grid
					return grid
				end
			end
			local value = frame[key]
			if type(value) == "function" then
				return function (_, ...) return value(frame, ...) end
			end
			return value
		end,
		-- Writes reach the real frame, or the page reads back nil for what it stored and its own
		-- controls stop working.
		__newindex = function (_, key, value) frame[key] = value end,
	})
	return proxy, created
end

function editor.centre_of(created)
	local sorted = {}
	for _, grid in ipairs(created) do
		local properties = grid.properties
		if properties and properties.x then sorted[#sorted + 1] = grid end
	end
	table.sort(sorted, function (a, b) return a.properties.x < b.properties.x end)
	return sorted[2] or sorted[1]
end

-- An edit: the page's Confirm Changes button lights up, the set is sent, and the page redraws.
local function changed(page)
	page.__abtr_dirty = true
	store.send()
	page.refreshInfoFrame()
end

local function toggle(page, listid, which, field, checked)
	store.set_of(listid, which)[field] = checked and true or false
	changed(page)
end

-- One restriction in the page's own table: its label and switch and, while it is on, its
-- allow-or-refuse switch, one row per faction with an x that removes it, and the picker.
function editor.add_set_block(page, centre, which, label, listid, faction_names)
	local set = store.set_of(listid, which)

	local row = centre:addRow(true, {})
	row[1]:createText(label .. ReadText(1001, 120))
	row[5]:createCheckBox(set.enabled, {})
	row[5].handlers.onClick = function (_, checked) toggle(page, listid, which, "enabled", checked) end
	if not set.enabled then return end

	local mode = centre:addRow(true, {})
	mode[2]:setColSpan(3):createText(set.whitelist and "only these pass" or "these are refused", { halign = "left" })
	mode[5]:createCheckBox(set.whitelist, {})
	mode[5].handlers.onClick = function (_, checked) toggle(page, listid, which, "whitelist", checked) end

	for index = 1, #set.factions do
		local faction_id = set.factions[index]
		local entry = centre:addRow(true, {})
		entry[2]:setColSpan(3):createText("    " .. (faction_names[faction_id] or faction_id), { halign = "left" })
		entry[5]:createButton({}):setText("x", { halign = "center" })
		entry[5].handlers.onClick = function ()
			local target = store.set_of(listid, which)
			for i, id in ipairs(target.factions) do
				if id == faction_id then table.remove(target.factions, i) break end
			end
			changed(page)
		end
	end

	-- The game's panel opens beside the page, where the vanilla Add / Remove Faction button opens it, and
	-- hands its whole selection back when it closes; the set is written then, never while ticking.
	local pick = centre:addRow(true, {})
	pick[2]:setColSpan(4):createButton({}):setText("Pick factions", { halign = "center" })
	pick[2].handlers.onClick = function ()
		-- The panel ticks whatever the page's own faction list holds, so that list is lent the set's
		-- factions while the panel is open.
		local edited = editor.edited_entry(page)
		if edited then
			local lent = {}
			for i, id in ipairs(set.factions) do lent[i] = id end
			edited.factions = lent
		end
		editor.picking = { listid = listid, which = which }
		local properties = centre.properties or {}
		local panel_x = (properties.x or 0) + (properties.width or 0) + (Helper.borderSize or 0)
		local panel_y = properties.y or 0
		if type(page.buttonBlacklistAddFaction) == "function" then
			page.buttonBlacklistAddFaction(panel_x, panel_y)
		end
	end
end
