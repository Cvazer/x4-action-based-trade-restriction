-- The fake UI surface. Everything the script draws lands in DRAWN, every table the DLL reports in SENT.
-- The names of the events the mod's layers exchange, as the checks raise and read them.
EVENTS = {
    set = "action_based_trade_restriction.set",          -- Lua -> DLL: one list's entry
    request = "action_based_trade_restriction.request",  -- Lua -> DLL: report the table
    sets = "action_based_trade_restriction.sets",        -- DLL -> Lua: the table it holds
    arm = "action_based_trade_restriction.arm",          -- DLL -> Lua: a load filled the table
}

DRAWN = {}
SENT = {}
SUBS = {}
ROW = 0
GRIDS = {}
FRAMES = 0
CONTEXT = nil
VANILLA_SELECTED = nil
FRAME_PROPS = nil
MENUS = {}
REFRESHED = 0

function RegisterEvent(name, fn) SUBS[name] = fn end

-- The lists the game holds; a list the player removes leaves this set.
LIVE = { [1] = true, [2] = true, [3] = true, [4] = true }

-- The checks read and write entries in a shorthand, `<id>|sell:<enabled><whitelist>:<factions>|buy:...`
-- with the flags one digit each and the factions joined by ",", and the messages are JSON. These two
-- translate between them, for the shapes the DLL and the editor exchange.
function shorthand(message)
    local entries = {}
    for id, sell, buy in string.gmatch(message or "", '{"list":(%d+),"sell":(%b{}),"buy":(%b{})}') do
        local parts = { id }
        for _, pair in ipairs({ { "sell", sell }, { "buy", buy } }) do
            local enabled, whitelist, factions =
                string.match(pair[2], '"enabled":(%a+),"whitelist":(%a+),"factions":%[(.-)%]')
            local names = {}
            for name in string.gmatch(factions or "", '"([^"]*)"') do names[#names + 1] = name end
            parts[#parts + 1] = pair[1] .. ":" .. (enabled == "true" and "1" or "0") .. (whitelist == "true" and "1" or "0")
                .. ":" .. table.concat(names, ",")
        end
        entries[#entries + 1] = table.concat(parts, "|")
    end
    return table.concat(entries, ";")
end

local function message_of(entry)
    local id, sell, buy = string.match(entry, "^(%d+)|sell:([^|]*)|buy:(.*)$")
    local sets = {}
    for _, flags_and_factions in ipairs({ sell, buy }) do
        local flags, factions = string.match(flags_and_factions, "^(%d%d):(.*)$")
        local names = {}
        for name in string.gmatch(factions, "[^,]+") do names[#names + 1] = '"' .. name .. '"' end
        sets[#sets + 1] = '{"enabled":' .. (string.sub(flags, 1, 1) == "1" and "true" or "false")
            .. ',"whitelist":' .. (string.sub(flags, 2, 2) == "1" and "true" or "false")
            .. ',"factions":[' .. table.concat(names, ",") .. "]}"
    end
    return tonumber(id), '{"list":' .. id .. ',"sell":' .. sets[1] .. ',"buy":' .. sets[2] .. "}"
end

-- The DLL, as the editor reaches it: it keeps one entry per list the game holds, replaced by id, and
-- answers each entry and each request with the whole table, a JSON array of entries by id.
DLL = {}
local function dll_table()
    local ids = {}
    for id in pairs(DLL) do
        if LIVE[id] then ids[#ids + 1] = id else DLL[id] = nil end
    end
    table.sort(ids)
    local parts = {}
    for _, id in ipairs(ids) do parts[#parts + 1] = DLL[id] end
    return "[" .. table.concat(parts, ",") .. "]"
end
local function dll_report()
    local text = dll_table()
    SENT[#SENT + 1] = { "dll", "sets", text }
    if SUBS[EVENTS.sets] then SUBS[EVENTS.sets](nil, text) end
end
local function dll_receive(name, param)
    if name == EVENTS.set then
        local id = tonumber(string.match(param or "", '^{"list":(%d+),'))
        if id then
            if LIVE[id] then DLL[id] = param else DLL[id] = nil end
            dll_report()
        end
    elseif name == EVENTS.request then
        dll_report()
    end
end

-- The DLL reports `text`, in the shorthand, as the table it holds, as it does after a load.
function dll_reports(text)
    DLL = {}
    local parts = {}
    for entry in string.gmatch(text, "[^;]+") do
        local id, message = message_of(entry)
        DLL[id] = message
        parts[#parts + 1] = message
    end
    if SUBS[EVENTS.sets] then SUBS[EVENTS.sets](nil, "[" .. table.concat(parts, ",") .. "]") end
end
function ReadText(page, id)
    if id == 120 then return ":" end
    return "text" .. tostring(id)
end
-- The engine serves faction names through this Lua global; its FFI binding of the same name is
-- restricted for addon scripts, so only the global exists here.
function GetFactionData(id, what)
    if what == "name" then return id end
    return nil
end
-- X4Native's bridge is installed by its own script, after this one, exactly like Helper.
_G.__X4NATIVE_API = nil
RAISED = {}
Color = setmetatable({}, { __index = function() return { 1, 1, 1, 1 } end })

-- The UI is not there while the addon's script runs. It appears afterwards, as it does in the game.
Helper = nil
Menus = {}

local function cell(row_index, col)
    local w = { handlers = {}, properties = { x = 0, y = 0, width = 10, height = 10 }, row = row_index, col = col, kind = "cell" }
    function w.setColSpan(self, n) self.span = n return self end
    function w.createText(self, text) self.kind = "text"; self.text = text; DRAWN[#DRAWN + 1] = self; return self end
    function w.createCheckBox(self, value, options) self.kind = "checkbox"; self.value = value; self.options = options; DRAWN[#DRAWN + 1] = self; return self end
    function w.createButton(self) self.kind = "button"; DRAWN[#DRAWN + 1] = self; return self end
    function w.setText(self, text) self.text = text; return self end
    return w
end

local function new_row(cols)
    return setmetatable({}, {
        __index = function(t, i)
            if i > cols then return nil end
            local c = cell(ROW, i); rawset(t, i, c); return c
        end,
    })
end

local function new_grid(cols, properties)
    local g = { cols = cols, properties = properties or { x = 0, y = 0, width = 100, height = 100 } }
    function g.addRow(self) ROW = ROW + 1; return new_row(cols) end
    function g.setColWidth() end
    function g.setDefaultCellProperties() end
    function g.setDefaultComplexCellProperties() end
    function g.addConnection() end
    GRIDS[#GRIDS + 1] = g
    return g
end

local function new_frame(properties)
    local f = { properties = properties or {} }
    function f.addTable(self, cols, props) return new_grid(cols, props) end
    function f.setBackground() end
    function f.display() DRAWN.displayed = true end
    return f
end

-- The UI becomes available later, and every menu registers itself through Helper.registerMenu.
function provide_bridge()
    _G.__X4NATIVE_API = { raise_event = function(name, param)
        RAISED[#RAISED + 1] = { name, param }
        dll_receive(name, param)
    end }
end

function provide_ui()
    Helper = {
        viewWidth = 1600, viewHeight = 900,
        borderSize = 1,
        scaleX = function(v) return v end,
        scaleY = function(v) return v end,
        createFrameHandle = function(_, props) FRAMES = FRAMES + 1; FRAME_PROPS = props; return new_frame(props or {}) end,
        registerMenu = function(m) MENUS[m.name] = m end,
        removeAllWidgetScripts = function(_, layer) REMOVED_AT = layer end,
    }
end

-- The game's own page: the list on the left, the controls in the centre, the panels on the right.
-- It is called as `menu.createEmpire(frame, tableProperties)`.
function vanilla_create_empire(frame, properties)
    local left = frame:addTable(4, { x = properties.x, y = properties.y, width = 350 })
    local row = left:addRow(true)
    row[1]:createText("Self Trade")
    local centre = frame:addTable(5, { x = 400, y = properties.y, width = 600 })
    row = centre:addRow(true)
    row[1]:createText("Name:")
    row = centre:addRow(true)
    row[2]:setColSpan(4):createButton()
    frame:addTable(2, { x = 1000, y = properties.y, width = 300 })
    frame:addTable(1, { x = 1000, y = 400, width = 300 })
    return centre
end

function draw_page(menu)
    DRAWN = {}; ROW = 0; GRIDS = {}
    local frame = new_frame({})
    menu.createEmpire(frame, { x = 50, y = 100, width = 1500, height = 600 })
end

-- Reading helpers for the assertions.
function at(row, col)
    for _, w in ipairs(DRAWN) do
        if w.row == row and w.col == col then return w end
    end
    return nil
end
function row_of(text)
    for _, w in ipairs(DRAWN) do
        if w.kind == "text" and w.text == text then return w.row end
    end
    return nil
end
function count_text(text)
    local n = 0
    for _, w in ipairs(DRAWN) do if w.kind == "text" and w.text == text then n = n + 1 end end
    return n
end
function has_text_containing(part)
    for _, w in ipairs(DRAWN) do
        if w.kind == "text" and type(w.text) == "string" and w.text:find(part, 1, true) then return true end
    end
    return false
end
function count_labelled(text)
    local n = 0
    for _, w in ipairs(DRAWN) do if w.text == text then n = n + 1 end end
    return n
end
-- The table the DLL last reported, in the shorthand.
function last_sent()
    return SENT[#SENT] and shorthand(SENT[#SENT][3]) or ""
end
