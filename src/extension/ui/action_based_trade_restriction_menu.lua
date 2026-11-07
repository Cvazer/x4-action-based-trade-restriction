-- The game's own list editor, taken over where it stands, and the events that tie the editor to the
-- DLL.
--
-- The page gains the sets when it draws, its Confirm, Reset and Remove carry them, and the game's
-- faction panel doubles as their picker.

local store = ActionBasedTradeRestriction.store
local editor = ActionBasedTradeRestriction.editor

local page = nil   -- the game's PlayerInfoMenu, once taken over

local function refresh_if_shown()
	if page and page.shown and page.refreshInfoFrame then page.refreshInfoFrame() end
end

-- The DLL reports the table it holds. The first report after a Confirm is what Reset goes back to.
local function on_reported(_, table_text)
	store.replace(table_text)
	if page and page.__abtr_take_saved then
		page.__abtr_take_saved = false
		page.__abtr_saved_sets = store.report
	end
	refresh_if_shown()
end

local function take_over(candidate)
	if type(candidate) ~= "table" then return false end
	if candidate.__scoped_original then return true end
	local build = candidate.createEmpire
	local confirm = candidate.buttonBlacklistConfirm
	if type(build) ~= "function" then return false end
	if type(confirm) ~= "function" then return false end

	page = candidate

	-- The page's own faction list, as it was before a picker borrowed it.
	local function give_back_factions()
		local edited = editor.edited_entry(candidate)
		if edited and candidate.__abtr_vanilla_factions then
			local restore = {}
			for i, id in ipairs(candidate.__abtr_vanilla_factions) do restore[i] = id end
			edited.factions = restore
		end
	end

	-- A close while the picker is open is its Cancel: the page's own factions go back and the game's own
	-- close handler runs, which closes its panel alone.
	local close_page = candidate.onCloseElement
	if type(close_page) == "function" then
		candidate.onCloseElement = function (dueToClose)
			if editor.picking then
				editor.picking = nil
				give_back_factions()
				local result = close_page(dueToClose)
				if candidate.refreshInfoFrame then candidate.refreshInfoFrame() end
				return result
			end
			return close_page(dueToClose)
		end
	end

	candidate.__scoped_original = confirm
	candidate.buttonBlacklistConfirm = function (...)
		candidate.__abtr_dirty = false
		confirm(...)
		local entry = editor.editing_trade_list(candidate)
		if entry and entry.id and entry.id ~= 0 then
			store.adopt_unsaved(entry.id)
			candidate.__abtr_take_saved = true
			store.send()
			if candidate.refreshInfoFrame then candidate.refreshInfoFrame() end
		end
	end

	local check_changes = candidate.buttonBlacklistCheckChanges
	if type(check_changes) == "function" then
		candidate.buttonBlacklistCheckChanges = function (...)
			if candidate.__abtr_dirty then return true end
			return check_changes(...)
		end
	end

	local reset_changes = candidate.buttonBlacklistReset
	if type(reset_changes) == "function" then
		candidate.buttonBlacklistReset = function (...)
			candidate.__abtr_dirty = false
			if candidate.__abtr_saved_sets then
				store.sets[0] = nil
				store.replace(candidate.__abtr_saved_sets)
				store.send()
			end
			return reset_changes(...)
		end
	end

	local remove_list = candidate.buttonBlacklistRemove
	if type(remove_list) == "function" then
		candidate.buttonBlacklistRemove = function (...)
			local id = candidate.editedBlacklist and candidate.editedBlacklist.id
			remove_list(...)
			if id and store.sets[id] then
				store.sets[id] = nil
				store.send()
			end
		end
	end

	-- The game's panel hands its whole selection back when it closes. While a picker is open that
	-- selection is the set being edited. The game's own handler still runs, since it is what closes the
	-- panel, and the page's own faction list it writes is put back as it was.
	local select_set = candidate.buttonSelectContextSetList
	if type(select_set) == "function" then
		candidate.buttonSelectContextSetList = function (data)
			if not editor.picking then return select_set(data) end
			local target = store.set_of(editor.picking.listid, editor.picking.which)

			pcall(select_set, data)
			give_back_factions()

			local staged = {}
			local hasplayer = false
			for faction in pairs(data or {}) do
				if faction == "player" then hasplayer = true
				else staged[#staged + 1] = faction end
			end
			if Helper.sortFactionName then table.sort(staged, Helper.sortFactionName) end
			if hasplayer then table.insert(staged, 1, "player") end
			target.factions = staged
			editor.picking = nil
			candidate.__abtr_dirty = true
			store.send()
			if candidate.refreshInfoFrame then candidate.refreshInfoFrame() end
		end
	end

	candidate.createEmpire = function (frame, properties)
		local proxy, created = editor.capturing(frame)
		local result = build(proxy, properties)
		local open_list = editor.editing_trade_list(candidate)
		local list_id = open_list and (open_list.id or 0) or nil
		-- Once per list: the sets as they were when it was opened, for Reset, and the page's own faction
		-- list, to put back exactly after the picker borrowed it.
		if open_list and candidate.__abtr_vanilla_for ~= list_id then
			candidate.__abtr_vanilla_for = list_id
			candidate.__abtr_dirty = false
			candidate.__abtr_saved_sets = store.report
			local edited = editor.edited_entry(candidate)
			local copy = {}
			if edited and edited.factions then
				for i, id in ipairs(edited.factions) do copy[i] = id end
			end
			candidate.__abtr_vanilla_factions = copy
		end
		if editor.picking and (not open_list or editor.picking.listid ~= list_id) then editor.picking = nil end
		if open_list then
			local centre = editor.centre_of(created)
			-- The page rebuilds itself on every change and this runs on every rebuild. The blocks go in once
			-- per centre table: adding them again would grow the page and push its own buttons away from
			-- where they draw.
			if centre and candidate.__abtr_centre ~= centre then
				candidate.__abtr_centre = centre
				local faction_names = editor.faction_names()
				editor.add_set_block(candidate, centre, "sell", "Separate sell restriction", list_id, faction_names)
				editor.add_set_block(candidate, centre, "buy", "Separate buy restriction", list_id, faction_names)
			end
		end
		return result
	end
	return true
end

local function find_menu(name)
	if type(Menus) ~= "table" then return nil end
	for _, menu in ipairs(Menus) do
		if menu and menu.name == name then return menu end
	end
	return nil
end

-- Every menu registers itself through `Helper.registerMenu` as the game creates it, and the game's own
-- scripts load after this one. `Helper` is not there yet while this file loads, so watching starts at
-- the first later signal.
local watching = false

local function start_watching()
	take_over(find_menu("PlayerInfoMenu"))
	if watching or type(Helper) ~= "table" or type(Helper.registerMenu) ~= "function" then return end
	watching = true
	local register = Helper.registerMenu
	Helper.registerMenu = function (registered, ...)
		local result = register(registered, ...)
		if type(registered) == "table" and registered.name == "PlayerInfoMenu" then take_over(registered) end
		return result
	end
end

RegisterEvent("action_based_trade_restriction.sets", on_reported)

-- The DLL arms the UI on every game load, once its table holds what the save carried. Arming is
-- idempotent; each one asks the DLL for its table, which a load has just replaced.
RegisterEvent("action_based_trade_restriction.arm", function ()
	start_watching()
	store.raise_native("action_based_trade_restriction.request", nil)
end)

Menus = Menus or {}

RegisterEvent("gfx_ok", start_watching)
RegisterEvent("show", start_watching)
start_watching()
