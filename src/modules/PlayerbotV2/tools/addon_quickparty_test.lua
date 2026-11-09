-- ForeverBots Quick party sizes: lua5.1 src/modules/PlayerbotV2/tools/addon_quickparty_test.lua src/modules/PlayerbotV2/Addon/ForeverBots/ForeverBots.lua
-- Runs the add-on's Quick party block (QUICK_DPS up to the Tip helper) against stubbed client calls.
local file = io.open(arg[1]):read("*a")
local first = file:find("local QUICK_DPS", 1, true)
local last = file:find("local function Tip(b, title, body)", 1, true)
assert(first and last, "Quick party block not found")
local src = file:sub(first, last - 1) .. "\nreturn QuickParty"
local function run(members, inInstance, kind, maxPlayers)
  local added, printed = {}, nil
  local env = setmetatable({
    AddBotWithGear = function(c, r) added[#added+1] = r end,
    Print = function(m) printed = m end,
    GetNumGroupMembers = function() return members end,
    IsInInstance = function() return inInstance, kind end,
    GetInstanceInfo = function() return "x", kind, 0, "", maxPlayers end,
    UI = {},   -- the add-on's window-builder table (defined above the block); the dungeon list hangs off it
    L = setmetatable({}, { __index = function(_, k) return k end }),   -- English text, as the add-on's L with no locale
  }, { __index = _G })
  setfenv(loadstring(src), env)()()
  local t = { tank = 0, healer = 0, dps = 0 }
  for _, r in ipairs(added) do t[r] = t[r] + 1 end
  return #added, t, printed
end
local function check(n, want, ...) assert(n == want, ("got %d want %d"):format(n, want)) end
-- open world, alone: 4 bots, 1 tank 1 healer 2 dps
local n, t = run(0, false, "none", 0); check(n, 4); assert(t.tank == 1 and t.healer == 1 and t.dps == 2)
-- open world with 2 already: 3 more, never past 5
n = run(2, false, "none", 0); check(n, 3)
-- open world full or a leftover raid of 6: nothing
n, t, p = run(5, false, "none", 0); check(n, 0); assert(p)
n = run(6, false, "none", 0); check(n, 0)
-- 5-player dungeon
n = run(1, true, "party", 5); check(n, 4)
-- UBRS 10: 9 bots, 2 tanks 3 healers 4 dps
n, t = run(1, true, "raid", 10); check(n, 9); assert(t.tank == 2 and t.healer == 3 and t.dps == 4)
-- UBRS with 6 already (Karrz's case): 4 more
n = run(6, true, "raid", 10); check(n, 4)
-- ZG 20, MC 40, unknown raid size -> 40
n, t = run(1, true, "raid", 20); check(n, 19); assert(t.tank == 2 and t.healer == 5)
n, t = run(1, true, "raid", 40); check(n, 39); assert(t.tank == 4 and t.healer == 10)
n = run(1, true, "raid", 0); check(n, 39)
-- battleground: party size
n = run(1, true, "pvp", 15); check(n, 4)
print("quick party: all cases pass")
