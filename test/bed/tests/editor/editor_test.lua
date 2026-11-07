-- The editor's checks, run after the fake surface and the shipped scripts are loaded.

local failures = 0
function check(name, holds)
    if not holds then failures = failures + 1 end
    print(string.format("BED %-4s %s", holds and "PASS" or "FAIL", name))
end

-- The UI is not there while the mod's scripts load, so arming is what follows it.
check("the editor is armed by the mod's own signal", SUBS[EVENTS.arm] ~= nil)
provide_bridge()
provide_ui()
SUBS[EVENTS.arm](nil)

local playerinfo = {
    name = "PlayerInfoMenu",
    mode = "globalorders",
    shown = true,
    editedBlacklist = { id = 3, name = "Self Trade", type = "objectactivity", defaults = {}, factions = {} },
    empireData = { mode = { "empire_list", "blacklist", { id = 3, name = "Self Trade", type = "objectactivity" } } },
    createEmpire = function (frame, properties) return vanilla_create_empire(frame, properties) end,
    buttonBlacklistConfirm = function () end,
    buttonBlacklistCheckChanges = function () return false end,
    buttonBlacklistReset = function () end,
    buttonBlacklistRemove = function () end,
    -- The game's own faction panel: opening it records where it was placed, and closing it reports the
    -- whole selection at once.
    buttonBlacklistAddFaction = function (x, y) CONTEXT = { x = x, y = y }; CONTEXT_MODE = "blacklistfaction" end,
    buttonSelectContextSetList = function (data)
        VANILLA_SELECTED = data
        CONTEXT_MODE = nil  -- the game's handler ends by closing its panel
        -- The game's handler writes the page's own faction list, which must not become the mod's set.
        if playerinfo and playerinfo.editedBlacklist then playerinfo.editedBlacklist.factions = { "player" } end
    end,
    refreshInfoFrame = function () REFRESHED = REFRESHED + 1 end,
    -- The game's own close: an open panel closes alone (closeContextMenu), otherwise the page closes.
    onCloseElement = function ()
        if CONTEXT_MODE then CONTEXT_MODE = nil; CONTEXT = nil else PAGE_CLOSED = true end
    end,
}
-- The game's own remove: the list leaves the game.
playerinfo.buttonBlacklistRemove = function ()
    LIST_REMOVED = true
    if playerinfo.editedBlacklist and playerinfo.editedBlacklist.id then LIVE[playerinfo.editedBlacklist.id] = nil end
end
Helper.registerMenu(playerinfo)
check("the game's own page is taken over when the game registers it",
      playerinfo.__scoped_original ~= nil)

