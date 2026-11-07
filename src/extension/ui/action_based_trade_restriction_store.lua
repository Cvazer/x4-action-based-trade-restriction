-- The player's separate sets, by list id, as the DLL last reported them, and the way an edit reaches
-- the DLL, which owns them and saves them with the game.
--
-- A list that has no id yet is held under id 0. The DLL keys everything by list id, so that list is never
-- sent until the game gives it one.

local wire = ActionBasedTradeRestriction.wire
local store = { sets = {}, report = "" }
ActionBasedTradeRestriction.store = store

function store.set_of(listid, which)
	local sets = store.sets
	sets[listid] = sets[listid] or {}
	sets[listid][which] = sets[listid][which] or { enabled = false, whitelist = true, factions = {} }
	return sets[listid][which]
end

-- To the DLL, through X4Native's bridge. The bridge is installed by X4Native's own script, which loads
-- after this one, so it is looked up each time rather than captured here.
function store.raise_native(name, param)
	local bridge = _G.__X4NATIVE_API
	if bridge then bridge.raise_event(name, param) end
end

-- Each list's entry to the DLL, which replaces its rule by id and answers with the whole table.
function store.send()
	for id, entry in pairs(store.sets) do
		if id ~= 0 then
			store.raise_native("action_based_trade_restriction.set", wire.encode_entry(id, entry))
		end
	end
end

-- The table as the DLL reports it. What is staged for a list with no id is kept: the report cannot carry
-- it, and the player's ticks would be wiped a moment after they were made.
function store.replace(text)
	local unsaved = store.sets[0]
	store.report = text or ""
	store.sets = wire.decode(store.report)
	if unsaved then store.sets[0] = unsaved end
end

-- A new list was given its real id, so its sets move off the placeholder.
function store.adopt_unsaved(id)
	if store.sets[0] then
		store.sets[id] = store.sets[0]
		store.sets[0] = nil
	end
end