-- the mod's own bridge did not exist while the script loaded, and the set still reaches it.
check("the table is asked for once the UI is armed", #RAISED > 0 and RAISED[1][1] == EVENTS.request)
dll_reports("4|sell:11:|buy:00:")
ActionBasedTradeRestriction.store.send()
check("an entry reaches the mod after the late bridge appears",
      #RAISED == 2 and RAISED[2][1] == EVENTS.set and shorthand(RAISED[2][2]):find("sell:11:", 1, true) ~= nil)

-- the sets live in the page the game draws, and no window of the mod's own appears.
draw_page(playerinfo)
check("the page carries both restrictions by the names the editor uses",
      count_text("Separate sell restriction:") == 1 and count_text("Separate buy restriction:") == 1)
check("the restrictions sit in the page's own centre table", row_of("Separate sell restriction:") ~= nil
      and at(row_of("Separate sell restriction:"), 1) ~= nil)
check("nothing is drawn in a window of the mod's own", FRAMES == 0)

-- a list that carried no set shows both restrictions off.
local sell_row = row_of("Separate sell restriction:")
local buy_row = row_of("Separate buy restriction:")
check("a list with no set shows both restrictions off",
      at(sell_row, 5).value == false and at(buy_row, 5).value == false)

-- an off restriction shows its on/off box alone.
check("an off restriction offers no faction picker", count_labelled("Pick factions") == 0)

-- switching the selling restriction on reaches the store, and its controls appear.
at(sell_row, 5).handlers.onClick(at(sell_row, 5), true)
check("switching the selling restriction on reaches the store",
      last_sent():find("3|sell:11:", 1, true) == 1)
check("editing a restriction activates the page's Confirm Changes button",
      playerinfo.buttonBlacklistCheckChanges() == true)
draw_page(playerinfo)
sell_row = row_of("Separate sell restriction:")
check("a switch that is on shows its own box and its picker",
      at(sell_row, 5).value == true and count_labelled("Pick factions") == 1)

-- the allow/refuse switch is the selling set's own.
at(sell_row + 1, 5).handlers.onClick(at(sell_row + 1, 5), false)
check("the selling set's own switch reaches the store",
      last_sent():find("3|sell:10:", 1, true) == 1)

-- a change the player makes is what the reopened page shows.
draw_page(playerinfo)
sell_row = row_of("Separate sell restriction:")
check("reopening the list shows the change that was made",
      at(sell_row, 5).value == true and at(sell_row + 1, 5).value == false)

-- what the store holds is what the page shows.
dll_reports("3|sell:11:terran|buy:00:")
draw_page(playerinfo)
sell_row = row_of("Separate sell restriction:")
check("the page shows the sets the store holds",
      at(sell_row, 5).value == true and at(sell_row + 1, 5).value == true)
check("the page names the factions the set holds", has_text_containing("terran"))
check("the page says which way the set works", has_text_containing("only these pass"))

-- the picker is the game's own faction panel, opened beside the page, and the set
-- changes only when that panel reports its selection on close.
local pick = nil
for _, w in ipairs(DRAWN) do
    if w.kind == "button" and w.text == "Pick factions" then pick = w end
end
local before = last_sent()
pick.handlers.onClick(pick)
check("the picker is the game's own panel, opened beside the page",
      FRAMES == 0 and CONTEXT ~= nil and CONTEXT.x > 1000)
check("the panel is seeded from the set being edited",
      #playerinfo.editedBlacklist.factions == 1 and playerinfo.editedBlacklist.factions[1] == "terran")
check("opening the picker changes no set", last_sent() == before)

playerinfo.buttonSelectContextSetList({ terran = true, argon = true })
check("the game's own close handler still runs", VANILLA_SELECTED ~= nil)
check("the panel's selection does not become the page's own list",
      #playerinfo.editedBlacklist.factions == 0)
check("the panel's selection is written when it closes",
      last_sent():find("argon", 1, true) ~= nil)
draw_page(playerinfo)
check("the member list shows what the panel reported",
      row_of("    argon (argon)") ~= nil and row_of("    terran (terran)") ~= nil)

-- with the faction panel open, a close leaves the page open.
CONTEXT = nil
PAGE_CLOSED = false
local reopen = nil
for _, w in ipairs(DRAWN) do
    if w.kind == "button" and w.text == "Pick factions" then reopen = w end
end
reopen.handlers.onClick(reopen)
playerinfo.onCloseElement("close")
draw_page(playerinfo)
check("a close while the faction list is open leaves the page open", PAGE_CLOSED == false)

CONTEXT_MODE = nil  -- nothing open
playerinfo.onCloseElement("close")
check("a close with nothing open reaches the page", PAGE_CLOSED == true)

-- the sets appear with the fields a trade list owns, and only once that type is chosen. The
-- page is drawn from a working copy (`editedBlacklist`), and the stored entry (`mode[3]`) stays empty
-- until the game saves, so the working copy is what decides.
playerinfo.empireData.mode = { "empire_list", "blacklist", {} }
playerinfo.editedBlacklist = { type = "objectactivity", defaults = {}, factions = {} }
draw_page(playerinfo)
check("a list that has not been saved yet but has its type shows the settings",
      count_text("Separate sell restriction:") == 1 and count_text("Separate buy restriction:") == 1)

playerinfo.empireData.mode = { "empire_list", "blacklist", {} }
playerinfo.editedBlacklist = { defaults = {}, factions = {} }
draw_page(playerinfo)
check("a list whose type is not chosen yet shows no separate restrictions",
      count_text("Separate sell restriction:") == 0 and count_text("Separate buy restriction:") == 0)

-- The type the player picks lands in the working copy, so a change on a list that already exists is
-- judged by the copy and not by the type the list was saved with.
playerinfo.empireData.mode = { "empire_list", "blacklist", { id = 4, type = "sectortravel" } }
playerinfo.editedBlacklist = { id = 4, type = "objectactivity", defaults = {}, factions = {} }
draw_page(playerinfo)
check("a type changed to a trade list shows the settings",
      count_text("Separate sell restriction:") == 1 and count_text("Separate buy restriction:") == 1)

playerinfo.empireData.mode = { "empire_list", "blacklist", { id = 4, type = "objectactivity" } }
playerinfo.editedBlacklist = { id = 4, type = "sectortravel", defaults = {}, factions = {} }
draw_page(playerinfo)
check("a type changed away from a trade list hides the settings",
      count_text("Separate sell restriction:") == 0 and count_text("Separate buy restriction:") == 0)

-- the store cannot answer for a list that has no id, and must not erase what was set on it.
playerinfo.empireData.mode = { "empire_list", "blacklist", {} }
playerinfo.editedBlacklist = { type = "objectactivity", defaults = {}, factions = {} }
draw_page(playerinfo)
local unsaved_row = row_of("Separate sell restriction:")
at(unsaved_row, 5).handlers.onClick(at(unsaved_row, 5), true)
check("an unsaved list is not published to the store",
      (last_sent() or ""):find("0|sell", 1, true) == nil)
dll_reports("3|sell:11:terran|buy:00:")
draw_page(playerinfo)
unsaved_row = row_of("Separate sell restriction:")
check("a setting on an unsaved list survives the store's reply",
      at(unsaved_row, 5).value == true)

-- a travel list's page is the game's alone.
playerinfo.empireData.mode = { "empire_list", "blacklist", { id = 9, name = "Enemy sectors ban", type = "sectoractivity" } }
playerinfo.editedBlacklist = { id = 9, name = "Enemy sectors ban", type = "sectoractivity", defaults = {}, factions = {} }
draw_page(playerinfo)
check("a travel list's page carries no restriction",
      count_text("Separate sell restriction:") == 0 and count_text("Separate buy restriction:") == 0)

-- saving the list hands its set to the mod.
playerinfo.empireData.mode = { "empire_list", "blacklist", { id = 3, name = "Self Trade", type = "objectactivity" } }
playerinfo.editedBlacklist = { id = 3, name = "Self Trade", type = "objectactivity", defaults = {}, factions = {} }
RAISED = {}
playerinfo.buttonBlacklistConfirm()
local confirmed = {}
for _, raised in ipairs(RAISED) do
    if raised[1] == EVENTS.set and shorthand(raised[2]):find("3|sell:", 1, true) == 1 then confirmed[#confirmed + 1] = raised[2] end
end
check("saving the list hands its set to the mod", #confirmed == 1)

-- a deleted list's entry is dropped by the DLL, and the store follows its report, while live lists stay.
dll_reports("3|sell:11:terran|buy:00:;99|sell:10:argon|buy:00:")
ActionBasedTradeRestriction.store.send()
check("an entry for a deleted list is pruned out of the store",
      last_sent():find("3|sell:11:terran", 1, true) ~= nil and last_sent():find("99|sell", 1, true) == nil
      and ActionBasedTradeRestriction.store.sets[99] == nil)

-- What the DLL holds: every list's entry, the unsaved list never among them.
local function open_list(id)
    playerinfo.mode = "globalorders"
    playerinfo.empireData.mode = { "empire_list", "blacklist", id and { id = id, type = "objectactivity" } or {} }
    playerinfo.editedBlacklist = { id = id, type = "objectactivity", defaults = {}, factions = {} }
    draw_page(playerinfo)
end

dll_reports("4|sell:10:argon|buy:00:;3|sell:11:terran|buy:00:")
open_list(3)
local buy_switch = at(row_of("Separate buy restriction:"), 5)
RAISED = {}
buy_switch.handlers.onClick(buy_switch, true)
check("every list's entry reaches the DLL, which holds them all",
      last_sent() == "3|sell:11:terran|buy:10:;4|sell:10:argon|buy:00:")

local handed = {}
for _, raised in ipairs(RAISED) do
    if raised[1] == EVENTS.set then handed[#handed + 1] = shorthand(raised[2]) end
end
table.sort(handed)
check("the stored table reaches the mod one entry per list",
      #handed == 2 and handed[1] == "3|sell:11:terran|buy:10:" and handed[2] == "4|sell:10:argon|buy:00:")

-- A faction leaves its set through the x at the end of its row.
open_list(3)
local terran_row = row_of("    terran (terran)")
at(terran_row, 5).handlers.onClick(at(terran_row, 5))
check("a faction's x removes it from the set",
      last_sent() == "3|sell:11:|buy:10:;4|sell:10:argon|buy:00:")

-- Reset puts back what the list held when it was opened.
open_list(4)
local opened_with = "3|sell:11:|buy:10:;4|sell:10:argon|buy:00:"
local sell_switch = at(row_of("Separate sell restriction:"), 5)
sell_switch.handlers.onClick(sell_switch, false)
check("an edit changes what the store holds", last_sent() ~= opened_with)
playerinfo.buttonBlacklistReset()
check("Reset sends the sets the list was opened with", last_sent() == opened_with)
check("Reset leaves no change pending", playerinfo.buttonBlacklistCheckChanges() == false)

-- Removing a list drops its sets.
open_list(4)
playerinfo.buttonBlacklistRemove()
check("the game's own remove still runs", LIST_REMOVED == true)
check("removing a list drops its sets from the store", last_sent() == "3|sell:11:|buy:10:")

-- A new list's sets move to the id the game gives it on Confirm.
open_list(nil)
local new_sell = at(row_of("Separate sell restriction:"), 5)
new_sell.handlers.onClick(new_sell, true)
playerinfo.editedBlacklist.id = 1
playerinfo.empireData.mode[3] = { id = 1, type = "objectactivity" }
playerinfo.buttonBlacklistConfirm()
check("Confirm on a new list moves its sets to the list's id",
      last_sent() == "1|sell:11:|buy:01:;3|sell:11:|buy:10:")

-- The sets belong to a blacklist in the global orders page only.
open_list(3)
playerinfo.mode = "other"
draw_page(playerinfo)
check("a page outside the global orders shows no restriction",
      count_text("Separate sell restriction:") == 0)
playerinfo.mode = "globalorders"
playerinfo.empireData.mode = { "empire_list", "traderule", { id = 3, type = "objectactivity" } }
draw_page(playerinfo)
check("a trade rule's page shows no restriction", count_text("Separate sell restriction:") == 0)

-- The table the mod reports redraws a page that is shown.
local refreshed = REFRESHED
dll_reports("3|sell:11:|buy:10:")
check("the reported table refreshes a page that is shown", REFRESHED == refreshed + 1)

-- Cancel on the faction picker: the game's Cancel is a "back" close while its panel is open.
open_list(2)
playerinfo.empireData.mode = { "empire_list", "blacklist", { id = 3, type = "objectactivity" } }
playerinfo.editedBlacklist = { id = 3, type = "objectactivity", defaults = {}, factions = { "argon" } }
dll_reports("3|sell:11:terran|buy:00:")
draw_page(playerinfo)
local picker = nil
for _, w in ipairs(DRAWN) do
    if w.kind == "button" and w.text == "Pick factions" then picker = w end
end
picker.handlers.onClick(picker)
PAGE_CLOSED = false
playerinfo.onCloseElement("back")
check("one Cancel on the faction picker closes the game's panel", CONTEXT_MODE == nil and PAGE_CLOSED == false)
check("Cancel puts the page's own faction list back",
      #playerinfo.editedBlacklist.factions == 1 and playerinfo.editedBlacklist.factions[1] == "argon")

print(string.format("%d failure(s)", failures))
FAILURES = failures
