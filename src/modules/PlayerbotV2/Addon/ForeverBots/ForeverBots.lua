--[[
ForeverBots: buttons for the server's .bot commands (Classic 1.60 client).

Commands go through ordinary chat (".bot add priest healer", ...); group orders ("follow", ...) go as addon messages
under one prefix, with the party/raid chat line as the out-of-combat fallback. The server answers in the chat
frame as it does for typed commands.

/fb (or /foreverbots) toggles the window. /fb add <class> [tank|healer|dps] adds one bot without opening it.
]]

local ADDON = ...
ForeverBotsDB = ForeverBotsDB or {}

-- Client 70685's own bag code (Blizzard_UIPanels_Game ContainerFrame.lua) reads EMPTY_BAG_SLOT_EFFECT_COLOR, which no file of that
-- client defines: opening bags raised a Lua error and drew empty slots blank. Defining it stops the error, but the same code then
-- draws a grey bag icon in place of the slot frame of every empty slot. So the global is defined (a client that has it keeps its
-- own and nothing below runs), and after Blizzard's SyncAppearanceWithHasItem an empty slot gets back the slot frame a filled slot
-- shows (its NormalTexture). Post-hooks only (hooksecurefunc), and only textures are touched: no Blizzard function is replaced.
if EMPTY_BAG_SLOT_EFFECT_COLOR == nil and CreateColor then
    EMPTY_BAG_SLOT_EFFECT_COLOR = CreateColor(1, 1, 1, 1)
    local look                                          -- the slot frame of a filled slot, once one was seen
    local waiting = setmetatable({}, { __mode = "k" })  -- empty slots drawn before that
    local hooked = setmetatable({}, { __mode = "k" })
    local function HasItem(button)
        local bag = button.GetBagID and button:GetBagID()
        return bag ~= nil and C_Container and C_Container.GetContainerItemID(bag, button:GetID()) ~= nil
    end
    local function Restore(button)
        local t = button:GetNormalTexture()
        if not t then return end
        if look.atlas then t:SetAtlas(look.atlas) else t:SetTexture(look.file) end
        t:SetTexCoord(unpack(look.coords))
        t:SetVertexColor(unpack(look.color))
        t:SetDesaturated(look.desaturated)
        t:SetAlpha(look.alpha)
    end
    local function AfterSync(button)
        local t = button:GetNormalTexture()
        if not t then return end
        if HasItem(button) then
            if not look then
                look = { atlas = t:GetAtlas(), file = t:GetTexture(), coords = { t:GetTexCoord() }, color = { t:GetVertexColor() },
                         desaturated = t:IsDesaturated() and true or false, alpha = t:GetAlpha() }
                for b in pairs(waiting) do if not HasItem(b) then Restore(b) end end
                wipe(waiting)
            end
        elseif look then
            Restore(button)
        else
            waiting[button] = true
        end
    end
    local function Hook(button)
        if hooked[button] or type(button.SyncAppearanceWithHasItem) ~= "function" then return end
        hooked[button] = true
        hooksecurefunc(button, "SyncAppearanceWithHasItem", AfterSync)
    end
    -- buttons made from now on get it from the mixin; the ones that already exist are hooked one by one
    local mixinSync
    if ContainerFrameItemButtonMixin and type(ContainerFrameItemButtonMixin.SyncAppearanceWithHasItem) == "function" then
        hooksecurefunc(ContainerFrameItemButtonMixin, "SyncAppearanceWithHasItem", AfterSync)
        mixinSync = ContainerFrameItemButtonMixin.SyncAppearanceWithHasItem
    end
    -- EnumerateValidItems counts up to the bag size, not the buttons made: the combined-bags frame knows its size (all bags, 96)
    -- before it has made its buttons, so before it is first shown every index gives nil (Lua error at login, 2026-10-29). Buttons
    -- it makes later copy the hooked mixin function, so skipping the missing ones loses nothing.
    local function HookFrame(frame)
        if frame and frame.EnumerateValidItems then
            for _, button in frame:EnumerateValidItems() do
                if type(button) == "table" and button.SyncAppearanceWithHasItem ~= mixinSync then Hook(button) end
            end
        end
    end
    local bagHook = CreateFrame("Frame")
    bagHook:RegisterEvent("PLAYER_ENTERING_WORLD")
    bagHook:RegisterEvent("BAG_OPEN")
    bagHook:SetScript("OnEvent", function()
        HookFrame(ContainerFrameCombinedBags)
        for i = 1, (NUM_CONTAINER_FRAMES or 13) do HookFrame(_G["ContainerFrame" .. i]) end
    end)
end

-- Text the player reads goes through L: the table of the client's language (Locale_ruRU.lua, loaded first, fills
-- ForeverBotsLocale) or, for any key it does not hold, the English text itself. A key "short:<text>" is the shorter wording for a
-- narrow button. Words sent to the server (orders, .bot commands) and server replies matched below never go through L.
local L = setmetatable(ForeverBotsLocale or {}, { __index = function(_, k) return (k:gsub("^short:", "")) end })
-- On a client that is not English, spell, class and raid marker names come from the client itself, so they read exactly as in
-- the game; L holds them too for a name the client does not return.
local CLIENT_NAMES = not (GetLocale and (GetLocale() == "enUS" or GetLocale() == "enGB"))

-- Timing of every handler the add-on gives the client (frame scripts, timers, hooks, chat filters, menu callbacks): client 70630
-- shows "Script from ForeverBots has exceeded its execution time limit" with an empty stack (BRS 2026-10-27), which names no
-- function. Each handler is wrapped once, where it is handed over, and keeps its count, total and slowest call under the file line
-- that set it. A call over PERF_SLOW_MS prints one line per handler; "/fb perf" lists the slowest, and the list is saved in
-- ForeverBotsDB.perf at logout for the SavedVariables file. Secure frames are left alone: their scripts are not ours to wrap.
local PERF = { stats = {}, said = {} }
local PERF_SLOW_MS = 15
local clockMs = debugprofilestop
local function PerfLabel(kind, level)
    local where = debugstack and debugstack(level or 3, 1, 0)
    local line = type(where) == "string" and where:match(":(%d+):") or "?"
    return kind .. "@" .. line
end
local function PerfWrap(label, fn)
    if type(fn) ~= "function" or not clockMs then return fn end
    local st = PERF.stats[label]
    if not st then st = { n = 0, total = 0, max = 0 }; PERF.stats[label] = st end
    return function(...)
        local t0 = clockMs()
        local outer = PERF.running
        PERF.running = label
        local r1, r2, r3, r4, r5 = fn(...)
        PERF.running = outer
        local ms = clockMs() - t0
        st.n = st.n + 1
        st.total = st.total + ms
        if ms > st.max then st.max = ms end
        if ms > PERF_SLOW_MS and not PERF.said[label] and DEFAULT_CHAT_FRAME then
            PERF.said[label] = true
            DEFAULT_CHAT_FRAME:AddMessage(string.format("|cff66ccffWoW Forever Bots|r: slow handler %s took %.1f ms (/fb perf lists all).", label, ms))
        end
        return r1, r2, r3, r4, r5
    end
end
local function PerfFrame(f, tmpl)
    if type(f) ~= "table" or (type(tmpl) == "string" and tmpl:find("Secure", 1, true)) then return f end
    local set, hook = f.SetScript, f.HookScript
    f.SetScript = function(self, what, fn)
        return set(self, what, PerfWrap(PerfLabel(tostring(what)), fn))
    end
    f.HookScript = function(self, what, fn, ...)
        return hook(self, what, PerfWrap(PerfLabel("hook " .. tostring(what)), fn), ...)
    end
    return f
end
-- A handler the client stops for running too long never returns, so its timing above is never taken: the label it set stays in
-- PERF.running, and the next frame names it (also for the add-on's own file load, marked here and cleared at the end of the file).
PERF.running = "file load"
PERF.aborted = {}
do
    local watchdog = _G.CreateFrame("Frame")
    watchdog:SetScript("OnUpdate", function()
        local label = PERF.running
        if not label then return end
        PERF.running = nil
        PERF.aborted[label] = (PERF.aborted[label] or 0) + 1
        if PERF.aborted[label] == 1 and DEFAULT_CHAT_FRAME then
            DEFAULT_CHAT_FRAME:AddMessage("|cff66ccffWoW Forever Bots|r: stopped by the client (or a Lua error) inside " .. label .. " (/fb perf lists all).")
        end
    end)
end
local CreateFrame = function(kind, name, parent, tmpl, ...)
    return PerfFrame(_G.CreateFrame(kind, name, parent, tmpl, ...), tmpl)
end
local C_Timer = _G.C_Timer and {
    After = function(sec, fn) return _G.C_Timer.After(sec, PerfWrap(PerfLabel("timer"), fn)) end,
} or nil
local hooksecurefunc = _G.hooksecurefunc and function(a, b, c)
    if c ~= nil then return _G.hooksecurefunc(a, b, PerfWrap(PerfLabel("hook " .. tostring(b)), c)) end
    return _G.hooksecurefunc(a, PerfWrap(PerfLabel("hook " .. tostring(a)), b))
end or nil
local ChatFrame_AddMessageEventFilter = _G.ChatFrame_AddMessageEventFilter and function(ev, fn)
    return _G.ChatFrame_AddMessageEventFilter(ev, PerfWrap(PerfLabel("filter " .. tostring(ev)), fn))
end or nil

function PERF.Report(top)
    local rows = {}
    for label, st in pairs(PERF.stats) do
        if st.n > 0 then rows[#rows + 1] = { label = label, n = st.n, total = st.total, max = st.max } end
    end
    table.sort(rows, function(a, b) return a.max > b.max end)
    local lines = {}
    for label, n in pairs(PERF.aborted) do lines[#lines + 1] = "STOPPED BY CLIENT: " .. label .. " x" .. n end
    for i = 1, math.min(top or 12, #rows) do
        local r = rows[i]
        lines[#lines + 1] = string.format("%s  max %.1f ms  total %.0f ms  calls %d", r.label, r.max, r.total, r.n)
    end
    return lines
end

-- This client hands tainted addon code "secret" strings for some units (an enemy NPC in combat) and some chat. A secret
-- value cannot be compared, concatenated or matched, so every name that comes from the game goes through SafeName / SafeStr
-- first and a secret one is treated as "not a bot": nothing in the menu or panel path touches it.
local function IsSecret(v)
    return issecretvalue ~= nil and issecretvalue(v)
end
local function SafeStr(v)
    if type(v) ~= "string" or IsSecret(v) then return nil end
    return v
end
local function SafeName(unit)
    local ok, n = pcall(UnitName, unit)
    if not ok then return nil end
    return SafeStr(n)
end

local CLASSES = {
    { key = "warrior", label = "Warrior", token = "WARRIOR", roles = { "tank", "dps" } },
    { key = "paladin", label = "Paladin", token = "PALADIN", roles = { "tank", "healer", "dps" } },
    { key = "shaman",  label = "Shaman",  token = "SHAMAN",  roles = { "healer", "dps" } },
    { key = "hunter",  label = "Hunter",  token = "HUNTER",  roles = { "dps" } },
    { key = "rogue",   label = "Rogue",   token = "ROGUE",   roles = { "dps" } },
    { key = "priest",  label = "Priest",  token = "PRIEST",  roles = { "healer", "dps" } },
    { key = "mage",    label = "Mage",    token = "MAGE",    roles = { "dps" } },
    { key = "warlock", label = "Warlock", token = "WARLOCK", roles = { "dps" } },
    { key = "druid",   label = "Druid",   token = "DRUID",   roles = { "tank", "healer", "dps" } },
}
for _, c in ipairs(CLASSES) do
    local native = CLIENT_NAMES and LOCALIZED_CLASS_NAMES_MALE and LOCALIZED_CLASS_NAMES_MALE[c.token]
    c.label = native or L[c.label]
end

local ROLE_LABEL = { tank = L["Tank"], healer = L["Heal"], dps = L["DPS"] }

-- What the add-on was doing when the client blocked a call: set before the steps that touch the client's own frames.
local currentStep = ""
local function Step(name) currentStep = name or "" end

local function Print(msg)
    DEFAULT_CHAT_FRAME:AddMessage("|cff66ccffWoW Forever Bots|r: " .. msg)
end

-- Diagnostic output that nobody asked for (group roster, battleground hints) goes through Debug: silent unless "/fb debug on",
-- and never saved, so every login starts quiet.
local debugOn = false
local function Debug(msg)
    if debugOn then Print(msg) end
end

-- A dot command (".bot add mage") is run by the server from any chat line the player sends, before the line is delivered. It
-- never goes to /say: client 70707 blocks SendChatMessage to SAY from add-on code, also straight from a click ("Interface action
-- failed because of an AddOn" and "the client blocked SendChatMessage() for ForeverBots (blocked) during: no step marked" three
-- times when a bot was added from the /fb window, 2026-10-31). In a group it goes as an add-on message on the group channel
-- (Cmd.Queue below, the route the order buttons and Quick party already use); without a group, from a click, as a whisper to
-- yourself (Cmd.Say), which the client does not block. From a timer or an event it always goes through Cmd.Queue.
local helloDotSent = false
local inAuto = false
local Cmd = {}
-- Whispers to bots: the last one the panel or the menu sent per bot (Cmd.NoteOwnWhisper) and when the player last whispered a bot by
-- hand (CHAT_MSG_WHISPER_INFORM in the "filters" part). WhisperFilter hides "<Bot>: ..." confirmations of the add-on's own orders only.
Cmd.ownSent, Cmd.typedAt = {}, {}
function Cmd.NoteOwnWhisper(name, text)
    local who = type(name) == "string" and strmatch(name, "^[^-]+")
    if who and type(text) == "string" then Cmd.ownSent[who] = { text = text, at = GetTime and GetTime() or 0 } end
end
-- The other files of the add-on (ForeverBots_Net.lua, the windows) share the add-on table; kept in Cmd, not a new local.
Cmd.ns = select(2, ...) or {}
Cmd.ns.Print, Cmd.ns.SafeName, Cmd.ns.L = Print, SafeName, L
Cmd.ns.Queue = function(text) Cmd.Queue(text) end
local function ServerCommand(text)
    if inAuto then
        Cmd.Queue(text)
        return
    end
    if not helloDotSent then
        helloDotSent = true
        Cmd.Say(".bot addon")
    end
    Cmd.Say(text)
end

-- Stance button of the panel, three steps (Karrz 2026-11-03: "passive, defensive and aggressive"). ForeverBotsDB.stance is
--   "nofight"    - Passive: the bots attack nothing at all, not even to defend; heals and buffs go on (taming a beast, a sheep that
--                  must not break, a quest mob that must stay alive). Server word "passive".
--   "defensive"  - Defensive: they start nothing and fight what you pull or what attacks the group (the server's start stance).
--                  Older add-ons saved this one as "passive".
--   "aggressive" - Aggressive: they also attack anything hostile within about 10 yards of you.
-- nil = never picked: Defensive. The pick is kept between sessions and re-sent to the group whenever it changes (new bots).
-- (Kept on Cmd: the main chunk is at Lua 5.1's 200-local limit.)
Cmd.STANCES = { "defensive", "aggressive", "nofight" }   -- the order a click steps through
function Cmd.StanceKey()
    local st = ForeverBotsDB and ForeverBotsDB.stance
    if st == "aggressive" or st == "nofight" then return st end
    return "defensive"   -- nil, "defensive", and the older add-on's "passive"
end
-- The word a plain pick sends (it also answers in the bot whispers) and the stance-only re-send for new bots.
Cmd.STANCE_ORDER  = { defensive = "defensive", aggressive = "aggressive", nofight = "passive" }
Cmd.STANCE_RESEND = { defensive = "stance defensive", aggressive = "stance aggressive", nofight = "stance nofight" }
local function StanceLabel(short)
    local k = Cmd.StanceKey()
    if k == "aggressive" then return short and L["Aggr."] or L["Aggressive"] end
    if k == "nofight" then return L["Passive"] end
    return short and L["short:Defensive"] or L["Defensive"]
end
local stanceBtn
local function RefreshStance()
    if stanceBtn then
        stanceBtn:SetText(StanceLabel())
        -- tint: green Passive, blue Defensive, red Aggressive (MakeSkinButton rows 7, 5, 6)
        local k = Cmd.StanceKey()
        if stanceBtn.FBSetRow then stanceBtn:FBSetRow(k == "aggressive" and 6 or k == "nofight" and 7 or 5) end
    end
end
-- One step on (back with step -1), or straight to a stance key, and send it to the group. Cmd.GroupOrder is set below GroupOrder.
function Cmd.SetStance(step, key)
    local list, i = Cmd.STANCES, 1
    for n, v in ipairs(list) do if v == (key or Cmd.StanceKey()) then i = n end end
    if not key then i = (i - 1 + (step or 1)) % #list + 1 end
    ForeverBotsDB.stance = list[i]
    Cmd.GroupOrder(Cmd.STANCE_ORDER[list[i]])
    RefreshStance()
end

-- Bot orders go to the group as addon messages under a fixed prefix; the server reads them like party chat. SendChatMessage
-- from addon code is blocked by this client in combat and in battlegrounds (taint), addon messages are not. Out of combat a
-- refused or unavailable addon message falls back to the old party/raid chat line.
local ORDER_PREFIX = "FBOTS"
local prefixOk = false
do
    local reg = C_ChatInfo and C_ChatInfo.RegisterAddonMessagePrefix or RegisterAddonMessagePrefix
    if reg then
        local ok, res = pcall(reg, ORDER_PREFIX)
        prefixOk = ok and res ~= false
    end
end

-- Result codes of SendAddonMessage that mean "try again in a moment" (AddonMessageThrottle 3, ChannelThrottle 8): a burst of
-- clicks in a 40-man raid, or the other raid addons' traffic in a fight, fills the client's addon message budget, and an order
-- lost to it is no refusal of combat. The order is retried for up to five seconds before it is given up.
local THROTTLED = { [3] = true, [8] = true }
local ORDER_RETRIES = 6

-- Anything the add-on sends on its own (a timer, OnUpdate, an event) is not a player action: the client blocks chat sent from
-- there ("Interface action failed because of an AddOn", Emberseer 2026-10-20). Those paths run under Auto(), and GroupOrder then
-- only ever uses the addon message (never SendChatMessage, never the chat box) and drops the order when that fails.
local function Auto(fn, ...)
    local was, step = inAuto, currentStep
    inAuto = true
    Step("timer/event send")
    local ok, err = pcall(fn, ...)
    inAuto = was
    Step(step)
    if not ok then Debug("auto: " .. tostring(err)) end
end

-- Commands sent without a click, one at a time: the client's addon message budget (about ten at once, then one a second) would
-- drop most of a 40-player Quick party, so a refused send waits a second and tries the same line again, in order. Kept in one
-- table: the main chunk is near Lua 5.1's limit of 200 locals.
Cmd.queue, Cmd.pumping = {}, false
Cmd.GAP, Cmd.WAIT, Cmd.TRIES = 0.4, 1.0, 20
function Cmd.SendSelf(text)
    local send = C_ChatInfo and C_ChatInfo.SendAddonMessage or SendAddonMessage
    -- the group channel when grouped (ns.SelfChannel, ForeverBots_Net.lua: this client's add-on whispers never reach the server)
    local channel, me = "WHISPER", SafeName("player")
    if Cmd.ns.SelfChannel then channel, me = Cmd.ns.SelfChannel() end
    if not (prefixOk and send and (me or channel ~= "WHISPER")) then return false, nil end
    local ok, res = pcall(send, ORDER_PREFIX, text, channel, me)
    if ok and (res == nil or res == true or res == 0 or (Enum and Enum.SendAddonMessageResult and res == Enum.SendAddonMessageResult.Success)) then
        return true
    end
    return false, ok and tonumber(res) or nil
end
function Cmd.Pump()
    local head = Cmd.queue[1]
    if not head then Cmd.pumping = false return end
    local sent, code = Cmd.SendSelf(head.text)
    if not sent and code and THROTTLED[code] and head.tries < Cmd.TRIES and C_Timer and C_Timer.After then
        head.tries = head.tries + 1
        C_Timer.After(Cmd.WAIT, function() Auto(Cmd.Pump) end)
        return
    end
    table.remove(Cmd.queue, 1)
    if not sent then
        Print(L["could not send \"%s\"%s: type it in chat."]:format(head.text, code and (" (addon message result " .. code .. ")") or ""))
    end
    if Cmd.queue[1] and C_Timer and C_Timer.After then
        C_Timer.After(Cmd.GAP, function() Auto(Cmd.Pump) end)
    else
        Cmd.pumping = false
    end
end
function Cmd.Queue(text)
    Cmd.queue[#Cmd.queue + 1] = { text = text, tries = 0 }
    if not Cmd.pumping then
        Cmd.pumping = true
        Cmd.Pump()
    end
end

-- A dot command from a click (ServerCommand). Grouped: the add-on message on the group channel, no chat at all. Alone: a whisper
-- to yourself; the server runs the command and never delivers the whisper, so nothing shows in chat. A whisper the client
-- refuses raises ADDON_ACTION_BLOCKED during the step "dot command", and the guard below prints the line to type.
function Cmd.Say(text)
    if prefixOk and Cmd.ns.SelfChannel and Cmd.ns.SelfChannel() ~= "WHISPER" then
        Cmd.Queue(text)
        return
    end
    local me = SafeName("player")
    if not me then
        Print(L["could not send \"%s\"%s: type it in chat."]:format(text, ""))
        return
    end
    local step = currentStep
    Step("dot command")
    Cmd.lastSaid = text
    local ok, err = pcall(SendChatMessage, text, "WHISPER", nil, me)
    Step(step)
    if not ok then
        Print(L["could not send \"%s\"%s: type it in chat."]:format(text, " (" .. tostring(err) .. ")"))
    end
end

local function GroupOrder(text, attempt)
    attempt = attempt or 0
    local channel
    if IsInRaid and IsInRaid() then channel = "RAID"
    elseif (IsInGroup and IsInGroup()) or (GetNumPartyMembers and GetNumPartyMembers() > 0) then channel = "PARTY"
    else
        Print(L["you have no group yet: add a bot first."])
        return
    end
    local code, errText
    if prefixOk then
        local send = C_ChatInfo and C_ChatInfo.SendAddonMessage or SendAddonMessage
        local ok, res = pcall(send, ORDER_PREFIX, text, channel)
        -- Enum.SendAddonMessageResult.Success is 0; older clients return nothing, which counts as sent.
        local sent = ok and (res == nil or res == true or res == 0 or (Enum and Enum.SendAddonMessageResult and res == Enum.SendAddonMessageResult.Success))
        if sent then return end
        if ok then code = tonumber(res) or (res == false and -1 or nil) else errText = tostring(res) end
        if code and THROTTLED[code] and attempt < ORDER_RETRIES and C_Timer and C_Timer.After then
            C_Timer.After(0.8, function() Auto(GroupOrder, text, attempt + 1) end)
            return
        end
    end
    if inAuto or attempt > 0 then
        Debug("order not sent from a timer/event: " .. tostring(text) .. (code and (" (addon message result " .. code .. ")") or ""))
        return
    end
    if InCombatLockdown and InCombatLockdown() then
        -- The client lets the player's own Enter send chat in combat (typing "pull back" in raid chat works while the button does
        -- not), so the line is put in the chat box ready to send: an order is never lost.
        local why = code and (" (addon message result " .. code .. ")") or errText and (" (" .. errText .. ")") or (prefixOk and "" or " (addon prefix not registered)")
        -- Only straight from the click (attempt 0): from the C_Timer retry the client blocks chat-box access as "an action only
        -- available to the Blizzard UI", so a retry prints the hint instead.
        if attempt == 0 and ChatFrame_OpenChat then
            ChatFrame_OpenChat((channel == "RAID" and "/raid " or "/party ") .. text)
            Print(L["the order could not be sent in combat%s: press Enter to send it."]:format(why))
        else
            Print(L["the order could not be sent in combat%s: type it in %s chat."]:format(why, L[strlower(channel)]))
        end
        return
    end
    SendChatMessage(text, channel)
end
Cmd.GroupOrder = GroupOrder
Cmd.ns.GroupOrder = function(text) GroupOrder(text) end

-- Raid markers the tank panel cycles through (the chat order words), and the one picked now.
local TANK_MARKERS = { "skull", "cross", "star", "circle", "diamond", "triangle", "moon", "square" }
local tankMarker = 1

-- Follow distance (yards, 1..20): the stepper in the panel and /fb distance <n>. The value is saved in ForeverBotsDB.distance
-- and sent to the group as the order "distance <n>". Clicks are coalesced: the final value goes out after 0.4 s without a
-- click and never more than one order per 0.3 s. It is re-sent silently on login and when the group changes size.
local DIST_MIN, DIST_MAX, DIST_START = 2, 20, 5
local InGroupQuiet
local distText                 -- value font string, filled by BuildFrame
local distDirtyAt              -- GetTime() of the last unsent change
local distLastSend = 0
local distSilentUntil = 0      -- hides the server's "Follow distance N yards" whisper after a silent re-send
local lastGroupSize = 0

local function Now() return GetTime and GetTime() or 0 end

local function ShowDistance()
    if distText then
        distText:SetText(ForeverBotsDB.distance and L["%d yd"]:format(ForeverBotsDB.distance) or L["auto"])
    end
end

local function SendDistance(silent)
    local d = ForeverBotsDB.distance
    if not d then return end
    if not InGroupQuiet() then return end
    -- The silent re-send runs from a timer after a roster change, not from a click. Inside a battleground or arena the client
    -- blocks anything an addon sends on its own ("Interface action failed because of an AddOn", seen when bots join the
    -- group), and the bots there are not companions anyway.
    if silent then
        local okI, inInstance, kind = pcall(IsInInstance)
        if okI and inInstance and (kind == "pvp" or kind == "arena") then return end
    end
    if silent then distSilentUntil = Now() + 3 end
    distLastSend = Now()
    distDirtyAt = nil
    GroupOrder("distance " .. d)
end

local function SetDistance(d)
    d = math.floor(tonumber(d) or DIST_START)
    d = math.max(DIST_MIN, math.min(DIST_MAX, d))
    ForeverBotsDB.distance = d
    ShowDistance()
    distDirtyAt = Now()
end

local function StepDistance(delta)
    SetDistance((ForeverBotsDB.distance or DIST_START) + delta)
end

-- Deletes the targeted companion for good, after a confirmation.
StaticPopupDialogs["FOREVERBOTS_DELETE"] = {
    text = L["Delete %s for good? Its character is removed. (Your own characters summoned as bots are never deleted: use Dismiss for them.)"],
    button1 = YES,
    button2 = NO,
    OnAccept = function(_, name) ServerCommand(".bot delete " .. name) end,
    timeout = 0,
    whileDead = true,
    hideOnEscape = true,
}

local function DeleteTarget()
    local name = UnitExists("target") and UnitIsPlayer("target") and SafeName("target")
    if not name or UnitIsUnit("target", "player") then
        Print(L["target the bot you want to delete first."])
        return
    end
    local dialog = StaticPopup_Show("FOREVERBOTS_DELETE", name)
    if dialog then dialog.data = name end
end

local function AddBot(class, role)
    if role then
        ServerCommand(".bot add " .. class .. " " .. role)
    else
        ServerCommand(".bot add " .. class)
    end
end

------------------------------------------------------------------------------------------------------------- assign
-- The client cannot put a raid icon on a friendly bot, so the panel assigns bots to icons through chat orders
-- ("assign <name> <marker> [tank|cc|kill]", "assign clear [marker|name]", "assign list"). The server keeps the
-- assignments; "assign list" makes ONE bot whisper "ASSIGN|<marker 0..7>|<name>|<role>" lines and a closing
-- "ASSIGN|END|<count>". Those whispers are parsed here and hidden from the chat frame.

local ICON_WORDS = { "star", "circle", "diamond", "triangle", "moon", "square", "cross", "skull" }   -- icon N = word N
-- What the player reads for a marker word: the client's own raid marker name (RAID_TARGET_N) or L. Markers are the words.
local MARKER_INDEX = {}
for i, w in ipairs(ICON_WORDS) do MARKER_INDEX[w] = i end
local function MarkerName(word)
    local i = MARKER_INDEX[word]
    local native = CLIENT_NAMES and i and _G["RAID_TARGET_" .. i]
    if type(native) == "string" and native ~= "" then return native end
    return L[word]
end
local ICON_TEX = "Interface\\TargetingFrame\\UI-RaidTargetingIcon_"
local ROLE_CYCLE = { "auto", "tank", "cc", "kill" }
local roleChoice = 1
local assignments, pending = {}, {}
local printNextList = false
local listLines = {}          -- font strings of the panel list, filled by BuildFrame
local statusFS                -- one-line confirmation under the icon row, filled by BuildFrame
local lastAssign              -- { name, marker (1..8), role, at } of the click still waiting for the server

local function SetStatus(text)
    if statusFS then statusFS:SetText(text or "") end
end
local MAX_LIST = 5

InGroupQuiet = function()
    return (IsInRaid and IsInRaid()) or (IsInGroup and IsInGroup()) or (GetNumPartyMembers and GetNumPartyMembers() > 0)
end

local function RefreshList()
    for i, fs in ipairs(listLines) do
        local a = assignments[i]
        if a and i <= MAX_LIST then
            if i == MAX_LIST and #assignments > MAX_LIST then
                fs:SetText(L["+%d more (/fb assign list)"]:format(#assignments - MAX_LIST + 1))
            else
                fs:SetText("|T" .. ICON_TEX .. (a.marker + 1) .. ":14|t  " .. a.name .. "  |cff9d9d9d" .. L[a.role] .. "|r")
            end
        else
            fs:SetText(i == 1 and ("|cff9d9d9d" .. L["No assignments."] .. "|r") or "")
        end
    end
end

-- Asks the server for the current assignments (one bot answers); the reply is parsed in the whisper handler.
-- The list request is a whisper to ONE group member (a bot answers a whisper by itself), never group chat: in raid or party chat
-- it reached all 40 bots, filled the server log and showed up in the raid chat.
local function ListAsker()
    local prefix, n = "party", (GetNumSubgroupMembers and GetNumSubgroupMembers()) or 0
    if IsInRaid and IsInRaid() then prefix, n = "raid", (GetNumGroupMembers and GetNumGroupMembers()) or 0 end
    for i = 1, n do
        local unit = prefix .. i
        if UnitExists(unit) and UnitIsPlayer(unit) and not UnitIsUnit(unit, "player") then
            local name = SafeName(unit)
            -- Names are not loaded yet right after a roster change: UnitName answers "Unknown" and the whisper would go nowhere.
            if name and name ~= UNKNOWNOBJECT and name ~= "Unknown" then return name end
        end
    end
    return nil
end

local function RequestList()
    -- no bot list inside a battleground/arena: bots there are not companions, and an addon-sent whisper is blocked in them
    -- (it was the "Interface action failed because of an AddOn" seen when bots join the instance group)
    local okI, inInstance, kind = pcall(IsInInstance)
    if okI and inInstance and (kind == "pvp" or kind == "arena") then return end
    -- Not in a fight: the client refuses addon sends there, and a whispered answer arrives as a secret string that no chat filter
    -- can read, so "ASSIGN|END|0" showed in chat (test4 tank run, 2026-10-30). Asked again when the fight ends.
    if InCombatLockdown and InCombatLockdown() then Cmd.listAfterCombat = true; return end
    if InGroupQuiet() then
        -- The hidden request first (add-on whisper or hidden system line, whichever this client takes): nothing reaches chat.
        local Net = Cmd.ns.Net
        if Net and (Net.mode == "addon" or Net.mode == "sys") then
            Net.Request("ASSIGN_REQ", {}, function(fields)
                if not fields then return end   -- an older server: the next roster change asks again
                local list = {}
                for i = 2, #fields do
                    local marker, name, role = strmatch(fields[i], "^(%d),([^,]+),(%a+)$")
                    if marker then list[#list + 1] = { marker = tonumber(marker), name = name, role = role } end
                end
                Cmd.ApplyAssignments(list)
            end)
            return
        end
        -- The hidden route is not known for this group yet. The login ping goes out before the first bot joins, as a whisper to
        -- yourself, and clients from 70658 on never pass that to the server, so it ended "none" and the list fell through to the
        -- whispered "assign list" below: on 70707 the bot's answer is a secret string no chat filter can read, and "ASSIGN|END|0"
        -- showed in chat each time a bot joined (test item 1, 2026-10-31). Now the ping goes on the group channel; ask again.
        if Net and Net.EnsureMode and not Net.ModeKnown() then
            Net.EnsureMode(function() RequestList() end)
            return
        end
        -- The whispered request is left for a client whose chat filter can read the answer (no secret strings): on one that has
        -- them the answer lines would show in chat, so without the hidden route the list is not asked for.
        if issecretvalue ~= nil then return end
        local asker = ListAsker()
        if asker and prefixOk then
            -- from a timer: an addon whisper (the server reads the FBOTS prefix like a whisper), never SendChatMessage
            local send = C_ChatInfo and C_ChatInfo.SendAddonMessage or SendAddonMessage
            pcall(send, ORDER_PREFIX, "assign list", "WHISPER", asker)
        end
    else
        assignments = {}
        RefreshList()
    end
end

local function RequestListSoon()
    if C_Timer and C_Timer.After then C_Timer.After(0.5, RequestList) else RequestList() end
end
do
    local f = CreateFrame("Frame")
    f:RegisterEvent("PLAYER_REGEN_ENABLED")
    f:SetScript("OnEvent", function()
        if Cmd.listAfterCombat then Cmd.listAfterCombat = false; RequestListSoon() end
    end)
end

local function AssignTarget(marker, button)   -- marker 1..8; right-click clears the icon
    if not (UnitExists("target") and UnitIsPlayer("target") and not UnitIsUnit("target", "player")
            and UnitIsFriend("player", "target")) then
        Print(L["target one of your bots first, then left-click an icon."]); SetStatus("|cffff4040" .. L["Target a bot first."] .. "|r")
        return
    end
    local name = SafeName("target")
    if not name then SetStatus("|cffff4040" .. L["Target a bot first."] .. "|r"); return end
    if button == "RightButton" then
        lastAssign = nil
        GroupOrder("assign clear " .. ICON_WORDS[marker])
        SetStatus("|cffffff00" .. L["Cleared %s, asking the server..."]:format(MarkerName(ICON_WORDS[marker])) .. "|r")
    else
        local role = ROLE_CYCLE[roleChoice]
        GroupOrder("assign " .. strlower(name) .. " " .. ICON_WORDS[marker] .. (role ~= "auto" and (" " .. role) or ""))
        lastAssign = { name = name, marker = marker, role = role, at = GetTime and GetTime() or 0 }
        SetStatus("|cffffff00" .. L["Sent: %s on %s, waiting for the server..."]:format(name, MarkerName(ICON_WORDS[marker])) .. "|r")
        Print(L["sent: %s on %s%s, waiting for the server to confirm."]:format(name, MarkerName(ICON_WORDS[marker]),
            role ~= "auto" and (" (" .. L[role] .. ")") or ""))
        roleChoice = 1
    end
    RequestListSoon()
end

local function ParseAssignLine(msg)
    local marker, name, role = strmatch(msg, "^ASSIGN|(%d)|([^|]+)|(%a+)$")
    if marker then
        pending[#pending + 1] = { marker = tonumber(marker), name = name, role = role }
        return
    end
    if strmatch(msg, "^ASSIGN|END|") then
        Cmd.ApplyAssignments(pending)
        pending = {}
    end
end

-- The finished list, from the hidden ASSIGN_REQ answer or the whispered "ASSIGN|...|END" lines.
-- (Cmd fields, not locals: this file's main chunk is at Lua's 200-local limit.)
function Cmd.ApplyAssignments(list)
    assignments = list
    RefreshList()
    if lastAssign then
        local found
        for _, a in ipairs(assignments) do
            if a.marker + 1 == lastAssign.marker and strlower(a.name) == strlower(lastAssign.name) then found = a end
        end
        if found then
            SetStatus("|cff00ff00" .. L["Confirmed: %s: %s on %s"]:format(found.name, L[found.role], MarkerName(ICON_WORDS[found.marker + 1])) .. "|r")
            Print(L["confirmed: %s: %s on %s"]:format(found.name, L[found.role], MarkerName(ICON_WORDS[found.marker + 1])))
            lastAssign = nil
        elseif (GetTime and GetTime() or 0) - lastAssign.at > 4 then
            SetStatus("|cffff4040" .. L["The server did not confirm %s on %s."]:format(lastAssign.name, MarkerName(ICON_WORDS[lastAssign.marker])) .. "|r")
            Print(L["the server did not confirm %s on %s (is it your bot, in your group?)."]:format(lastAssign.name, MarkerName(ICON_WORDS[lastAssign.marker])))
            lastAssign = nil
        end
    end
    if printNextList then
        printNextList = false
        if #assignments == 0 then Print(L["no assignments."]) end
        for _, a in ipairs(assignments) do
            Print(MarkerName(ICON_WORDS[a.marker + 1]) .. ": " .. a.name .. " (" .. L[a.role] .. ")")
        end
    end
end

local function IsAssignMsg(msg) return type(msg) == "string" and not IsSecret(msg) and strsub(msg, 1, 7) == "ASSIGN|" end

-- A per-bot confirmation whisper has the form "<BotName>: <what it did>" (server: BotCommandParser ConfirmWhisperOrder /
-- OrderAssign / OrderDistance). The bot panel below parses it into its status line and the chat frame does not show it.
local function ConfirmText(msg, author)
    if type(msg) ~= "string" or type(author) ~= "string" or IsSecret(msg) or IsSecret(author) then return nil end
    local who = strmatch(author, "^[^-]+")
    if not who then return nil end
    -- The answer to a whisper the player typed himself ("gear", "equip upgrades", "stats") is for him to read: only the
    -- confirmations of the panel's and the menu's own orders are hidden (Gremloc 11-08: "gear" seemed to answer nothing).
    local asked = Cmd.typedAt and Cmd.typedAt[who]
    if asked and Now() - asked < 20 then return nil end
    return strmatch(msg, "^" .. who .. ": (.+)$")
end

-- Hide the structured whispers and our own periodic "assign list" request from the chat frames.
local function WhisperFilter(_, _, msg, author)
    if IsAssignMsg(msg) then return true end
    -- /fb chat on: show the bot confirmations ("<Bot>: followed you") and distance replies in chat as well
    if ForeverBotsDB.showChat then return false end
    if ConfirmText(msg, author) then return true end
    if Now() >= distSilentUntil or type(msg) ~= "string" then return false end
    -- the server answers in the player's language: English, or the Russian line from Util/BotLocaleRu.cpp (written as bytes)
    return strmatch(msg, "^Follow distance %d+ yards$") ~= nil or strmatch(msg, "^\208\148\208\184\209\129\209\130\208\176\208\189\209\134\208\184\209\143 \209\129\208\187\208\181\208\180\208\190\208\178\208\176\208\189\208\184\209\143: %d+ \209\143\209\128\208\180%.$") ~= nil
end
local function OwnListFilter(_, _, msg, author)
    if msg ~= "assign list" and not (Now() < distSilentUntil and type(msg) == "string" and strmatch(msg, "^distance %d+$")) then
        return false
    end
    local me = SafeName("player")
    return me ~= nil and author ~= nil and strmatch(author, "^[^-]+") == me
end
-- Optional parts that run inside the client's own code (chat filters, the SetBinding hook, the issue reporter search): each can be
-- switched off with "/fb part <name> off" and /reload, to find which one the client's time limit is about. They start once the
-- saved settings are loaded (ADDON_LOADED of this add-on).
local PARTS = { list = {}, start = {} }
function PARTS.On(name) return not (ForeverBotsDB.partsOff and ForeverBotsDB.partsOff[name]) end
PARTS.list[1] = "menu"   -- the portrait menu entries (AddPopupEntry at login); listed here so /fb part knows the name
local function AddPart(name, fn) PARTS.list[#PARTS.list + 1] = name; PARTS.start[name] = fn end
do
    local starter = CreateFrame("Frame")
    starter:RegisterEvent("ADDON_LOADED")
    starter:RegisterEvent("PLAYER_LOGOUT")
    starter:SetScript("OnEvent", function(self, event, name)
        if event == "PLAYER_LOGOUT" then
            ForeverBotsDB.perf = PERF.Report(25)
            return
        end
        if name ~= ADDON then return end
        self:UnregisterEvent("ADDON_LOADED")
        for _, part in ipairs(PARTS.list) do
            if PARTS.On(part) then
                local ok, err = pcall(PARTS.start[part])
                if not ok then Debug("part " .. part .. ": " .. tostring(err)) end
            end
        end
    end)
end

AddPart("filters", function()
    if not ChatFrame_AddMessageEventFilter then return end
    ChatFrame_AddMessageEventFilter("CHAT_MSG_WHISPER", WhisperFilter)
    ChatFrame_AddMessageEventFilter("CHAT_MSG_WHISPER_INFORM", function(_, _, msg) return type(msg) == "string" and not IsSecret(msg) and msg == "assign list" end)
    -- A whisper the player sent by hand (his own /w or the chat box, not the panel or the menu): the bot's answer stays visible.
    local typed = CreateFrame("Frame")
    typed:RegisterEvent("CHAT_MSG_WHISPER_INFORM")
    typed:SetScript("OnEvent", function(_, _, msg, target)
        if type(msg) ~= "string" or type(target) ~= "string" or IsSecret(msg) or IsSecret(target) or msg == "assign list" then return end
        local who = strmatch(target, "^[^-]+")
        if not who then return end
        local own = Cmd.ownSent[who]
        if own and own.text == msg and Now() - own.at < 5 then
            Cmd.ownSent[who] = nil
            return
        end
        Cmd.typedAt[who] = Now()
    end)
    for _, ev in ipairs({ "CHAT_MSG_PARTY", "CHAT_MSG_PARTY_LEADER", "CHAT_MSG_RAID", "CHAT_MSG_RAID_LEADER" }) do
        ChatFrame_AddMessageEventFilter(ev, OwnListFilter)
    end
end)

local whisperFrame = CreateFrame("Frame")
whisperFrame:RegisterEvent("CHAT_MSG_WHISPER")
whisperFrame:SetScript("OnEvent", function(_, _, msg)
    if IsSecret(msg) then
        if lastAssign then SetStatus("|cffff8000" .. L["The server answered but the client hid the reply (restricted chat)."] .. "|r") end
        return
    end
    if IsAssignMsg(msg) then ParseAssignLine(msg) end
end)


------------------------------------------------------------------------------------------------------------- window

local frame
local UI = {}   -- window builders as table functions: BuildFrame alone captured over 60 file-level locals, a Lua 5.1 compile error

local function MakeButton(parent, text, width, onClick, height)
    local b = CreateFrame("Button", nil, parent, "UIPanelButtonTemplate")
    b:SetSize(width, height or 22)
    b:SetText(text)
    b:SetScript("OnClick", onClick)
    return b
end

-- Fixed group orders (Follow, Pull back, Fall back, ...) are secure macro buttons that run "/raid <order>" or "/party <order>":
-- the client keeps addon code from sending anything itself during a boss fight (SendAddonMessage is refused and the chat call
-- raises "Interface action failed because of an AddOn", live MC Baron Geddon 2026-10-12), but a click on a secure button runs
-- the player's own chat command, in combat too. The server reads that line like typed chat. The macro text names the channel and
-- is set out of combat (a secure attribute cannot change in a fight): on every group change and when combat ends.
local secureOrders = {}
local secureDirty = false

local function OrderChannelWord()
    return (IsInRaid and IsInRaid()) and "/raid " or "/party "
end

local function RefreshOrderMacros()
    if InCombatLockdown and InCombatLockdown() then
        secureDirty = true
        return
    end
    secureDirty = false
    Step("secure order buttons")
    local word = OrderChannelWord()
    for _, b in ipairs(secureOrders) do
        b:SetAttribute("type", "macro")
        b:SetAttribute("macrotext", (b.macroPre or "") .. word .. b.orderText)
    end
    Step("")
end

local function MakeSecureOrderButton(parent, text, width, order, height)
    local b = CreateFrame("Button", nil, parent, "UIPanelButtonTemplate,SecureActionButtonTemplate")
    b:SetSize(width, height or 22)
    b:SetText(text)
    b:RegisterForClicks("AnyUp", "AnyDown")
    b.orderText = order
    secureOrders[#secureOrders + 1] = b
    RefreshOrderMacros()
    return b
end

local orderMacroFrame = CreateFrame("Frame")
for _, ev in ipairs({ "GROUP_ROSTER_UPDATE", "PARTY_MEMBERS_CHANGED", "RAID_ROSTER_UPDATE", "PLAYER_REGEN_ENABLED", "PLAYER_ENTERING_WORLD" }) do
    pcall(orderMacroFrame.RegisterEvent, orderMacroFrame, ev)
end
orderMacroFrame:SetScript("OnEvent", function() RefreshOrderMacros() end)

------------------------------------------------------------------------------------------------------------- key bindings
-- Esc > Options > Key Bindings > AddOns > "WoW Forever Bots" (Bindings.xml; unbound until the player picks keys). Order keys
-- click hidden secure buttons that run the same "/party <order>" or "/raid <order>" line as the window buttons, so they work in
-- combat. Nothing here touches the chat edit box or runs from a timer.
local KEY_ORDERS = {
    { "Attack", "attack", L["Attack (your target)"] },
    { "Pull", "pull", L["Pull"] },
    { "PullBack", "pull back", L["Pull back"] },
    { "Follow", "follow", L["Follow"] },
    { "Stay", "stay", L["Stay"] },
    { "Resume", "resume", L["Resume"] },
    { "Stop", "disengage", L["Stop attacking"] },
    { "Flee", "flee", L["Flee"] },
    { "Come", "come", L["Come"] },
    { "FallBack", "fallback", L["Fall back"] },
    { "AttackMouseover", "attack", L["Attack (mouseover)"], "/target [@mouseover,harm,nodead]\n" },
    -- The three stances, one key each (a key works in combat too; the panel's stance button follows it).
    { "Passive", "passive", L["Stance: Passive (attack nothing)"], nil, "nofight" },
    { "Defensive", "defensive", L["Stance: Defensive"], nil, "defensive" },
    { "Aggressive", "aggressive", L["Stance: Aggressive"], nil, "aggressive" },
    -- Your target goes on (or, pressed again on it, off) the list of mobs your bots never attack; the second key empties the list.
    { "IgnoreTarget", "ignore target", L["Ignore my target (again: undo)"] },
    { "IgnoreClear", "ignore clear", L["Clear ignored mobs"] },
}
BINDING_HEADER_FOREVERBOTS = "WoW Forever Bots"
BINDING_NAME_FOREVERBOTS_TOGGLE_WINDOW = L["Show / hide the bot window"]
BINDING_NAME_FOREVERBOTS_TOGGLE_STANCE = L["Next stance (Defensive / Aggressive / Passive)"]
for _, k in ipairs(KEY_ORDERS) do
    local name = "ForeverBotsKey" .. k[1]
    local b = CreateFrame("Button", name, UIParent, "SecureActionButtonTemplate")
    b:RegisterForClicks("AnyUp", "AnyDown")
    b.orderText = k[2]
    b.macroPre = k[4]   -- optional macro lines before the order (Attack mouseover targets the unit under the cursor first)
    if k[5] then
        -- A stance key: the panel shows it and re-sends it to bots that join (saved like a pick on the button). Plain Lua after the
        -- secure click, nothing protected, so it runs in combat too.
        local key = k[5]
        b:HookScript("OnClick", function()
            if ForeverBotsDB then ForeverBotsDB.stance = key end
            RefreshStance()
        end)
    end
    secureOrders[#secureOrders + 1] = b
    _G["BINDING_NAME_CLICK " .. name .. ":LeftButton"] = k[3]
end
RefreshOrderMacros()

-- A secure window key: it shows or hides the window in combat too (the plain toggle key and /fb wait for the end of combat). Its
-- reference to the window is set when the window is built, at login.
do
    local ok, b = pcall(CreateFrame, "Button", "ForeverBotsKeyWindow", UIParent, "SecureHandlerClickTemplate")
    if ok and b and b.SetFrameRef then
        b:RegisterForClicks("AnyUp")
        b:SetAttribute("_onclick", [[
            local w = self:GetFrameRef("fbwin")
            if w then
                if w:IsShown() then w:Hide() else w:Show() end
            end
        ]])
        UI.keyWindow = b
        _G["BINDING_NAME_CLICK ForeverBotsKeyWindow:LeftButton"] = L["Show / hide the bot window (works in combat)"]
    end
end

-- Lazy lock (warlock): one key, the server picks the next spell (dots, curse, Shadow Bolt, cooldowns) and casts it on your target.
-- The client will not let an add-on choose spells in a fight, so the key clicks a secure button that runs the server command
-- ".playerbot lazy" as the player's own line, in combat too. Press it again for every cast.
do
    local b = CreateFrame("Button", "ForeverBotsKeyLazy", UIParent, "SecureActionButtonTemplate")
    b:RegisterForClicks("AnyUp", "AnyDown")
    b:SetAttribute("type", "macro")
    b:SetAttribute("macrotext", ".playerbot lazy")
    _G["BINDING_NAME_CLICK ForeverBotsKeyLazy:LeftButton"] = L["Lazy lock (warlock: next spell on your target)"]
end

-- Duplicate macros: a key bound to a macro ("bind ALT-E MACRO Corruption" in bindings-cache.wtf) runs the first macro of that
-- name. The client sorts the macro list by name, and copies with the same name come out in a different order at each login,
-- so it renumbers them and saves the list again at logout (live: Karrz's 59 account macros held 9 names two or three times,
-- and their numbers swapped between the 14:59 and 19:13 saves of 2026-10-24). Once one copy is edited, the key runs the old
-- copy after the next logout. Out of combat at login, copies that match an earlier macro in name, icon and text are deleted;
-- copies that differ are kept and named in chat so the player can rename or delete one.
local function RemoveDuplicateMacros()
    if not (GetNumMacros and GetMacroInfo and DeleteMacro) then return end
    if InCombatLockdown and InCombatLockdown() then return end
    Step("duplicate macros")
    local numAccount, numChar = GetNumMacros()
    local charFirst = (MAX_ACCOUNT_MACROS or 120) + 1
    local function Scan(first, count)
        local seen, drop, differ = {}, {}, {}
        for i = first, first + count - 1 do
            local name, icon, body = GetMacroInfo(i)
            if name then
                local kept = seen[name]
                if not kept then
                    seen[name] = { icon = icon, body = body }
                elseif kept.icon == icon and kept.body == body then
                    drop[#drop + 1] = i
                else
                    differ[name] = true
                end
            end
        end
        for k = #drop, 1, -1 do DeleteMacro(drop[k]) end   -- highest first: a delete moves every later macro down by one
        return #drop, differ
    end
    local droppedA, differA = Scan(1, numAccount or 0)
    local droppedC, differC = Scan(charFirst, numChar or 0)
    if droppedA + droppedC > 0 then
        Print(L["removed %d duplicate macro(s) (same name and text as another), so macro keys stay on the same macro."]:format(droppedA + droppedC))
    end
    for _, differ in ipairs({ differA, differC }) do
        for name in pairs(differ) do
            Print(L["two macros are named %s but differ; a key bound to that name uses the first. Rename or delete one in /macro."]:format("|cffffd100" .. name .. "|r"))
        end
    end
    Step("")
end

local macroFrame = CreateFrame("Frame")
macroFrame:RegisterEvent("PLAYER_ENTERING_WORLD")
macroFrame:SetScript("OnEvent", function(self, _, isLogin, isReload)
    if isLogin == false and isReload == false then return end   -- zoning: the list has not changed
    self:UnregisterEvent("PLAYER_ENTERING_WORLD")
    -- the client fills the macro list during the login; a second later it is complete
    if C_Timer and C_Timer.After then C_Timer.After(1, RemoveDuplicateMacros) else RemoveDuplicateMacros() end
end)

------------------------------------------------------------------------------------------------------------- skin
-- Own artwork in Textures\ (32-bit .tga, power-of-two sizes): wood body, bronze bead border, medallion, tabs. The buttons are the
-- client's own red buttons (UIPanelButtonTemplate), so they look and behave like every other button in the game.
local TEXDIR = "Interface\\AddOns\\" .. tostring(ADDON) .. "\\Textures\\"
local FONT_LABEL = "Fonts\\FRIZQT__.TTF"
local ROW_NORMAL, ROW_GOLD, ROW_BLUE, ROW_RED = 0, 4, 5, 6   -- resting look, chosen (kept highlighted), blue text, red text
-- row 7: green text (the stance button's Passive)

-- o.order: a secure order button (runs "/party <order>" or "/raid <order>", also in combat). Same template and font as the
-- "Raid Info" button of the client's Raid window.
local function MakeSkinButton(parent, text, w, h, o)
    o = o or {}
    local b = CreateFrame("Button", nil, parent, o.order and "UIPanelButtonTemplate,SecureActionButtonTemplate" or "UIPanelButtonTemplate")
    b:SetSize(w, h)
    b:SetText(text or "")
    local function Tint(self)
        local fs = self.GetFontString and self:GetFontString()
        if fs and self.fbTint then fs:SetTextColor(self.fbTint[1], self.fbTint[2], self.fbTint[3]) end
    end
    b:HookScript("OnEnter", Tint)
    b:HookScript("OnLeave", Tint)
    -- row: ROW_GOLD keeps the highlight lit (the chosen button), ROW_BLUE / ROW_RED tint the word
    b.FBSetRow = function(self, row)
        if row == ROW_GOLD then self:LockHighlight() else self:UnlockHighlight() end
        if row == ROW_BLUE then self.fbTint = { 0.62, 0.8, 1 }
        elseif row == ROW_RED then self.fbTint = { 1, 0.45, 0.35 }
        elseif row == 7 then self.fbTint = { 0.45, 1, 0.45 }
        else
            self.fbTint = nil
            local fs = self.GetFontString and self:GetFontString()
            if fs then fs:SetTextColor(1, 0.82, 0) end
        end
        Tint(self)
    end
    if o.order then
        b:RegisterForClicks("AnyUp", "AnyDown")
        b.orderText = o.order
        secureOrders[#secureOrders + 1] = b
        RefreshOrderMacros()
    end
    return b
end

local function ClassColor(token)
    local c = RAID_CLASS_COLORS and RAID_CLASS_COLORS[token]
    if c then
        return string.format("|cff%02x%02x%02x", c.r * 255, c.g * 255, c.b * 255)
    end
    return "|cffffffff"
end

-- Gear the next bots wear. The word goes to the group as the order "gear <word>" before an add; the server reads it
-- (match = like your own gear, the default; best = best for their level; bis; t1, t2, t3 = tier sets).
local GEARS = {
    { "match", "Match" }, { "best", "Best" }, { "bis", "BiS" },
    { "t1", "T1" }, { "t2", "T2" }, { "t3", "T3" },
}
local GEAR_NAME = {
    match = L["Match my gear"], best = L["Best for their level"], bis = L["Best in slot"],
    t1 = L["Tier 1"], t2 = L["Tier 2"], t3 = L["Tier 3"],
}
local gearSent = "match"           -- what the server holds for this group (its default until we say otherwise)

local function SendGear()
    local g = ForeverBotsDB.gear or "match"
    if not GEAR_NAME[g] then g = "match" end
    if InGroupQuiet() then
        if gearSent ~= g then
            gearSent = g
            GroupOrder("gear " .. g)
        end
        return true
    end
    return g == "match"
end

local function AddBotWithGear(class, role)
    local sent = SendGear()
    AddBot(class, role)
    if not sent and C_Timer and C_Timer.After then
        -- no group yet: the first bot makes one, so the gear word follows once it exists (it applies to the next bots)
        C_Timer.After(2.5, function() Auto(SendGear) end)
    end
end

local gearBtn
local function SetGearChoice(g)
    ForeverBotsDB.gear = g
    if gearBtn then gearBtn:SetText(L["short:Gear: %s"]:format(GEAR_NAME[g] and (g == "match" and L["Match"] or g == "best" and L["Best"] or g == "bis" and "BiS" or strupper(g)) or g)) end
    SendGear()
end

-- One click for a ready group, sized to where you are: a party of five in the open world and in 5-player dungeons, the size the
-- instance holds in a bigger one (10 in Upper Blackrock Spire, 20 or 40 in a raid). The members already in the group count, so it
-- never adds past that size: a sixth member made an open-world party a raid, and a raid gets no credit for normal quests
-- (Discord report, 2026-10-27), while in Upper Blackrock Spire it stopped at six of ten.
local QUICK_DPS = { "mage", "rogue", "hunter", "warlock" }
local QUICK_HEALERS = { "priest", "druid" }
local PARTY_SIZE = 5

-- How many players the group should hold here.
local function QuickPartySize()
    local okI, inInstance, kind = pcall(IsInInstance)
    if not (okI and inInstance) or (kind ~= "party" and kind ~= "raid") then return PARTY_SIZE end
    local maxPlayers = 0
    if GetInstanceInfo then
        local okG, _, _, _, _, mp = pcall(GetInstanceInfo)
        if okG and type(mp) == "number" then maxPlayers = mp end
    end
    if maxPlayers <= 0 then maxPlayers = (kind == "raid") and 40 or PARTY_SIZE end
    return math.max(PARTY_SIZE, math.min(maxPlayers, 40))
end

-- The roles for a group of `size`, tanks first, then healers, then damage dealers; the first `count` are added.
-- 5: 1 tank, 1 healer, 2 damage (you are the fifth). 10: 2 tanks, 3 healers. 20: 2 and 5. 40: 4 and 10.
local function QuickPartyRoster(size, count)
    local tanks = (size <= PARTY_SIZE) and 1 or (size <= 20 and 2 or 4)
    local healers = (size <= PARTY_SIZE) and 1 or math.ceil(size / 4)
    local roster = {}
    for i = 1, count do
        if i <= tanks then
            roster[i] = { "warrior", "tank" }
        elseif i <= tanks + healers then
            roster[i] = { QUICK_HEALERS[(i - tanks - 1) % #QUICK_HEALERS + 1], "healer" }
        else
            roster[i] = { QUICK_DPS[(i - tanks - healers - 1) % #QUICK_DPS + 1], "dps" }
        end
    end
    return roster
end

local function QuickParty()
    local members = 1
    if GetNumGroupMembers then members = math.max(GetNumGroupMembers(), 1)
    elseif GetNumPartyMembers then members = GetNumPartyMembers() + 1 end
    local size = QuickPartySize()
    local free = size - members
    if free <= 0 then
        Print(L["quick party: your group is full (%d of %d)."]:format(members, size))
        return
    end
    -- The first bot is said straight from the click and makes the group; the rest follow a moment apart from a timer, where
    -- chat is blocked, so they run under Auto() and go out one by one as addon messages (ServerCommand, Cmd.Queue).
    local roster = QuickPartyRoster(size, free)
    AddBotWithGear(roster[1][1], roster[1][2])
    if roster[2] and C_Timer and C_Timer.After then
        C_Timer.After(0.4, function()
            Auto(function()
                for i = 2, #roster do AddBotWithGear(roster[i][1], roster[i][2]) end
            end)
        end)
    else
        for i = 2, #roster do AddBotWithGear(roster[i][1], roster[i][2]) end
    end
end

-- Dungeon finder (".bot dungeon"): pick a dungeon, or any for your level; the server fills the missing roles with your bots at your level
-- and puts the group inside the dungeon's door. "leave" takes you back to where you queued. The table is the server's default list
-- (Playerbot.Lfg.BotGroups.Activities): the word the server knows the dungeon by, its name, its level range, its party size.
-- Kept on UI, not as file locals: the client's Lua allows a function only 200 locals.
UI.DUNGEONS = {
    { "rfc", "Ragefire Chasm", 13, 18 }, { "wc", "Wailing Caverns", 15, 24 }, { "vc", "Deadmines", 17, 26 },
    { "sfk", "Shadowfang Keep", 20, 30 }, { "bfd", "Blackfathom Deeps", 24, 32 }, { "stocks", "Stormwind Stockades", 24, 32 },
    { "gnomer", "Gnomeregan", 29, 38 }, { "rfk", "Razorfen Kraul", 29, 38 }, { "smgy", "Scarlet Monastery: Graveyard", 30, 38 },
    { "smlib", "Scarlet Monastery: Library", 33, 41 }, { "smarm", "Scarlet Monastery: Armory", 36, 44 },
    { "smcath", "Scarlet Monastery: Cathedral", 38, 46 }, { "rfd", "Razorfen Downs", 37, 46 }, { "ulda", "Uldaman", 41, 51 },
    { "zf", "Zul'Farrak", 44, 54 }, { "mara", "Maraudon", 46, 55 }, { "st", "Sunken Temple", 50, 60 },
    { "brd", "Blackrock Depths", 52, 60 }, { "dme", "Dire Maul: East", 54, 60 }, { "lbrs", "Lower Blackrock Spire", 55, 60 },
    { "dmw", "Dire Maul: West", 56, 60 }, { "dmn", "Dire Maul: North", 56, 60 }, { "strat", "Stratholme", 58, 60 },
    { "scholo", "Scholomance", 58, 60 }, { "ubrs", "Upper Blackrock Spire", 59, 60, 10 },
}

-- The 5-player dungeons for your level, as the server picks "any": those whose range holds it, else the nearest.
function UI.DungeonsForLevel(level)
    local fit, nearest, best = {}, {}, nil
    for _, d in ipairs(UI.DUNGEONS) do
        if not d[5] then
            if level >= d[3] and level <= d[4] then
                fit[#fit + 1] = d
            else
                local off = (level < d[3]) and (d[3] - level) or (level - d[4])
                if not best or off < best then best = off; nearest = {} end
                if off == best then nearest[#nearest + 1] = d end
            end
        end
    end
    return (#fit > 0) and fit or nearest
end

-- Queues for `word` (a dungeon's word, "any", "leave", "cancel" or "list"), with your own role when given. The gear choice goes
-- first, as for Quick party. From the Group finder tab the line goes as a hidden add-on message (Cmd.Queue), typed it is said.
function UI.DungeonQueue(word, role, hidden)
    local line = ".bot dungeon " .. word .. ((role and role ~= "") and (" " .. role) or "")
    local say = hidden and Cmd.Queue or ServerCommand
    if word == "leave" or word == "cancel" or word == "list" then
        say(line)
        return
    end
    local sent = SendGear()
    say(line)
    if not sent and C_Timer and C_Timer.After then
        C_Timer.After(2.5, function() Auto(SendGear) end)
    end
end

local function Tip(b, title, body)
    b:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
        GameTooltip:SetText(title)
        if body then GameTooltip:AddLine(body, 1, 1, 1, true) end
        GameTooltip:Show()
    end)
    b:SetScript("OnLeave", function() GameTooltip:Hide() end)
end

-- Raid ready: one click for the usual start of a raid night. It calls your saved bots (".bot summon saved", the call every player may use), waits until the group
-- stops growing, makes it a raid when it holds more than five, sets the formation and spacing, hands the fight back
-- ("resume"), sets the Main Tank flag when you target a group member, and starts a ready check once the bots have buffed
-- themselves. A new click or combat cancels a run in progress.
local raidReadyRun = 0
local function MemberCount()
    if GetNumGroupMembers then return GetNumGroupMembers() end
    if GetNumRaidMembers and GetNumRaidMembers() > 0 then return GetNumRaidMembers() end
    return (GetNumPartyMembers and GetNumPartyMembers() or 0) + 1
end

local function RaidReady()
    if InCombatLockdown and InCombatLockdown() then
        Print(L["raid ready: not in combat."])
        return
    end
    if not (C_Timer and C_Timer.After) then
        Print(L["raid ready: this client has no timers; use /fb call all."])
        return
    end
    raidReadyRun = raidReadyRun + 1
    local run = raidReadyRun
    local function After(sec, fn)
        C_Timer.After(sec, function()
            if run == raidReadyRun and not (InCombatLockdown and InCombatLockdown()) then Auto(fn) end
        end)
    end
    local function IsRaidNow() return IsInRaid and IsInRaid() end

    local function ReadyCheck()
        local ok = pcall(function() DoReadyCheck() end)
        Print(ok and L["raid ready: ready check sent."] or L["raid ready: done (only the leader can start a ready check)."])
    end
    local function Orders(n)
        GroupOrder("resume")
        After(0.4, function() GroupOrder("formation circle") end)
        After(0.8, function() GroupOrder("spacing close") end)
        After(1.2, function()
            GroupOrder(ForeverBotsDB.cc == false and "crowdcontrol off" or "crowdcontrol on")
            local tgt = UnitExists and UnitExists("target") and not IsSecret(UnitName("target"))
            if tgt and (UnitInRaid and UnitInRaid("target") or UnitInParty and UnitInParty("target")) then
                GroupOrder("set main tank")
            end
        end)
        Print(L["raid ready: %d in the group, formation set. Ready check in 20 seconds."]:format(n))
        After(20, ReadyCheck)
    end
    local function Finish(n)
        if n <= 1 then
            Print(L["raid ready: no bots joined. Save a group with /fb save all first."])
            return
        end
        if n > 5 and not IsRaidNow() then
            local conv = (C_PartyInfo and C_PartyInfo.ConvertToRaid) or ConvertToRaid
            if conv then pcall(conv) end
            After(1.5, function() Orders(MemberCount()) end)
        else
            Orders(n)
        end
    end

    -- Already in a group: it is the group that gets the orders and the ready check. The saved-roster call replaces the group with the
    -- saved raid (a 10-man in a dungeon was dismissed), so it only runs when you are alone.
    if MemberCount() > 1 then
        Print(L["raid ready: using your current group (%d)."]:format(MemberCount()))
        Finish(MemberCount())
        return
    end
    Print(L["raid ready: calling your saved bots..."])
    ServerCommand(".bot summon saved")
    local lastN, stable, waited = -1, 0, 0
    local function Poll()
        local n = MemberCount()
        if n == lastN then stable = stable + 1 else stable = 0; lastN = n end
        waited = waited + 1
        if (n > 1 and stable >= 3) or waited >= 30 then Finish(n) else After(1, Poll) end
    end
    After(2, Poll)
end

-- One table, not five locals: Lua 5.1 (the client's) allows a function only 60 upvalues, and BuildFrame is near that.
local LAYOUT = {
    W = 260,          -- 238 of content between 11 px margins
    CONTENT = 238,
    TOP = 70,
    PANE_H = 250,     -- every tab is this tall: the window never changes size
    FOOTER = 38,
    TAB_STEP = 44,    -- side tabs down the right edge
}
local TAB_DEFS = { { "orders", L["Orders"] }, { "bots", L["Bots"] }, { "form", L["Form"] }, { "marks", L["Marks"] }, { "finder", L["Group finder"] },
                   { "place", L["Positions"] } }   -- no World bots tab (0.6.13, owner decision 2026-11-04: world bots are set in the launcher only)
local frameTabs, framePanes = {}, {}
local emptyFS, ordersGrid, footerFS
local nativeFrame = false

-- Window size: ForeverBotsDB.scale (0.7 to 1.5), 1.2 until set with /fb scale <n>.
local SCALE_MIN, SCALE_MAX, SCALE_DEFAULT = 0.7, 1.5, 1.2
local function WindowScale()
    local n = tonumber(ForeverBotsDB.scale)
    if not n then return SCALE_DEFAULT end
    return math.max(SCALE_MIN, math.min(SCALE_MAX, n))
end

-- In combat the client refuses Show, Hide and moving for a protected frame, and the secure order buttons make the window and its
-- Orders and Form tabs protected: every refused call printed a "the client blocked Frame:Show()" line (Upper Blackrock Spire raid
-- 2026-10-26, a tab click and a drag in a fight). Such a change waits for the end of combat instead; the last one asked for wins.
UI.afterCombat = {}   -- frame -> true (show) or false (hide), done on PLAYER_REGEN_ENABLED

function UI.Locked(f)
    if not (InCombatLockdown and InCombatLockdown()) then return false end
    if f and f.IsProtected then return f:IsProtected() and true or false end
    return true
end

-- Shows or hides f now, or once combat ends when the client would refuse it. Returns false when it waits.
function UI.SetShown(f, show)
    show = show and true or false
    if UI.Locked(f) then
        if f:IsShown() == show then UI.afterCombat[f] = nil else UI.afterCombat[f] = show end
        return false
    end
    UI.afterCombat[f] = nil
    if show then f:Show() else f:Hide() end
    return true
end

local shownTab, tabWaitSaid
function UI.TabShown(i)
    UI.pendingTab = nil
    shownTab = i
    ForeverBotsDB.tab = i
    for k, t in pairs(frameTabs) do t:SetChecked(k == i) end
end
local function ShowTab(i)
    i = (framePanes[i] and i) or 1
    for k, pane in ipairs(framePanes) do
        if pane:IsShown() ~= (k == i) and UI.Locked(pane) then
            -- Bots and Marks still switch in a fight; a switch that shows or hides Orders or Form waits for the end of combat
            UI.pendingTab = i
            for n, t in pairs(frameTabs) do t:SetChecked(n == shownTab) end
            if not tabWaitSaid then
                tabWaitSaid = true
                Print(L["that tab opens when combat ends (the client keeps the order buttons fixed in a fight)."])
            end
            return
        end
    end
    UI.pendingTab = nil
    shownTab = i
    ForeverBotsDB.tab = i
    for k, pane in ipairs(framePanes) do
        -- only the panes that change: in a fight even a Show of a shown protected pane is a refused call
        if pane:IsShown() ~= (k == i) then
            if k == i then pane:Show() else pane:Hide() end
        end
        if frameTabs[k] then frameTabs[k]:SetChecked(k == i) end
    end
end

local function UpdateEmpty()
    if not (emptyFS and ordersGrid) then return end
    local grouped = InGroupQuiet() and true or false
    if grouped then emptyFS:Hide() else emptyFS:Show() end
    ordersGrid:SetAlpha(grouped and 1 or 0.45)
    if footerFS then footerFS:SetText((grouped and L["%d bots"]:format(MemberCount() - 1) or L["No bots"]) .. " - " .. StanceLabel(true)) end
end

-- The client's own window: the same portrait frame its Raid and Looking For Group windows use. If this client has no such
-- template the window falls back to a plain dialog frame with the same contents.
local function CreateWindow()
    for _, tmpl in ipairs({ "PortraitFrameTemplate", "ButtonFrameTemplate" }) do
        local ok, f = pcall(CreateFrame, "Frame", "ForeverBotsFrame", UIParent, tmpl)
        if ok and f then return f, true end
    end
    local f = CreateFrame("Frame", "ForeverBotsFrame", UIParent, BackdropTemplateMixin and "BackdropTemplate" or nil)
    if f.SetBackdrop then
        f:SetBackdrop({
            bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background",
            edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
            tile = true, tileSize = 32, edgeSize = 24,
            insets = { left = 6, right = 6, top = 6, bottom = 6 },
        })
    end
    return f, false
end

local CLASS_ICON_TEX = "Interface\\GLUES\\CHARACTERCREATE\\UI-CHARACTERCREATE-CLASSES"
local ROLE_ATLAS = {
    tank = { "UI-LFG-RoleIcon-Tank", "groupfinder-icon-role-large-tank" },
    healer = { "UI-LFG-RoleIcon-Healer", "groupfinder-icon-role-large-heal" },
    dps = { "UI-LFG-RoleIcon-DPS", "groupfinder-icon-role-large-dps" },
}
local ROLE_TEXCOORDS = { tank = { 0, 19 / 64, 22 / 64, 41 / 64 }, healer = { 20 / 64, 39 / 64, 1 / 64, 20 / 64 }, dps = { 20 / 64, 39 / 64, 22 / 64, 41 / 64 } }
local ROLE_NAME = { tank = L["Tank"], healer = L["Healer"], dps = L["Damage"] }
local function SetRoleTexture(tex, role)
    for _, atlas in ipairs(ROLE_ATLAS[role]) do
        local info = C_Texture and C_Texture.GetAtlasInfo and C_Texture.GetAtlasInfo(atlas)
        if info and pcall(tex.SetAtlas, tex, atlas) then return end
    end
    tex:SetTexture("Interface\\LFGFrame\\UI-LFG-ICON-PORTRAITROLES")
    local c = ROLE_TEXCOORDS[role]
    tex:SetTexCoord(c[1], c[2], c[3], c[4])
end

-- Every pane is a protected frame (SecureFrameTemplate), so the secure tab click below may show and hide each of them in a fight:
-- restricted code only moves protected frames, and Orders and Form are protected anyway through their secure order buttons.
function UI.NewPane()
    local ok, pane = pcall(CreateFrame, "Frame", nil, frame, "SecureFrameTemplate")
    if not (ok and pane) then pane = CreateFrame("Frame", nil, frame) end
    pane:SetSize(LAYOUT.CONTENT, LAYOUT.PANE_H)
    pane:SetPoint("TOPLEFT", frame, "TOPLEFT", 11, -LAYOUT.TOP)
    framePanes[#framePanes + 1] = pane
    return pane
end
function UI.Section(pane, y, text)
    local fs = pane:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    fs:SetPoint("TOPLEFT", pane, "TOPLEFT", 2, -y)
    fs:SetText(text)
    local d = pane:CreateTexture(nil, "ARTWORK")
    d:SetTexture("Interface\\Buttons\\WHITE8X8")
    d:SetVertexColor(0.55, 0.47, 0.25, 0.7)
    d:SetSize(LAYOUT.CONTENT, 1)
    d:SetPoint("TOPLEFT", pane, "TOPLEFT", 0, -(y + 17))
end
function UI.Btn(pane, x, y, w, h, text, o)
    local b = MakeSkinButton(pane, text, w, h, o)
    b:SetPoint("TOPLEFT", pane, "TOPLEFT", x, -y)
    return b
end
function UI.Hint(pane, y, text)
    local fs = pane:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    fs:SetPoint("TOPLEFT", pane, "TOPLEFT", 2, -y)
    fs:SetText(text)
    return fs
end

function UI.BuildOrders()
    local Btn, Section, Hint, NewPane = UI.Btn, UI.Section, UI.Hint, UI.NewPane
    ---------------------------------------------------------------------------------------------- orders
    local orders = NewPane()
    emptyFS = Hint(orders, 0, "|cffc8a050" .. L["No bots with you yet: see the Bots tab."] .. "|r")
    ordersGrid = CreateFrame("Frame", nil, orders)
    ordersGrid:SetAllPoints(orders)
    local OG = {
        -- word, order, tooltip title, tooltip text
        { L["short:Follow"], "follow", L["Follow"], L["Bots follow you and only defend. They do not advance or pull until you say Attack, Pull or Resume."] },
        { L["Stay"], "stay", L["Stay"], L["Bots hold where they stand."] },
        { L["Stop"], "disengage", L["Stop"], L["Bots stop attacking and go back to normal."] },
        { L["Attack"], "attack", L["Attack"], L["Bots attack your target."] },
        { L["Pull"], "pull", L["Pull"], L["Your tank bots open on your target; the rest join once it is engaged."] },
        { L["short:Pull back"], "pull back", L["Pull back"], L["The main tank bot opens on your target from range, runs back to the rally point (or where it stood) and tanks it there, so nothing else aggros."] },
        { L["Come"], "come", L["Come"], L["Bots teleport to you."] },
        { L["short:Release"], "release", L["Release"], L["After a wipe: every dead bot releases, runs back to its corpse and rejoins you. Living bots ignore it."] },
        { L["short:Fall back"], "fallback", L["Fall back"], L["Every bot drops the fight and runs to the rally point, then re-forms there. It fights only what follows until you say Attack, Pull or Resume."] },
    }
    for i, d in ipairs(OG) do
        local b = Btn(ordersGrid, ((i - 1) % 3) * 81, 18 + math.floor((i - 1) / 3) * 32, 76, 26, d[1], { order = d[2], size = 11 })
        Tip(b, d[3], d[4])
    end
    local rally = Btn(ordersGrid, 0, 18 + 3 * 32, 76, 26, L["short:Set rally"], { order = "rally here", size = 11 })
    Tip(rally, L["Set rally"], L["Sets the rally point to where you stand (used by Fall back and Pull back)."])
    -- Stance: Defensive (blue word), Aggressive (red) or Passive (green); a click steps on, a right-click back. Sent to the group on
    -- every pick and again to every bot that joins.
    stanceBtn = Btn(ordersGrid, 81, 18 + 3 * 32, 157, 26, StanceLabel(), { size = 11 })
    stanceBtn:RegisterForClicks("LeftButtonUp", "RightButtonUp")
    stanceBtn:SetScript("OnClick", function(_, button) Cmd.SetStance(button == "RightButton" and -1 or 1) end)
    Tip(stanceBtn, L["Stance"], L["Click for the next stance, right-click for the one before.\nDefensive (the start setting): bots start nothing, fight only what you pull or what attacks the group, and come back to follow you.\nAggressive: they also attack anything hostile within about 10 yards of you.\nPassive: they attack nothing at all, not even to defend you or themselves; healers still heal and everyone still buffs. For taming a beast, a sheep that must not break or a mob that must stay alive.\nShows what you last sent; it is sent again to every bot that joins."])
    RefreshStance()
    -- Ignore target: your target goes on the list of mobs your bots never attack, even when it attacks you (a beast you tame); the
    -- same button on it again takes it off. Clear empties the list. Secure order buttons, so they work in combat.
    local ign = Btn(ordersGrid, 0, 18 + 4 * 32, 157, 26, L["Ignore target"], { order = "ignore target", size = 11 })
    Tip(ign, L["Ignore target"], L["Your bots will not attack the mob you have targeted, even if it attacks you or them, until you press this again on it (or Clear). They fight every other mob as usual. Area spells wait while it is near. Forgotten after 30 minutes."])
    local ignClear = Btn(ordersGrid, 162, 18 + 4 * 32, 76, 26, L["short:Clear ignored"], { order = "ignore clear", size = 11 })
    Tip(ignClear, L["Clear ignored"], L["Your bots may attack every mob again."])

    -- Two on/off check boxes at the bottom (the client's own check box). Area spells: on (the server's start setting) the bots use
    -- them as their rotations call for; off, nothing that hits more than one enemy (server: "aoe_all on|off", whispered to one of
    -- your bots, which applies it to every bot in the group). Crowd control: on (the default) the bots crowd-control the mobs you mark.
    local function Switch(y, key, default, caption, onText, offText, send)
        local cb = CreateFrame("CheckButton", nil, ordersGrid, "UICheckButtonTemplate")
        cb:SetSize(26, 26)
        cb:SetPoint("TOPLEFT", ordersGrid, "TOPLEFT", 0, -y)
        local label = ordersGrid:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
        label:SetPoint("LEFT", cb, "RIGHT", 2, 1)
        label:SetText(caption)
        local function State()
            local on = ForeverBotsDB[key]
            if on == nil then on = default end
            return on
        end
        cb:SetChecked(State())
        cb:SetScript("OnClick", function(self)
            ForeverBotsDB[key] = self:GetChecked() and true or false
            send(State())
        end)
        cb:SetScript("OnEnter", function(self)
            GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
            GameTooltip:SetText(caption)
            GameTooltip:AddLine(State() and onText or offText, 1, 1, 1, true)
            GameTooltip:Show()
        end)
        cb:SetScript("OnLeave", function() GameTooltip:Hide() end)
    end
    Switch(LAYOUT.PANE_H - 56, "aoeOn", true, L["Area spells"],
        L["The bots use their area spells as their rotations call for."],
        L["No spell that hits more than one enemy (Blizzard, Consecration, Multi-Shot, Cleave...): keeps a sheep or sap safe."],
        function(on)
            local asker = ListAsker()
            if asker then SendChatMessage("aoe_all " .. (on and "on" or "off"), "WHISPER", nil, asker)
            else Print(L["add a bot first: the area-spell setting goes to your group."]) end
        end)
    Switch(LAYOUT.PANE_H - 28, "cc", true, L["Crowd-control marked mobs"],
        L["The bots crowd-control the mobs you mark (give a bot and a mob the same icon, see Marks)."],
        L["The bots leave marked mobs alone: no crowd control."],
        function(on) GroupOrder(on and "crowdcontrol on" or "crowdcontrol off") end)
end

function UI.BuildBots()
    local Btn, Section, Hint, NewPane = UI.Btn, UI.Section, UI.Hint, UI.NewPane
    ---------------------------------------------------------------------------------------------- bots
    local botsPane = NewPane()
    UI.botsPane = botsPane
    Section(botsPane, 0, L["Add a bot"])
    -- Every class for both factions: Forever has Dwarf Shamans for the Alliance and Undead Paladins for the Horde.
    local shown = CLASSES
    local selFS = botsPane:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    selFS:SetPoint("TOPLEFT", botsPane, "TOPLEFT", 4, -68)
    selFS:SetWidth(86)
    selFS:SetJustifyH("LEFT")
    selFS:SetWordWrap(false)
    local roleBtns, classBtns, selected = {}, {}, 1
    for i = 1, 3 do
        local rb = CreateFrame("Button", nil, botsPane)
        rb:SetSize(44, 44)
        rb:SetPoint("TOPLEFT", botsPane, "TOPLEFT", 92 + (i - 1) * 52, -52)
        local tex = rb:CreateTexture(nil, "ARTWORK")
        tex:SetAllPoints()
        rb.fbTex = tex
        -- just the role icon, no check box under it (0.6.15, owner decision 2026-11-04): one click on the icon adds the bot
        rb:SetHighlightTexture("Interface\\Minimap\\UI-Minimap-ZoomButton-Highlight", "ADD")
        rb:SetScript("OnClick", function(self)
            local class = shown[selected]
            if not (class and self.fbRole) then return end
            AddBotWithGear(class.key, self.fbRole)
        end)
        rb:SetScript("OnEnter", function(self)
            GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
            GameTooltip:SetText(L["Add a %s bot"]:format(self.fbRole and ROLE_NAME[self.fbRole] or ""))
            GameTooltip:AddLine(L["A new bot joins you at your level, wearing the gear chosen below."], 1, 1, 1, true)
            GameTooltip:Show()
        end)
        rb:SetScript("OnLeave", function() GameTooltip:Hide() end)
        roleBtns[i] = rb
    end
    local function SelectClass(i)
        selected = i
        local class = shown[i]
        selFS:SetText(ClassColor(class.token) .. class.label .. "|r")
        for k, cb in ipairs(classBtns) do
            if k == i then cb:LockHighlight() else cb:UnlockHighlight() end
        end
        for k, rb in ipairs(roleBtns) do
            local role = class.roles[k]
            rb.fbRole = role
            if role then SetRoleTexture(rb.fbTex, role); rb:Show() else rb:Hide() end
        end
    end
    local step = (LAYOUT.CONTENT - 24) / (#shown - 1)
    for i, class in ipairs(shown) do
        local cb = CreateFrame("Button", nil, botsPane)
        cb:SetSize(24, 24)
        cb:SetPoint("TOPLEFT", botsPane, "TOPLEFT", (i - 1) * step, -22)
        local tex = cb:CreateTexture(nil, "ARTWORK")
        tex:SetAllPoints()
        local c = CLASS_ICON_TCOORDS and CLASS_ICON_TCOORDS[class.token]
        tex:SetTexture(CLASS_ICON_TEX)
        if c then tex:SetTexCoord(c[1], c[2], c[3], c[4]) end
        cb:SetHighlightTexture("Interface\\Buttons\\CheckButtonHilight", "ADD")
        cb:SetScript("OnClick", function() SelectClass(i) end)
        cb:SetScript("OnEnter", function(self)
            GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
            GameTooltip:SetText(ClassColor(class.token) .. class.label .. "|r")
            GameTooltip:AddLine(L["Pick the class, then a role below."], 1, 1, 1, true)
            GameTooltip:Show()
        end)
        cb:SetScript("OnLeave", function() GameTooltip:Hide() end)
        classBtns[i] = cb
    end
    SelectClass(1)
    local quick = Btn(botsPane, 0, 106, 117, 24, L["Quick party"], {})
    quick:SetScript("OnClick", QuickParty)
    Tip(quick, L["Quick party"], L["Fills your group with bots at your level: to five in the open world and 5-player dungeons, to the size of a bigger instance (10 in Upper Blackrock Spire) or raid."])
    gearBtn = Btn(botsPane, 121, 106, 117, 24, "", {})
    gearBtn:SetScript("OnClick", function()
        local cur = ForeverBotsDB.gear or "match"
        for i, g in ipairs(GEARS) do
            if g[1] == cur then SetGearChoice(GEARS[i % #GEARS + 1][1]); break end
        end
        if GameTooltip:IsOwned(gearBtn) then gearBtn:GetScript("OnEnter")(gearBtn) end
    end)
    gearBtn:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
        GameTooltip:SetText(L["Gear: %s"]:format(GEAR_NAME[ForeverBotsDB.gear or "match"] or "?"))
        GameTooltip:AddLine(L["The gear the next bots wear. Click to go to the next choice: Match, Best, BiS, T1, T2, T3."], 1, 1, 1, true)
        GameTooltip:Show()
    end)
    gearBtn:SetScript("OnLeave", function() GameTooltip:Hide() end)
    SetGearChoice(GEAR_NAME[ForeverBotsDB.gear or ""] and ForeverBotsDB.gear or "match")

    Section(botsPane, 142, L["Your saved bots"])
    local SV = {
        { L["Save group"], 0, 166, L["Puts every bot with you on your saved list."], function() ServerCommand(".bot save all") end },
        { L["Unsave all"], 1, 166, L["Empties your saved list."], function() ServerCommand(".bot unsave all") end },
        { L["List"], 0, 194, L["Lists your bots in chat (saved ones are marked)."], function() ServerCommand(".bot list") end },
        { L["Regear all"], 1, 194, L["Re-rolls the gear of your saved bots so they do not all wear the same pieces. Better pieces they already wear stay."], function() ServerCommand(".bot regear all") end },
        { L["Delete target"], 0, 222, L["Deletes the targeted bot's character for good (asks first)."], DeleteTarget },
        { L["Manage bots"], 1, 222, L["Opens the bot manager: every bot you own, online or not, with its details and buttons for it (/fb bots)."], function()
            if Cmd.ns.Windows then Cmd.ns.Windows.Toggle() end
        end },
    }
    for _, d in ipairs(SV) do
        local b = Btn(botsPane, d[2] * 121, d[3], 117, 24, d[1], {})
        b:SetScript("OnClick", d[5])
        Tip(b, d[1], d[4])
    end
end

function UI.BuildForm()
    local Btn, Section, Hint, NewPane = UI.Btn, UI.Section, UI.Hint, UI.NewPane
    ---------------------------------------------------------------------------------------------- form
    local formPane = NewPane()
    local formBtns, spaceBtns = {}, {}
    local function Lit(map, key)
        for k, b in pairs(map) do b:FBSetRow(k == key and ROW_GOLD or ROW_NORMAL) end
    end
    Section(formPane, 0, L["Shape"])
    local SH = {
        { "circle", L["Circle"], L["Out of combat the bots stand in a ring round you."] },
        { "arrow", L["Arrow"], L["Out of combat the bots form an arrow pointing the way you face."] },
        { "line", L["Line"], L["Out of combat the bots line up abreast behind you."] },
        { "none", L["None"], L["The plain follow behind you."] },
    }
    for i, d in ipairs(SH) do
        local b = Btn(formPane, (i - 1) * 60, 24, 58, 24, d[2], { order = "formation " .. d[1], size = 11 })
        b:HookScript("PostClick", function() ForeverBotsDB.form = d[1]; Lit(formBtns, d[1]) end)
        Tip(b, d[2], d[3])
        formBtns[d[1]] = b
    end
    Section(formPane, 62, L["Spacing"])
    local SP = {
        { "tight", L["Tight"], L["About 2.5 yards between bots."] },
        { "close", L["Close"], L["About 4 yards between bots (the default)."] },
        { "spread", L["Spread"], L["About 8 yards between bots."] },
    }
    for i, d in ipairs(SP) do
        local b = Btn(formPane, (i - 1) * 81, 86, 76, 24, d[2], { order = "spacing " .. d[1], size = 11 })
        b:HookScript("PostClick", function() ForeverBotsDB.space = d[1]; Lit(spaceBtns, d[1]) end)
        Tip(b, L["%s spacing"]:format(d[2]), d[3])
        spaceBtns[d[1]] = b
    end
    Lit(formBtns, ForeverBotsDB.form)
    Lit(spaceBtns, ForeverBotsDB.space)
    Section(formPane, 124, L["Follow distance"])
    local distMinus = Btn(formPane, 70, 148, 28, 24, "-", { size = 13 })
    distMinus:SetScript("OnClick", function() StepDistance(-1) end)
    distText = formPane:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    distText:SetPoint("TOP", formPane, "TOPLEFT", LAYOUT.CONTENT / 2, -153)
    distText:SetWidth(60)
    distText:SetJustifyH("CENTER")
    local distPlus = Btn(formPane, 140, 148, 28, 24, "+", { size = 13 })
    distPlus:SetScript("OnClick", function() StepDistance(1) end)
    Tip(distMinus, L["Follow distance"], L["How far the bots follow you out of combat, 2 to 20 yards. Also sets the formation spacing."])
    Tip(distPlus, L["Follow distance"], L["How far the bots follow you out of combat, 2 to 20 yards. Also sets the formation spacing."])
    ShowDistance()
end

function UI.BuildMarks()
    local Btn, Section, Hint, NewPane = UI.Btn, UI.Section, UI.Hint, UI.NewPane
    ---------------------------------------------------------------------------------------------- marks
    local marksPane = NewPane()
    Section(marksPane, 0, L["Assign a bot to an icon"])
    Hint(marksPane, 24, L["Target a bot, then click an icon."])
    statusFS = marksPane:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    for i = 1, 8 do
        local ib = CreateFrame("Button", nil, marksPane)
        ib:SetSize(28, 28)
        ib:SetPoint("TOPLEFT", marksPane, "TOPLEFT", (i - 1) * 30, -40)
        local tex = ib:CreateTexture(nil, "ARTWORK")
        tex:SetPoint("TOPLEFT", 3, -3)
        tex:SetPoint("BOTTOMRIGHT", -3, 3)
        tex:SetTexture(ICON_TEX .. i)
        ib:SetNormalTexture("Interface\\Buttons\\UI-Quickslot2")
        ib:SetPushedTexture("Interface\\Buttons\\UI-Quickslot-Depress")
        ib:SetHighlightTexture("Interface\\Buttons\\ButtonHilight-Square", "ADD")
        ib:RegisterForClicks("LeftButtonUp", "RightButtonUp")
        ib:SetScript("OnClick", function(_, button) AssignTarget(i, button) end)
        ib:SetScript("OnEnter", function(self)
            GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
            GameTooltip:SetText(MarkerName(ICON_WORDS[i]))
            GameTooltip:AddLine(L["Click: assign the targeted bot"], 1, 1, 1)
            GameTooltip:AddLine(L["Right-click: clear this icon"], 1, 1, 1)
            GameTooltip:Show()
        end)
        ib:SetScript("OnLeave", function() GameTooltip:Hide() end)
    end
    local roleBtn
    roleBtn = Btn(marksPane, 0, 76, 76, 24, L["Role: %s"]:format(L["auto"]), {})
    roleBtn:SetScript("OnClick", function()
        roleChoice = roleChoice % #ROLE_CYCLE + 1
        roleBtn:SetText(L["Role: %s"]:format(L[ROLE_CYCLE[roleChoice]]))
    end)
    Tip(roleBtn, L["Role"], L["auto: a tank bot tanks the mob with that icon, casters crowd-control it until a skull goes on, the rest kill it first. Click to force tank, cc or kill for the next assignment."])
    local clearAll = Btn(marksPane, 81, 76, 76, 24, L["Clear all"], {})
    clearAll:SetScript("OnClick", function()
        GroupOrder("assign clear")
        lastAssign = nil
        SetStatus("|cffffff00" .. L["Cleared all, asking the server..."] .. "|r")
        RequestListSoon()
    end)
    Tip(clearAll, L["Clear all"], L["Removes every icon assignment. Right-click an icon to clear just that one."])
    local refreshBtn = Btn(marksPane, 162, 76, 76, 24, L["Refresh"], {})
    refreshBtn:SetScript("OnClick", RequestList)
    statusFS:SetPoint("TOPLEFT", marksPane, "TOPLEFT", 4, -106)
    statusFS:SetWidth(LAYOUT.CONTENT - 8)
    statusFS:SetJustifyH("LEFT")
    for i = 1, MAX_LIST do
        local fs = marksPane:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
        fs:SetPoint("TOPLEFT", marksPane, "TOPLEFT", 4, -118 - (i - 1) * 11)
        fs:SetWidth(LAYOUT.CONTENT - 8)
        fs:SetJustifyH("LEFT")
        fs:SetWordWrap(false)
        listLines[i] = fs
    end
    RefreshList()

    -- the five list lines end at 174; the Tanks block sits below them and clear of the footer buttons
    Section(marksPane, 178, L["Tanks"])
    local markerBtn
    local mt = Btn(marksPane, 0, 200, 117, 22, L["Main tank"], {})
    mt:SetScript("OnClick", function() GroupOrder("set main tank") end)
    Tip(mt, L["Make main tank"], L["Sets the raid Main Tank flag on the raid member you target. Raid groups only."])
    markerBtn = Btn(marksPane, 121, 200, 117, 22, L["Mark: %s"]:format(MarkerName(TANK_MARKERS[tankMarker])), {})
    markerBtn:SetScript("OnClick", function(self)
        tankMarker = tankMarker % #TANK_MARKERS + 1
        self:SetText(L["Mark: %s"]:format(MarkerName(TANK_MARKERS[tankMarker])))
    end)
    Tip(markerBtn, L["Marker"], L["Click to cycle the marker (skull, cross, star, circle, diamond, triangle, moon, square) the next button hands out."])
    local give = Btn(marksPane, 0, 224, 117, 22, L["Give to tank"], {})
    give:SetScript("OnClick", function() GroupOrder("tank target " .. TANK_MARKERS[tankMarker]) end)
    Tip(give, L["Give marker to tank"], L["The tank bot you target holds whichever mob carries this marker (mark the mob yourself with the raid icons)."])
    local clearTanks = Btn(marksPane, 121, 224, 117, 22, L["Clear tanks"], {})
    clearTanks:SetScript("OnClick", function() GroupOrder("clear tanks") end)
    Tip(clearTanks, L["Clear tanks"], L["Back to the automatic tank split."])
end

function UI.BuildFooter()
    ---------------------------------------------------------------------------------------------- footer
    -- group and stance on the left (one line, never wrapped) on every tab. Call saved and Dismiss all only on the Bots tab: they
    -- belong to the Bots pane, so they show and hide with it (owner request 2026-11-04: not on every page). Plain buttons, so
    -- hiding them is allowed in a fight too.
    footerFS = frame:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    footerFS:SetPoint("BOTTOMLEFT", frame, "BOTTOMLEFT", 14, 17)
    footerFS:SetWidth(92)
    footerFS:SetJustifyH("LEFT")
    footerFS:SetWordWrap(false)
    local owner = UI.botsPane or frame
    local fCall = MakeSkinButton(owner, L["Call saved"], 72, 22, {})
    fCall:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", -88, 12)
    fCall:SetScript("OnClick", function() ServerCommand(".bot summon saved") end)
    Tip(fCall, L["Call saved"], L["Brings your saved bots, and every bot in your group, to you."])
    local fDismiss = MakeSkinButton(owner, L["short:Dismiss all"], 74, 22, {})
    fDismiss:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", -12, 12)
    fDismiss:SetScript("OnClick", function() ServerCommand(".bot dismiss all") end)
    Tip(fDismiss, L["Dismiss all"], L["Sends all your bots away."])
end

function UI.BuildTabs()
    ---------------------------------------------------------------------------------------------- side tabs
    -- Square icon tabs down the right edge of the window, built like the spell book's side tabs (its tab border art, a client
    -- icon, the same check highlight), each with a tooltip naming the page.
    local TAB_ICON = {
        orders = "Interface\\Icons\\Ability_Warrior_BattleShout",
        bots = "Interface\\Icons\\INV_Misc_GroupNeedMore",
        form = "Interface\\Icons\\INV_Misc_Map_01",
        marks = "Interface\\TargetingFrame\\UI-RaidTargetingIcon_8",
        finder = "Interface\\Icons\\INV_Misc_Spyglass_03",
        place = "Interface\\Icons\\Ability_Warrior_DefensiveStance",
    }
    -- The tabs are secure click handlers: the client runs their pane switch as its own code, so they work in combat too (an add-on
    -- function may not show or hide the Orders pane in a fight, 0.5.15 in UBRS 2026-10-26: Follow and Stop could not be reached).
    -- Without the secure template (an older client) a tab is a plain button and a switch to or from Orders waits for the end of combat.
    local switchSnippet = [[
        local want = self:GetAttribute("fbtab")
        for k = 1, 7 do
            local p = self:GetFrameRef("fbpane" .. k)
            if p then
                if k == want then p:Show() else p:Hide() end
            end
        end
    ]]
    for i, def in ipairs(TAB_DEFS) do
        local okT, t = pcall(CreateFrame, "CheckButton", nil, frame, "SecureHandlerClickTemplate")
        if okT and t and t.SetFrameRef then
            t:SetAttribute("fbtab", i)
            for k, pane in ipairs(framePanes) do t:SetFrameRef("fbpane" .. k, pane) end
            t:SetAttribute("_onclick", switchSnippet)
            t.fbSecure = true
        else
            t = CreateFrame("CheckButton", nil, frame)
        end
        t:SetSize(32, 32)
        t:SetPoint("TOPLEFT", frame, "TOPRIGHT", -1, -(36 + (i - 1) * LAYOUT.TAB_STEP))
        local border = t:CreateTexture(nil, "BACKGROUND")
        border:SetTexture("Interface\\SpellBook\\SpellBook-SkillLineTab")
        border:SetSize(64, 64)
        border:SetPoint("TOPLEFT", -3, 11)
        local icon = t:CreateTexture(nil, "ARTWORK")
        icon:SetAllPoints()
        icon:SetTexture(TAB_ICON[def[1]])
        if def[1] == "bots" then SetRoleTexture(icon, "tank") end   -- the Add a bot row's shield, known to render here
        if not icon:GetTexture() then icon:SetTexture("Interface\\Icons\\INV_Misc_QuestionMark") end
        t.fbIcon = icon
        t:SetHighlightTexture("Interface\\Buttons\\ButtonHilight-Square", "ADD")
        t:SetCheckedTexture("Interface\\Buttons\\CheckButtonHilight")
        -- SetCheckedTexture takes no blend mode (only SetHighlightTexture does): without ADD the checked square is drawn opaque,
        -- which blacked out whichever tab was selected
        local ct = t:GetCheckedTexture()
        if ct and ct.SetBlendMode then ct:SetBlendMode("ADD") end
        if t.fbSecure then
            -- after the secure switch: remember the tab and light it
            t:HookScript("OnClick", function() UI.TabShown(i) end)
        else
            t:SetScript("OnClick", function() ShowTab(i) end)
        end
        t:SetScript("OnEnter", function(self)
            GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
            GameTooltip:SetText(def[2])
            GameTooltip:Show()
        end)
        t:SetScript("OnLeave", function() GameTooltip:Hide() end)
        frameTabs[i] = t
    end
end

local function BuildFrame()
    local native
    frame, native = CreateWindow()
    nativeFrame = native
    frame:SetSize(LAYOUT.W, LAYOUT.TOP + LAYOUT.PANE_H + LAYOUT.FOOTER)
    frame:SetScale(WindowScale())
    frame:SetPoint("CENTER")
    frame:SetFrameStrata("DIALOG")
    frame:SetMovable(true)
    frame:SetClampedToScreen(true)
    frame:EnableMouse(true)
    frame:RegisterForDrag("LeftButton")
    -- no drag in a fight (the client refuses moving a protected frame); a drag still going when combat starts is ended then
    frame:SetScript("OnDragStart", function(self)
        if UI.Locked(self) then return end
        self.fbMoving = true
        self:StartMoving()
    end)
    frame:SetScript("OnDragStop", function(self) UI.StopDrag(self) end)
    if ForeverBotsDB.pos then
        local p = ForeverBotsDB.pos
        frame:ClearAllPoints()
        frame:SetPoint(p[1], UIParent, p[2], p[3], p[4])
    end

    -- title bar: the template's own centred title and portrait slot (a small group icon), its own close button
    local titled = false
    if native and frame.SetTitle then titled = pcall(frame.SetTitle, frame, "WoW Forever Bots") end
    if native and frame.SetPortraitToAsset then pcall(frame.SetPortraitToAsset, frame, "Interface\\Icons\\INV_Misc_GroupNeedMore") end
    if not titled then
        local t = frame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
        t:SetPoint("TOP", frame, "TOP", 0, -10)
        t:SetText("WoW Forever Bots")
    end
    local close = native and frame.CloseButton
    if not close then
        close = CreateFrame("Button", nil, frame, "UIPanelCloseButton")
        close:SetPoint("TOPRIGHT", 2, 2)
    end
    -- The template's close script is replaced by a secure one that hides the window in combat too (an add-on function may not).
    -- The secure handler sits on its own button over the X, as the X itself was created without the template.
    local okC, sc = pcall(CreateFrame, "Button", nil, close, "SecureHandlerClickTemplate")
    if okC and sc and sc.SetFrameRef then
        sc:SetAllPoints(close)
        sc:SetFrameRef("fbwin", frame)
        sc:SetAttribute("_onclick", [[ self:GetFrameRef("fbwin"):Hide() ]])
        sc:HookScript("OnEnter", function() local f = close:GetScript("OnEnter"); if f then f(close) end end)
        close:SetScript("OnClick", nil)
    else
        close:SetScript("OnClick", function() UI.SetWindowShown(false) end)
    end
    -- the secure window key (Key Bindings: "Show / hide the bot window (works in combat)")
    if UI.keyWindow and UI.keyWindow.SetFrameRef then UI.keyWindow:SetFrameRef("fbwin", frame) end

    -- dark inner panel under the contents, as in the client's own windows
    local inset = frame.Inset
    if not inset then
        local okI, f = pcall(CreateFrame, "Frame", nil, frame, "InsetFrameTemplate")
        if okI then inset = f end
    end
    if inset then
        inset:ClearAllPoints()
        inset:SetPoint("TOPLEFT", frame, "TOPLEFT", 6, -26)
        inset:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", -6, 6)
    end

    local raidReadyBtn = MakeSkinButton(frame, L["Raid ready"], LAYOUT.CONTENT, 24, {})
    raidReadyBtn:SetScript("OnClick", RaidReady)
    raidReadyBtn:SetPoint("TOPLEFT", 11, -36)
    Tip(raidReadyBtn, L["Raid ready"], L["Alone: calls your saved bots. Already in a group: uses that group and never changes it. Makes the group a raid if it needs one, sets the formation and spacing, and starts a ready check once the bots have buffed. Target your main tank first to set the Main Tank flag too. Not in combat."])

    UI.BuildOrders()
    UI.BuildBots()
    UI.BuildForm()
    UI.BuildMarks()
    -- Group finder (ForeverBots_Finder.lua): the pane is made here so the fifth tab always has one
    local finderPane = UI.NewPane()
    if Cmd.ns.BuildFinder then
        Cmd.ns.BuildFinder(finderPane, { Btn = UI.Btn, Section = UI.Section, Tip = Tip, DUNGEONS = UI.DUNGEONS,
                                         DungeonsForLevel = UI.DungeonsForLevel, Queue = UI.DungeonQueue,
                                         Lfg = function(role) SendGear(); Cmd.Queue(".bot lfg" .. (role and (" " .. role) or "")) end })
    end
    -- Positions (ForeverBots_Place.lua): ticked bots stand in a shape here or round the target and stay there
    local placePane = UI.NewPane()
    if Cmd.ns.BuildPlace then
        Cmd.ns.BuildPlace(placePane, { Btn = UI.Btn, Section = UI.Section, Tip = Tip })
    end
    UI.BuildFooter()
    UI.BuildTabs()

    frame:SetScript("OnUpdate", function(self, elapsed)
        self.fbTick = (self.fbTick or 0) + elapsed
        self.fbSlow = (self.fbSlow or 0) + elapsed
        if self.fbTick >= 30 then self.fbTick = 0; RequestList() end
        if self.fbSlow >= 0.5 then self.fbSlow = 0; UpdateEmpty() end
    end)

    ShowTab(ForeverBotsDB.tab or 1)
    UpdateEmpty()

    frame:Hide()
    -- Deliberately NOT in UISpecialFrames: Escape and "close all windows" must not close the panel.
    -- Only /fb (or the X button) closes it, and the open state is remembered across /reload and relog.
    frame:SetScript("OnShow", function(self)
        ForeverBotsDB.open = true
        UI.afterCombat[self] = nil   -- shown by the secure window key: a waiting open is done
        self.fbTick = 0
        RequestList()
        UpdateEmpty()
    end)
    frame:SetScript("OnHide", function(self) ForeverBotsDB.open = false; UI.afterCombat[self] = nil end)
end


-- Sends the coalesced distance order and re-sends it silently when the group grows or shrinks (new bots need it).
local function GroupSize()
    if GetNumGroupMembers then return GetNumGroupMembers() end
    if GetNumRaidMembers and GetNumRaidMembers() > 0 then return GetNumRaidMembers() end
    return (GetNumPartyMembers and GetNumPartyMembers() or 0)
end
local distFrame = CreateFrame("Frame")
distFrame.acc = 0
distFrame:SetScript("OnUpdate", function(self, elapsed)
    self.acc = self.acc + elapsed
    if self.acc < 0.1 then return end
    self.acc = 0
    local now = Now()
    if distDirtyAt and now - distDirtyAt >= 0.4 and now - distLastSend >= 0.3 then
        Auto(SendDistance, false)
    end
    if self.groupAt and now >= self.groupAt then
        self.groupAt = nil
        Auto(SendDistance, true)
    end
    if self.stanceAt and now >= self.stanceAt then
        self.stanceAt = nil
        -- The bots of a group start on the server's default stance, so the stance the panel shows is sent to every one of them
        -- (new and re-created bots included); a never-picked stance is sent only inside an instance, where Passive is the default.
        -- As "stance ...": it sets the stance only. A plain "passive" also calls off an attack under way, so a bot joining just after
        -- you said "attack" called the whole group off (UBRS: the melee stood by you until you said it again).
        local okI, inInstance = pcall(IsInInstance)
        if GroupSize() > 0 and (ForeverBotsDB.stance or (okI and inInstance)) then
            Auto(GroupOrder, Cmd.STANCE_RESEND[Cmd.StanceKey()])
        end
        RefreshStance()
    end
end)
local function OnGroupChanged()
    local n = GroupSize()
    if n ~= lastGroupSize then
        lastGroupSize = n
        if n == 0 then gearSent = "match" end
        if n > 0 and ForeverBotsDB.distance then distFrame.groupAt = Now() + 2 end
        if n > 0 then distFrame.stanceAt = Now() + 2.5 end
        RefreshStance()
    end
end
for _, ev in ipairs({ "GROUP_ROSTER_UPDATE", "PARTY_MEMBERS_CHANGED", "RAID_ROSTER_UPDATE" }) do
    pcall(distFrame.RegisterEvent, distFrame, ev)
end
-- A zone change or loading screen can re-create the bots on the server (their stance starts over), with the group size unchanged:
-- the stance is sent again then, so a pick of Aggressive or Passive is never lost.
for _, ev in ipairs({ "PLAYER_ENTERING_WORLD", "ZONE_CHANGED_NEW_AREA" }) do
    pcall(distFrame.RegisterEvent, distFrame, ev)
end
distFrame:SetScript("OnEvent", function(self, event)
    if event == "PLAYER_ENTERING_WORLD" or event == "ZONE_CHANGED_NEW_AREA" then
        if GroupSize() > 0 then self.stanceAt = Now() + 2.5 end
        return
    end
    OnGroupChanged()
end)

function UI.StopDrag(self)
    if not self.fbMoving then return end
    self.fbMoving = nil
    self:StopMovingOrSizing()
    local point, _, relPoint, x, y = self:GetPoint()
    ForeverBotsDB.pos = { point, relPoint, x, y }
end

-- Opens or closes the window, or says it will once combat ends. The window is built out of combat only: it creates the secure
-- order buttons and sets their macros.
function UI.SetWindowShown(show)
    if not frame then
        if not show then UI.buildAfterCombat = nil; return end
        if InCombatLockdown and InCombatLockdown() then
            UI.buildAfterCombat = true
            Print(L["the bot window opens when combat ends."])
            return
        end
        BuildFrame()
    end
    if not UI.SetShown(frame, show) and UI.afterCombat[frame] ~= nil then
        Print(show and L["the bot window opens when combat ends."] or L["the bot window closes when combat ends."])
    end
end

local function Toggle()
    local open
    if frame then
        open = frame:IsShown()
        if UI.afterCombat[frame] ~= nil then open = UI.afterCombat[frame] end   -- a second press undoes the one waiting
    else
        open = UI.buildAfterCombat and true or false
    end
    UI.SetWindowShown(not open)
end

local combatFrame = CreateFrame("Frame")
combatFrame:RegisterEvent("PLAYER_REGEN_DISABLED")
combatFrame:RegisterEvent("PLAYER_REGEN_ENABLED")
combatFrame:SetScript("OnEvent", function(_, event)
    if event == "PLAYER_REGEN_DISABLED" then
        -- still allowed here, a moment before the lockdown starts
        if frame then UI.StopDrag(frame) end
        return
    end
    if UI.buildLater then
        UI.buildLater = nil
        if not frame then BuildFrame() end
    end
    if UI.buildAfterCombat then
        UI.buildAfterCombat = nil
        if not frame then BuildFrame() end
        frame:Show()
    end
    if frame and UI.scaleAfterCombat then
        UI.scaleAfterCombat = nil
        frame:SetScale(WindowScale())
    end
    if UI.pendingTab then ShowTab(UI.pendingTab) end
    tabWaitSaid = nil
    for f, show in pairs(UI.afterCombat) do
        UI.afterCombat[f] = nil
        if show then f:Show() else f:Hide() end
    end
end)

function ForeverBots_ToggleWindow() Toggle() end

------------------------------------------------------------------------------------------------------------- minimap button
-- The FB button on the minimap ring: left-click opens the window, right-click a short menu, drag moves it round the ring.
-- ForeverBotsDB.minimap = { angle = degrees, hidden = bool }; /fb minimap brings it back after "Hide this button".
local mmButton, mmMenu
local atan2 = math.atan2 or math.atan

local function MinimapPlace()
    if not (mmButton and Minimap) then return end
    local a = math.rad(ForeverBotsDB.minimap.angle or 215)
    local r = (Minimap:GetWidth() / 2) + 5
    mmButton:ClearAllPoints()
    mmButton:SetPoint("CENTER", Minimap, "CENTER", math.cos(a) * r, math.sin(a) * r)
end

local function MinimapDrag()
    local cx, cy = Minimap:GetCenter()
    local mx, my = GetCursorPosition()
    local scale = Minimap:GetEffectiveScale()
    ForeverBotsDB.minimap.angle = math.deg(atan2(my / scale - cy, mx / scale - cx))
    MinimapPlace()
end

local function ToggleMinimapMenu()
    if not mmMenu then
        local m = CreateFrame("Frame", "ForeverBotsMinimapMenu", UIParent, BackdropTemplateMixin and "BackdropTemplate" or nil)
        mmMenu = m
        m:SetSize(150, 4 * 28 + 12)
        m:SetFrameStrata("DIALOG")
        m:EnableMouse(true)
        if m.SetBackdrop then
            m:SetBackdrop({
                bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background", edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
                tile = true, tileSize = 32, edgeSize = 16,
                insets = { left = 4, right = 4, top = 4, bottom = 4 },
            })
        end
        local items = {
            { L["Raid ready"], RaidReady },
            { L["Call saved bots"], function() ServerCommand(".bot summon saved") end },
            { L["Dismiss all bots"], function() ServerCommand(".bot dismiss all") end },
            { L["Hide this button"], function()
                ForeverBotsDB.minimap.hidden = true
                mmButton:Hide()
                Print(L["minimap button hidden: /fb minimap brings it back."])
            end },
        }
        for i, it in ipairs(items) do
            local b = MakeSkinButton(m, it[1], 138, 24, { size = 12 })
            b:SetPoint("TOPLEFT", 6, -6 - (i - 1) * 28)
            b:SetScript("OnClick", function() m:Hide(); it[2]() end)
        end
        m:SetScript("OnUpdate", function(self, elapsed)
            -- (the global MouseIsOver no longer exists on this client: "attempt to call a nil value" every frame)
            if self:IsMouseOver() or mmButton:IsMouseOver() then self.away = 0 else self.away = (self.away or 0) + elapsed end
            if self.away > 1.5 then self:Hide() end
        end)
        m:Hide()
    end
    if mmMenu:IsShown() then mmMenu:Hide(); return end
    mmMenu:ClearAllPoints()
    local x = mmButton:GetCenter()
    if x and x > UIParent:GetWidth() / 2 then
        mmMenu:SetPoint("TOPRIGHT", mmButton, "BOTTOMRIGHT", 0, -2)
    else
        mmMenu:SetPoint("TOPLEFT", mmButton, "BOTTOMLEFT", 0, -2)
    end
    mmMenu.away = 0
    mmMenu:Show()
end

local function BuildMinimapButton()
    Step("minimap button")
    ForeverBotsDB.minimap = ForeverBotsDB.minimap or {}
    if mmButton or not Minimap then return end
    local b = CreateFrame("Button", "ForeverBotsMinimapButton", Minimap)
    mmButton = b
    b:SetSize(31, 31)
    b:SetFrameStrata("MEDIUM")
    b:SetFrameLevel(8)
    b:RegisterForClicks("LeftButtonUp", "RightButtonUp")
    b:RegisterForDrag("LeftButton")
    local icon = b:CreateTexture(nil, "BACKGROUND")
    icon:SetTexture(TEXDIR .. "mono_fb64")
    icon:SetSize(20, 20)
    icon:SetPoint("TOPLEFT", 6, -5)
    local border = b:CreateTexture(nil, "OVERLAY")
    border:SetTexture("Interface\\Minimap\\MiniMap-TrackingBorder")
    border:SetSize(53, 53)
    border:SetPoint("TOPLEFT")
    b:SetHighlightTexture("Interface\\Minimap\\UI-Minimap-ZoomButton-Highlight")
    b:SetScript("OnClick", function(_, button)
        if button == "RightButton" then ToggleMinimapMenu() else Toggle() end
    end)
    b:SetScript("OnDragStart", function(self) self:SetScript("OnUpdate", MinimapDrag) end)
    b:SetScript("OnDragStop", function(self) self:SetScript("OnUpdate", nil) end)
    b:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_LEFT")
        GameTooltip:SetText("WoW Forever Bots")
        GameTooltip:AddLine(L["Click: open or close the bot window"], 1, 1, 1)
        GameTooltip:AddLine(L["Right-click: raid ready, call, dismiss"], 1, 1, 1)
        GameTooltip:AddLine(L["Drag: move this button"], 0.7, 0.7, 0.7)
        GameTooltip:Show()
    end)
    b:SetScript("OnLeave", function() GameTooltip:Hide() end)
    MinimapPlace()
    if ForeverBotsDB.minimap.hidden then b:Hide() end
    Step("")
end

local function SetMinimapShown(show)
    BuildMinimapButton()
    if not mmButton then return end
    ForeverBotsDB.minimap.hidden = not show
    if show then mmButton:Show() else mmButton:Hide() end
end

function ForeverBots_ToggleStance()
    if not ForeverBotsDB then return end
    Cmd.SetStance(1)
    Print(L["stance: %s"]:format(StanceLabel()))
end

------------------------------------------------------------------------------------------------------------- bot panel
-- A small floating panel for ONE bot: it follows your target whenever that is a friendly player in your group (a bot),
-- and every button is a whisper to that bot ("/w <name> follow"), so the order acts on that bot only and the bot
-- answers with a confirmation whisper "<Name>: <what it did>", which fills the status line (and is hidden from chat).
-- Not a protected frame, so it opens and closes in combat. /fb bot on|off toggles it (default on); the position is saved
-- in ForeverBotsDB.botPanelPos. Another bot's whisper that is not a confirmation stays in chat and also lands in the status.
-- Bot detection: the client has no "is a bot" flag, so a bot is a friendly player of your party/raid other than you. Names
-- that have answered with a confirmation are remembered for the session (knownBots) and shown without the "?" mark.

local botPanel, botName, botClassToken, botStatusFS, botTitleFS, botDistFS, botRoleBtn
local botRoleChoice = 1                 -- ROLE_CYCLE index for the next icon click (resets to auto)
local botDist = nil                     -- yards shown for the current bot
local botDistDirtyAt, botSentAt, botForced
local knownBots = {}

local OpenWhisperBox   -- defined below (the chat box helper); the panel and the invite button call it from a click

local function BotPanelEnabled() return ForeverBotsDB.botPanel ~= false end

local function SetBotStatus(text) if botStatusFS then botStatusFS:SetText(text or "") end end

local function InMyGroup(unit)
    if not (UnitExists(unit) and UnitIsPlayer(unit)) or UnitIsUnit(unit, "player") or not UnitIsFriend("player", unit) then
        return false
    end
    return (UnitInParty and UnitInParty(unit)) or (UnitInRaid and UnitInRaid(unit)) and true or false
end

-- The player's own invite: the call typed /invite makes (the client's portrait "Invite" sends nothing for a bot on 70338).
local function InviteByName(name)
    -- Always says what it did, so a click that sends nothing is visible in the chat frame (portrait Invite, 2026-10-12: no
    -- packet reached the server and nothing was shown).
    local can
    if C_PartyInfo and C_PartyInfo.CanInvite then
        local okC, res = pcall(C_PartyInfo.CanInvite)
        if okC then can = res end
    end
    if can == false then Print(L["cannot invite %s (the game says you may not invite right now)"]:format(name)) end
    local sent, how = false, nil
    if C_PartyInfo and C_PartyInfo.InviteUnit then
        local ok, err = pcall(C_PartyInfo.InviteUnit, name)
        if ok then sent, how = true, "C_PartyInfo.InviteUnit" else Debug("invite: C_PartyInfo.InviteUnit failed: " .. tostring(err)) end
    end
    if not sent and InviteUnit then
        local ok, err = pcall(InviteUnit, name)
        if ok then sent, how = true, "InviteUnit" else Debug("invite: InviteUnit failed: " .. tostring(err)) end
    end
    if not sent and SlashCmdList and SlashCmdList.INVITE then
        local ok, err = pcall(SlashCmdList.INVITE, name)
        if ok then sent, how = true, "/invite" else Debug("invite: /invite failed: " .. tostring(err)) end
    end
    if sent then Print(L["invite sent to %s (%s)"]:format(name, how)) else Print(L["could not send the invite; type /invite %s"]:format(name)) end
end

local function BotWhisper(text)
    if not botName then return end
    if inAuto then
        -- from the panel's timer (the coalesced distance order): an addon whisper, a chat whisper would be blocked
        if not prefixOk then return end
        local send = C_ChatInfo and C_ChatInfo.SendAddonMessage or SendAddonMessage
        pcall(send, ORDER_PREFIX, text, "WHISPER", botName)
    else
        SendChatMessage(text, "WHISPER", nil, botName)
        Cmd.NoteOwnWhisper(botName, text)
    end
    botSentAt = Now()
    SetBotStatus("|cffffff00" .. text .. " ...|r")
end

local function BotStepDistance(delta)
    botDist = math.max(DIST_MIN, math.min(DIST_MAX, math.floor((botDist or ForeverBotsDB.distance or DIST_START) + delta)))
    if botDistFS then botDistFS:SetText(L["%d yd"]:format(botDist)) end
    botDistDirtyAt = Now()
end

local function UpdateBotPanel()
    if not botPanel then return end
    local name, token
    if botForced then
        name, token = botForced.name, botForced.token
    elseif BotPanelEnabled() and InMyGroup("target") then
        name = SafeName("target")
        token = select(2, UnitClass("target"))
    end
    if not name then
        botName = nil
        botPanel:Hide()
        return
    end
    if name ~= botName then
        botName, botClassToken = name, token
        botRoleChoice = 1
        botRoleBtn:SetText(L["Role: %s"]:format(L["auto"]))
        botDist = ForeverBotsDB.distance or DIST_START
        botDistFS:SetText(L["%d yd"]:format(botDist))
        botDistDirtyAt, botSentAt = nil, nil
        SetBotStatus("")
    end
    botTitleFS:SetText(ClassColor(botClassToken) .. name .. "|r" .. (knownBots[name] and "" or " |cff9d9d9d?|r"))
    botPanel:Show()
end

local function BuildBotPanel()
    local p = CreateFrame("Frame", "ForeverBotsBot", UIParent, BackdropTemplateMixin and "BackdropTemplate" or nil)
    botPanel = p
    p:SetSize(236, 140)
    p:SetFrameStrata("MEDIUM")
    p:SetClampedToScreen(true)
    p:SetMovable(true)
    p:EnableMouse(true)
    p:RegisterForDrag("LeftButton")
    p:SetScript("OnDragStart", p.StartMoving)
    p:SetScript("OnDragStop", function(self)
        self:StopMovingOrSizing()
        local point, _, relPoint, x, y = self:GetPoint()
        ForeverBotsDB.botPanelPos = { point, relPoint, x, y }
    end)
    if p.SetBackdrop then
        p:SetBackdrop({
            bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background",
            edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
            tile = true, tileSize = 32, edgeSize = 16,
            insets = { left = 4, right = 4, top = 4, bottom = 4 },
        })
    end
    local pos = ForeverBotsDB.botPanelPos
    if pos then
        p:SetPoint(pos[1], UIParent, pos[2], pos[3], pos[4])
    elseif TargetFrame then
        p:SetPoint("TOPLEFT", TargetFrame, "BOTTOMLEFT", 0, -4)
    else
        p:SetPoint("CENTER", UIParent, "CENTER", 200, 0)
    end

    botTitleFS = p:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    botTitleFS:SetPoint("TOPLEFT", 12, -10)
    local close = CreateFrame("Button", nil, p, "UIPanelCloseButton")
    close:SetSize(22, 22)
    close:SetPoint("TOPRIGHT", -2, -2)
    -- The X turns the panel off for good: it used to hide only until the next target change, so it came back each time a bot was
    -- clicked and could not be got rid of (Discord 2026-11-09). /fb bot on, or "Open the bot panel" in a bot's menu, brings it back.
    close:SetScript("OnClick", function()
        botForced = nil
        ForeverBotsDB.botPanel = false
        botPanel:Hide()
        Print(L["bot panel is %s."]:format(L["off"]) .. " /fb bot on")
    end)

    for i = 1, 8 do
        local ib = CreateFrame("Button", nil, p)
        ib:SetSize(24, 24)
        ib:SetPoint("TOPLEFT", 12 + (i - 1) * 27, -30)
        local tex = ib:CreateTexture(nil, "ARTWORK")
        tex:SetAllPoints()
        tex:SetTexture(ICON_TEX .. i)
        ib:SetHighlightTexture("Interface\\Buttons\\ButtonHilight-Square", "ADD")
        ib:RegisterForClicks("LeftButtonUp", "RightButtonUp")
        ib:SetScript("OnClick", function(_, button)
            if not botName then return end
            if button == "RightButton" then
                BotWhisper("assign clear " .. ICON_WORDS[i])
            else
                local role = ROLE_CYCLE[botRoleChoice]
                BotWhisper("assign " .. ICON_WORDS[i] .. (role ~= "auto" and (" " .. role) or ""))
                botRoleChoice = 1
                botRoleBtn:SetText(L["Role: %s"]:format(L["auto"]))
            end
        end)
        ib:SetScript("OnEnter", function(self)
            GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
            GameTooltip:SetText(MarkerName(ICON_WORDS[i]))
            GameTooltip:AddLine(L["Click: this bot takes this icon"], 1, 1, 1)
            GameTooltip:AddLine(L["Right-click: clear it"], 1, 1, 1)
            GameTooltip:Show()
        end)
        ib:SetScript("OnLeave", function() GameTooltip:Hide() end)
    end

    local function B(text, w, x, y, fn)
        local b = MakeButton(p, text, w, fn)
        b:SetPoint("TOPLEFT", x, y)
        return b
    end
    B(L["short:Follow"], 72, 12, -58, function() BotWhisper("follow") end)
    B(L["Stay"], 72, 87, -58, function() BotWhisper("stay") end)
    B(L["Attack"], 72, 162, -58, function() BotWhisper("attack") end)
    B(L["short:Pull back"], 72, 12, -84, function() BotWhisper("pull back") end)
    B("-", 24, 92, -84, function() BotStepDistance(-1) end)
    botDistFS = p:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    botDistFS:SetPoint("TOPLEFT", 118, -89)
    botDistFS:SetWidth(44)
    botDistFS:SetJustifyH("CENTER")
    B("+", 24, 166, -84, function() BotStepDistance(1) end)
    botRoleBtn = B(L["Role: %s"]:format(L["auto"]), 84, 12, -110, function()
        botRoleChoice = botRoleChoice % #ROLE_CYCLE + 1
        botRoleBtn:SetText(L["Role: %s"]:format(L[ROLE_CYCLE[botRoleChoice]]))
    end)
    B(L["Dismiss"], 72, 102, -110, function() if botName then ServerCommand(".bot dismiss " .. botName) end end)
    -- This bot's stance, three steps like the window's: Defensive, Aggressive, Passive (attacks nothing).
    local botStanceStep = 1
    local botStanceBtn
    botStanceBtn = B(L["short:Defensive"], 56, 177, -110, function()
        botStanceStep = botStanceStep % 3 + 1
        BotWhisper(botStanceStep == 1 and "defensive" or botStanceStep == 2 and "aggressive" or "passive")
        botStanceBtn:SetText(botStanceStep == 1 and L["short:Defensive"] or botStanceStep == 2 and L["Aggress."] or L["short:Passive"])
    end)
    botStatusFS = p:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    botStatusFS:SetPoint("TOPLEFT", 14, -136)
    botStatusFS:SetWidth(208)
    botStatusFS:SetJustifyH("LEFT")
    p:SetHeight(158)

    p:SetScript("OnUpdate", function(self, elapsed)
        self.acc = (self.acc or 0) + elapsed
        if self.acc < 0.1 then return end
        self.acc = 0
        local now = Now()
        if botDistDirtyAt and now - botDistDirtyAt >= 0.4 and botName then
            botDistDirtyAt = nil
            Auto(BotWhisper, "distance " .. botDist)
        end
        if botSentAt and now - botSentAt > 4 then
            botSentAt = nil
            SetBotStatus("|cffff4040" .. L["No answer: is %s your bot?"]:format(botName or L["it"]) .. "|r")
        end
    end)
    p:Hide()
end

local botFrame = CreateFrame("Frame")
botFrame:RegisterEvent("PLAYER_TARGET_CHANGED")
botFrame:RegisterEvent("CHAT_MSG_WHISPER")
for _, ev in ipairs({ "GROUP_ROSTER_UPDATE", "PARTY_MEMBERS_CHANGED", "RAID_ROSTER_UPDATE" }) do
    pcall(botFrame.RegisterEvent, botFrame, ev)
end
botFrame:SetScript("OnEvent", function(_, event, msg, author)
    if event == "CHAT_MSG_WHISPER" then
        -- In a battleground the client hands this handler secret strings for chat: nothing below may touch them (a match,
        -- compare or concatenation raised "attempt to perform string conversion on a secret string value" in WSG).
        if type(author) ~= "string" or type(msg) ~= "string" or IsSecret(author) or IsSecret(msg) then return end
        local who = strmatch(author, "^[^-]+")
        if not who or IsAssignMsg(msg) or not botName or who ~= botName then return end
        local confirm = ConfirmText(msg, author)
        knownBots[who] = true
        botSentAt = nil
        SetBotStatus("|cff00ff00" .. (confirm or msg) .. "|r")
        UpdateBotPanel()
        return
    end
    if event == "PLAYER_TARGET_CHANGED" then botForced = nil end
    if not botPanel then BuildBotPanel() end
    UpdateBotPanel()
end)

-- Unit right-click menu entry "ForeverBots": only through the client's own Menu.ModifyMenu API (a supported, taint-free
-- extension point on the 11.x-style UI). The old UnitPopupMenus / UnitPopup_ShowMenu tables are NOT touched: editing
-- them taints the protected popup buttons (Set Focus, Target...), so on a client without Menu.ModifyMenu this is skipped
-- and the floating panel (target-driven) is the way in.
-- Opens the client's chat box already in whisper mode for <name> (no slash text, so the client's slash parser is not involved:
-- on client 70291 in a battleground every slash line errors inside Blizzard's HandleChatType, "table cannot be indexed with
-- secret keys", playtest 2026-10-09). Enter then sends an ordinary whisper packet. It touches the chat box, so it is called ONLY
-- straight from a click handler (the portrait menu's "Whisper (Forever Bots)"): never from a timer, an event or a hook on the
-- client's own functions, where the client blocks it ("ForeverBots has been blocked from an action only available to the
-- Blizzard UI", reported 2026-10-15). Anything that is not a click whispers with SendChatMessage / an addon whisper instead.
function OpenWhisperBox(name, text)
    if not (ChatEdit_ChooseBoxForSend and ChatEdit_ActivateChat and ChatEdit_UpdateHeader) then
        Print(L["type /w %s <message> to whisper"]:format(name))
        return
    end
    local eb = ChatEdit_ChooseBoxForSend()
    eb:SetAttribute("chatType", "WHISPER")
    eb:SetAttribute("tellTarget", name)
    ChatEdit_UpdateHeader(eb)
    ChatEdit_ActivateChat(eb)
    eb:SetText(text or "")
end

-- The client's own Whisper (name click, friends list, portrait Interact menu) opens the box with "/cw Name"; fixing that used
-- to hook ChatFrameUtil.SendTellWithMessage, ChatFrame_OpenChat, ChatEdit_ActivateChat and the edit boxes' SetText and rewrite
-- the box from those callbacks. That is chat-box access without a player action and is what the client blocks
-- (2026-10-15), so the hooks are gone. The portrait menu's "Whisper (Forever Bots)" and typed /w are the supported ways.

-- The bot submenu of the unit right-click menu (a group member): orders and the bot's own picks, each a whisper to that bot.
-- Class entries appear from the level the spell is trained at; the server answers "I don't know ... yet" for one the bot lacks
-- (a talent such as Kings or Sanctity). The picks are the server's "prefer <slot> <name>" (memory only there, and here), so the
-- marks below show what was sent in this session.
local MARKERS = { "star", "circle", "diamond", "triangle", "moon", "square", "cross", "skull" }
-- slot word, menu title, class token, { word, label, level, spell id } (word = the server's first word for the choice,
-- Bot/OwnerPicks.h). The spell id only names the entry on a client that is not English (SpellLabel); ids checked in SpellName.db2.
local PICKS = {
    { "blessing", "Blessing", "PALADIN", {
        { "might", "Blessing of Might", 4, 19740 }, { "wisdom", "Blessing of Wisdom", 14, 19742 }, { "kings", "Blessing of Kings", 20, 20217 },
        { "salvation", "Blessing of Salvation", 26, 1038 }, { "light", "Blessing of Light", 40, 19977 }, { "sanctuary", "Blessing of Sanctuary", 30 } } },
    { "aura", "Aura", "PALADIN", {
        { "devotion", "Devotion Aura", 1, 465 }, { "retribution", "Retribution Aura", 16, 7294 }, { "concentration", "Concentration Aura", 22, 19746 },
        { "shadow", "Shadow Resistance Aura", 28, 19876 }, { "frost", "Frost Resistance Aura", 32, 19888 }, { "fire", "Fire Resistance Aura", 36, 19891 },
        { "sanctity", "Sanctity Aura", 20, 20218 } } },
    { "earth", "Earth totem", "SHAMAN", {
        { "stoneskin", "Stoneskin Totem", 4, 8071 }, { "earthbind", "Earthbind Totem", 6, 2076 }, { "stoneclaw", "Stoneclaw Totem", 8, 5730 },
        { "strength", "Strength of Earth Totem", 10, 8075 }, { "tremor", "Tremor Totem", 18, 8143 } } },
    { "fire", "Fire totem", "SHAMAN", {
        { "searing", "Searing Totem", 10, 2075 }, { "nova", "Fire Nova Totem", 12, 27623 }, { "frost", "Frost Resistance Totem", 24, 8181 },
        { "magma", "Magma Totem", 26, 8187 }, { "flametongue", "Flametongue Totem", 28, 8227 } } },
    { "water", "Water totem", "SHAMAN", {
        { "healing", "Healing Stream Totem", 20, 5394 }, { "poison", "Poison Cleansing Totem", 22, 8166 }, { "mana", "Mana Spring Totem", 26, 5675 },
        { "fire", "Fire Resistance Totem", 28, 8184 }, { "disease", "Disease Cleansing Totem", 38, 8170 } } },
    { "air", "Air totem", "SHAMAN", {
        { "grounding", "Grounding Totem", 30, 8177 }, { "nature", "Nature Resistance Totem", 30, 10595 }, { "windfury", "Windfury Totem", 32, 8512 },
        { "windwall", "Windwall Totem", 36, 15107 }, { "grace", "Grace of Air Totem", 42, 8835 }, { "tranquil", "Tranquil Air Totem", 50 } } },
    { "aspect", "Aspect", "HUNTER", {
        { "monkey", "Aspect of the Monkey", 4, 13163 }, { "hawk", "Aspect of the Hawk", 10, 13165 }, { "cheetah", "Aspect of the Cheetah (out of combat)", 20, 5118 },
        { "pack", "Aspect of the Pack (out of combat)", 30, 13159 }, { "wild", "Aspect of the Wild", 46, 20043 } } },
    { "demon", "Demon", "WARLOCK", {
        { "imp", "Imp", 1 }, { "voidwalker", "Voidwalker", 10 }, { "succubus", "Succubus", 20 }, { "felhunter", "Felhunter", 30 } } },
}
local PORTALS = {
    Alliance = { { "stormwind", "Stormwind" }, { "ironforge", "Ironforge" }, { "darnassus", "Darnassus" } },
    Horde = { { "orgrimmar", "Orgrimmar" }, { "undercity", "Undercity" }, { "thunder bluff", "Thunder Bluff" } },
}
-- Crowd control the owner can say no to for a whole class (the server's "@<class> no <kind>", CcDeny.h): kind word, label,
-- spell ids (as in PICKS).
local DANGER = {
    WARRIOR = { { "fear", "Intimidating Shout", 5246 } },
    PRIEST  = { { "fear", "Psychic Scream", 8122 }, { "shackle", "Shackle Undead", 1425 } },
    WARLOCK = { { "fear", "Fear, Howl of Terror, Death Coil", { 663, 5484, 6789 } }, { "banish", "Banish", 710 }, { "seduce", "Seduction, Enslave Demon", { 6358, 1098 } } },
    MAGE    = { { "sheep", "Polymorph", 118 } },
    HUNTER  = { { "fear", "Scare Beast", 1513 }, { "trap", "Freezing Trap", 1499 }, { "sleep", "Wyvern Sting", 24335 } },
    ROGUE   = { { "sap", "Sap", 2070 } },
    DRUID   = { { "sleep", "Hibernate", 2637 } },
    PALADIN = { { "fear", "Turn Undead", 2878 } },
}
-- The menu name of a spell choice: on a client that is not English, the client's own names for its spell ids (joined with ", ",
-- and the "out of combat" note kept); otherwise, or when the client does not know an id, L.
local GetSpellName = (C_Spell and C_Spell.GetSpellName) or GetSpellInfo
local function SpellLabel(label, ids)
    if CLIENT_NAMES and ids and GetSpellName then
        local names = {}
        for _, id in ipairs(type(ids) == "table" and ids or { ids }) do
            local ok, n = pcall(GetSpellName, id)
            if not (ok and type(n) == "string" and n ~= "") then names = nil; break end
            names[#names + 1] = n
        end
        if names then
            return table.concat(names, ", ") .. (label:find(" (out of combat)", 1, true) and L[" (out of combat)"] or "")
        end
    end
    return L[label]
end
local sentPicks = {}   -- [bot name][slot word] = choice word, this session

local function MenuWhisper(who, text)
    Cmd.NoteOwnWhisper(who, text)
    local ok, err = pcall(SendChatMessage, text, "WHISPER", nil, who)
    if not ok then Print(L["could not whisper %s: %s"]:format(who, tostring(err))) end
end

-- The group members by name (yourself first), for the summon and soulstone lists.
local function GroupNames()
    local names = {}
    local me = SafeName("player")
    if me then names[#names + 1] = me end
    local raid = IsInRaid and IsInRaid()
    local n = (GetNumGroupMembers and GetNumGroupMembers()) or 0
    for i = 1, raid and n or math.max(0, n - 1) do
        local nm = SafeName((raid and "raid" or "party") .. i)
        if nm and nm ~= me then names[#names + 1] = strtrim((strsplit("-", nm))) end
    end
    return names
end

local function AddBotMenu(root, unit, who)
    local token = select(2, UnitClass(unit))
    local level = UnitLevel(unit) or 0
    local function Send(text) MenuWhisper(who, text) end
    local function Mark(on) return on and "|cff40ff40>|r " or "" end
    local menu = root:CreateButton("WoW Forever Bots")
    menu:CreateButton(L["Open the bot panel"], function()
        botForced = { name = who, token = token }
        if not botPanel then BuildBotPanel() end
        UpdateBotPanel()
    end)
    menu:CreateButton(L["Follow me"], function() Send("follow") end)
    menu:CreateButton(L["Stay here"], function() Send("stay") end)
    menu:CreateButton(L["Attack my target"], function() Send("attack") end)
    local mineAoe = sentPicks[who] and sentPicks[who].aoe
    local aoe = menu:CreateButton(L["Area spells"])
    aoe:CreateButton(Mark(mineAoe ~= "off") .. L["On"], function()
        sentPicks[who] = sentPicks[who] or {}; sentPicks[who].aoe = nil; Send("aoe on")
    end)
    aoe:CreateButton(Mark(mineAoe == "off") .. L["Off: nothing that hits more than one enemy"], function()
        sentPicks[who] = sentPicks[who] or {}; sentPicks[who].aoe = "off"; Send("aoe off")
    end)

    -- Role: the roles of the class (CLASSES), and auto (the spec decides again).
    for _, c in ipairs(CLASSES) do
        if c.token == token and #c.roles > 1 then
            local roleMenu = menu:CreateButton(L["Role"])
            for _, r in ipairs(c.roles) do
                roleMenu:CreateButton(ROLE_LABEL[r] or r, function() Send("setrole " .. r) end)
            end
            roleMenu:CreateButton(L["Auto (by spec)"], function() Send("setrole clear") end)
        end
    end

    -- Raid icons: the bot follows the icon you put on a mob (the server's "assign <marker> [kill|cc|tank]").
    -- One list, a title per job: a submenu per job made this menu four levels deep, and where the screen has no room on the
    -- right the client opens each level on the other side, so the fourth level lay over the bot menu and the third over the
    -- unit menu, the text of both mixed (Tornn, 2026-10-29). Every bot submenu is now one level below the bot menu.
    local icons = menu:CreateButton(L["Raid icon"])
    local function IconList(title, role)
        local titled = icons.CreateTitle and pcall(icons.CreateTitle, icons, title)
        for _, m in ipairs(MARKERS) do
            local label = titled and MarkerName(m) or (title .. ": " .. MarkerName(m))
            icons:CreateButton(label, function() Send("assign " .. m .. (role and (" " .. role) or "")) end)
        end
        icons:CreateDivider()
    end
    IconList(L["Kill it first"], "kill")
    if token ~= "WARRIOR" and token ~= "SHAMAN" then IconList(L["Crowd-control it"], "cc") end
    if token == "WARRIOR" or token == "PALADIN" or token == "DRUID" then IconList(L["Tank it"], "tank") end
    icons:CreateButton(L["Clear"], function() Send("assign clear " .. who) end)

    -- The bot's own picks for its class.
    local mine = sentPicks[who] or {}
    for _, slot in ipairs(PICKS) do
        if slot[3] == token then
            local sub
            for _, ch in ipairs(slot[4]) do
                if level >= ch[3] then
                    sub = sub or menu:CreateButton(L[slot[2]])
                    sub:CreateButton(Mark(mine[slot[1]] == ch[1]) .. SpellLabel(ch[2], ch[4]), function()
                        sentPicks[who] = sentPicks[who] or {}
                        sentPicks[who][slot[1]] = ch[1]
                        Send("prefer " .. slot[1] .. " " .. ch[1])
                    end)
                end
            end
            if sub then
                sub:CreateButton(Mark(mine[slot[1]] == nil) .. L["Auto (the group plan)"], function()
                    if sentPicks[who] then sentPicks[who][slot[1]] = nil end
                    Send("prefer " .. slot[1] .. " auto")
                end)
            end
        end
    end
    if token == "HUNTER" or token == "WARLOCK" then
        local pet = menu:CreateButton(L["Pet"])
        pet:CreateButton(Mark(mine.pet ~= "off") .. L["Keep the pet out"], function()
            sentPicks[who] = sentPicks[who] or {}; sentPicks[who].pet = nil; Send("pet on")
        end)
        pet:CreateButton(Mark(mine.pet == "off") .. L["Dismiss it and keep it away"], function()
            sentPicks[who] = sentPicks[who] or {}; sentPicks[who].pet = "off"; Send("pet off")
        end)
    end
    if token == "ROGUE" then
        local st = menu:CreateButton(L["Stealth"])
        st:CreateButton(Mark(mine.stealth ~= "off") .. L["Open from Stealth"], function()
            sentPicks[who] = sentPicks[who] or {}; sentPicks[who].stealth = nil; Send("stealth on")
        end)
        st:CreateButton(Mark(mine.stealth == "off") .. L["No Stealth (and no Sap)"], function()
            sentPicks[who] = sentPicks[who] or {}; sentPicks[who].stealth = "off"; Send("stealth off")
        end)
    end
    if token == "WARLOCK" and level >= 20 then
        local sm = menu:CreateButton(L["Summon a player"])
        for _, nm in ipairs(GroupNames()) do
            if nm ~= who then sm:CreateButton(nm, function() Send("ritual " .. nm) end) end
        end
        sm:CreateButton(L["Help click the portal (party)"], function() GroupOrder("use summoning") end)
    end
    if token == "WARLOCK" and level >= 18 then
        local ss = menu:CreateButton(L["Soulstone"])
        for _, nm in ipairs(GroupNames()) do
            ss:CreateButton(nm, function() Send("soulstone " .. nm) end)
        end
    end
    -- Mage: conjured water and food are a craft order; the mage asks you to open a trade with it and hands them over.
    if token == "MAGE" and level >= 4 then
        menu:CreateButton(L["Conjure water"], function() Send("craft water x20") end)
        if level >= 6 then menu:CreateButton(L["Conjure food"], function() Send("craft food x20") end) end
    end
    if token == "MAGE" and level >= 40 then
        local ok, faction = pcall(UnitFactionGroup, unit)
        local list = ok and PORTALS[faction]
        if list then
            local pm = menu:CreateButton(L["Open a portal"])
            for _, c in ipairs(list) do pm:CreateButton(L[c[2]], function() Send("portal " .. c[1]) end) end
        end
    end

    -- No danger spells: the owner's crowd-control orders for the whole class (every bot of it, and ones that join later).
    local danger = DANGER[token]
    if danger then
        local cls = strlower(token)
        local native = CLIENT_NAMES and LOCALIZED_CLASS_NAMES_MALE and LOCALIZED_CLASS_NAMES_MALE[token]
        local dm = menu:CreateButton(native and L["Danger spells (class: %s)"]:format(native) or ("Danger spells (all " .. cls .. "s)"))
        for _, d in ipairs(danger) do
            dm:CreateButton(L["No %s"]:format(SpellLabel(d[2], d[3])), function() Send("@" .. cls .. " no " .. d[1]) end)
        end
        dm:CreateButton(L["No crowd control at all"], function() Send("@" .. cls .. " off cc") end)
        dm:CreateButton(L["Allow all again"], function() Send("@" .. cls .. " on cc") end)
    end
    menu:CreateButton(L["What did I pick?"], function() Send("prefer") end)
    menu:CreateButton(L["Clear my picks"], function()
        sentPicks[who] = { aoe = sentPicks[who] and sentPicks[who].aoe }   -- the area-spell switch is not a pick
        Send("prefer clear")
    end)
end

local function AddPopupEntry()
    Step("portrait menu entries")
    if not (Menu and Menu.ModifyMenu) then return end
    -- /fb menu off (then /reload) leaves the client's unit menus completely untouched: a menu we modified can taint the client's
    -- own menu actions ("ForeverBots has been blocked from an action only available to the Blizzard UI", reported 2026-10-15).
    if ForeverBotsDB.menuEntries == false or not PARTS.On("menu") then return end
    -- Entries through the client's supported Menu.ModifyMenu only (no UnitPopup tables, no hooks). The client's own portrait
    -- Invite sends nothing for a bot on 70338 (playtest 2026-10-11: no CMSG_PARTY_INVITE from the menu, typed /invite works), so
    -- "Whisper (Forever Bots)" is added for every friendly player, "Invite (Forever Bots)" for one outside my group, and the
    -- bot-panel entry for a group member (world, party, raid and battleground menus alike).
    -- Right-clicking the target frame crashed the client once with entries of ours in MENU_UNIT_PLAYER (Menu.lua:1103,
    -- ldebug.c:747, 2026-10-11 16:10; cause not found), so an entry is added only after every value is checked to be a plain,
    -- non-secret one, a unit that is not an enemy, and the whole callback runs under pcall: nothing is added for an enemy or
    -- a unit whose name or token is secret (combat, battleground).
    local function modify(_, root, ctx)
        Step("portrait menu")
        local ok = pcall(function()
            if type(ctx) ~= "table" then return end
            local name, unit = ctx.name, ctx.unit
            if type(name) ~= "string" or IsSecret(name) or type(unit) ~= "string" or IsSecret(unit) then return end
            if name == "" or name == SafeName("player") then return end
            if not (UnitExists(unit) and UnitIsPlayer(unit) and UnitIsFriend("player", unit)) or UnitIsUnit(unit, "player") then return end
            local who = strtrim((strsplit("-", name)))
            if who == "" then return end
            local grouped = (UnitInParty and UnitInParty(unit)) or (UnitInRaid and UnitInRaid(unit))
            root:CreateDivider()
            root:CreateButton(L["Whisper (Forever Bots)"], function() OpenWhisperBox(who) end)
            if grouped then
                AddBotMenu(root, unit, who)
            else
                -- The client's own Invite entry sends nothing on this build: point it at the working call instead of
                -- leaving a dead entry next to ours. Our own entry stays when the native one is not found.
                local replaced = false
                if root.EnumerateElementDescriptions then
                    for _, desc in root:EnumerateElementDescriptions() do
                        pcall(function()
                            local text = desc.GetText and desc:GetText()
                            if type(text) == "string" and not IsSecret(text) and (text == PARTY_INVITE or text == INVITE or text == "Invite")
                                and desc.SetResponder then
                                desc:SetResponder(function() InviteByName(who) end)
                                replaced = true
                            end
                        end)
                    end
                end
                if not replaced then
                    root:CreateButton(L["Invite (Forever Bots)"], function() InviteByName(who) end)
                end
            end
        end)
        if not ok then Debug("unit menu entry skipped") end
    end
    modify = PerfWrap("menu", modify)
    for _, tag in ipairs({ "MENU_UNIT_PARTY", "MENU_UNIT_RAID_PLAYER", "MENU_UNIT_RAID", "MENU_UNIT_PLAYER", "MENU_UNIT_TARGET", "MENU_UNIT_FOCUS" }) do
        pcall(Menu.ModifyMenu, tag, modify)
    end
end

------------------------------------------------------------------------------------------------------------- slash

-- Typed whispers: on this client "/w Name text" and the unit menu's "/cw Name text" do nothing (playtest 2026-10-08: no packet
-- reaches the server, no Lua error, also with every addon off), while SendChatMessage(text, "WHISPER", nil, name) works.
-- So /w /t /tell /cw are defined here as a plain SendChatMessage whisper (the client's own /whisper and /r stay native: they work).
-- Restored 2026-10-11: in a raid (Molten Core) typed "/w Cal hi" did nothing while "/whisper Cal hi" worked, and the box filled by
-- clicking a name carries "/cw Name". The client's own SLASH_SMART_WHISPER1 /
-- SLASH_WHISPER1 globals stay untouched (the unit menu's Whisper builds its "/cw Name " text from them; clearing them broke
-- it with "bad argument #2 to 'format'"); they have no SlashCmdList handler, so they do not compete with ours. The native
-- path has no handler in 70205, so the whisper is sent directly.
do
    local function InPvP()
        local ok, inInstance, kind = pcall(IsInInstance)
        return ok and inInstance and (kind == "pvp" or kind == "arena")
    end

    -- Inside a BG the whisper goes out as an addon whisper (FBOTS) that the server hands to the bot. Remembers the last one so
    -- the Enter-key path below never sends the same line twice.
    local lastBg = { at = 0 }
    local function BgWhisper(name, text)
        if ForeverBotsDB.bgWhisper == "dot" or not prefixOk then return false end
        local send = C_ChatInfo and C_ChatInfo.SendAddonMessage or SendAddonMessage
        local ok, res = pcall(send, ORDER_PREFIX, text, "WHISPER", name)
        if not (ok and (res == nil or res == true or res == 0 or (Enum and Enum.SendAddonMessageResult and res == Enum.SendAddonMessageResult.Success))) then
            return false
        end
        lastBg.name, lastBg.text, lastBg.at = name, text, GetTime()
        DEFAULT_CHAT_FRAME:AddMessage("|cffff80ff" .. L["To %s: %s"]:format(name, text) .. "|r")
        return true
    end

    local function Whisper(msg)
        msg = SafeStr(msg) or ""
        -- the unit menu can leave non-breaking / zero-width blanks after the name (the plain-space trim does not see them)
        msg = msg:gsub("\194\160", " "):gsub("\226\128[\128-\143]", "")
        local name, text = strmatch(msg, "^%s*(%S+)%s+(.-)%s*$")
        if not name then name = strmatch(msg, "^%s*(%S+)%s*$"); text = nil end
        if not name then
            Print(L["/w <name> <message> whispers a player"])
            return
        end
        name = strtrim((strsplit("-", name)))   -- "Name-Realm" -> "Name": a single realm
        if name == "" then return end
        if InPvP() then
            -- The client's own /w and /whisper do nothing in a battleground (70338: the box stays in Say mode). The typed line is
            -- sent as an addon whisper under the FBOTS prefix, which the server hands to the bot like a whisper (the bot answers
            -- as usual); it needs no second step.
            if text and BgWhisper(name, text) then return end
            -- no chat box from a slash handler: the portrait menu's "Whisper (Forever Bots)" opens it from a click
            Print(L["battleground: the whisper to %s could not be sent%s; right-click %s and choose Whisper (Forever Bots)"]:format(name,
                text and "" or L[" (type /w %s <message>)"]:format(name), name))
            return
        end
        if not text then
            Print(L["type /w %s <message> to whisper"]:format(name))
            return
        end
        -- Sent straight from the slash handler (no timer, no hook on SendChatMessage): the client's own /w never works in 70205.
        local ok, err = pcall(SendChatMessage, text, "WHISPER", nil, name)
        if not ok then
            Print(L["whisper to %s failed: %s"]:format(name, tostring(err)))
            return
        end
    end

    -- The client's own Whisper (name click, friends list, portrait Interact > Whisper) leaves "/cw Name " in the chat box. Outside a
    -- battleground pressing Enter on it reaches the /cw handler above. In a battleground the client's slash parser fails on such a
    -- line before any handler runs, so the whisper is lost. For that case the edit boxes' OnEnterPressed gets a post-hook that
    -- only READS the text (kept from OnTextChanged, since the client clears the box when it sends) and, for "/cw Name message"
    -- typed in a BG, sends the addon whisper during that same key press (a player action). Nothing writes to the box, no ChatEdit_
    -- function is called, no client function is replaced.
    local seen = {}   -- [editbox] = { text = last non-empty text, at = time }
    local function HookBox(box)
        if not (box and box.HookScript) then return end
        -- All three hooks do nothing outside a battleground: the secure order buttons run their "/party ..." line through this same
        -- edit box, so our code stays out of that path everywhere the bg-whisper workaround is not needed.
        pcall(box.HookScript, box, "OnTextChanged", function(self)
            if not InPvP() then return end
            local ok, text = pcall(self.GetText, self)
            text = ok and SafeStr(text) or nil
            if text and text ~= "" then seen[self] = { text = text, at = GetTime() } end
        end)
        pcall(box.HookScript, box, "OnEscapePressed", function(self) if InPvP() then seen[self] = nil end end)
        pcall(box.HookScript, box, "OnEnterPressed", function(self)
            if not InPvP() then return end
            local rec = seen[self]
            seen[self] = nil
            if not rec or GetTime() - rec.at > 30 then return end
            local name, text = strmatch(rec.text, "^/[cC][wW]%s+(%S+)%s+(.-)%s*$")
            if not name or text == "" then return end
            name = strtrim((strsplit("-", name)))
            if name == "" then return end
            if lastBg.name == name and lastBg.text == text and GetTime() - lastBg.at < 2 then return end   -- the handler already sent it
            BgWhisper(name, text)
        end)
    end
    for i = 1, (NUM_CHAT_WINDOWS or 10) do HookBox(_G["ChatFrame" .. i .. "EditBox"]) end

    for i, word in ipairs({ "/w", "/t", "/tell", "/cw", "/whisper" }) do
        _G["SLASH_FOREVERBOTSW" .. i .. "1"] = word
        SlashCmdList["FOREVERBOTSW" .. i] = Whisper
    end
end

SLASH_FOREVERBOTS1 = "/fb"
SLASH_FOREVERBOTS2 = "/foreverbots"
SlashCmdList.FOREVERBOTS = function(msg)
    local verb, class, role, extra = strsplit(" ", strlower(strtrim(msg or "")))
    verb = verb and verb:gsub("[%p]+$", "")   -- "/fb perf." (dictated) is still perf
    if verb == "chat" then
        if class == "on" then ForeverBotsDB.showChat = true
        elseif class == "off" then ForeverBotsDB.showChat = false
        else ForeverBotsDB.showChat = not ForeverBotsDB.showChat end
        Print(L["bot confirmations in chat: %s"]:format(ForeverBotsDB.showChat and L["shown"] or L["hidden (shown in the Bots panel status line)"]) .. "  (/fb chat on|off)")
        return
    end
    if verb == "bots" or verb == "manage" or verb == "roster" then
        if Cmd.ns.Windows then Cmd.ns.Windows.Toggle() else Print(L["%s is missing: reinstall the add-on."]:format("ForeverBots_Windows.lua")) end
        return
    end
    if verb == "ping" then
        if Cmd.ns.Net then Cmd.ns.Net.Ping() else Print(L["%s is missing: reinstall the add-on."]:format("ForeverBots_Net.lua")) end
        return
    end
    if verb == "minimap" then
        if class == "reset" then ForeverBotsDB.minimap = { angle = 215 }; BuildMinimapButton(); MinimapPlace(); SetMinimapShown(true)
        elseif class == "off" then SetMinimapShown(false)
        elseif class == "on" then SetMinimapShown(true)
        else SetMinimapShown(ForeverBotsDB.minimap == nil or ForeverBotsDB.minimap.hidden == true) end
        Print(L["minimap button: %s"]:format((ForeverBotsDB.minimap and ForeverBotsDB.minimap.hidden) and L["hidden"] or L["shown"]) .. "  (/fb minimap on|off|reset)")
        return
    end
    if verb == "scale" then
        local n = tonumber(class)
        if n then
            ForeverBotsDB.scale = math.max(SCALE_MIN, math.min(SCALE_MAX, n))
            if frame and UI.Locked(frame) then UI.scaleAfterCombat = true elseif frame then frame:SetScale(WindowScale()) end
        end
        Print(L["window size: %.2f  (/fb scale %s-%s, default %s)"]:format(WindowScale(), SCALE_MIN, SCALE_MAX, SCALE_DEFAULT))
        return
    end
    if verb == "perf" then
        if class == "reset" then
            for _, st in pairs(PERF.stats) do st.n, st.total, st.max = 0, 0, 0 end
            PERF.said = {}
            Print("handler timings cleared.")
            return
        end
        local lines = PERF.Report(12)
        Print("slowest handlers since login (/fb perf reset clears; saved at logout):" .. (#lines == 0 and " none yet" or ""))
        for _, l in ipairs(lines) do Print("  " .. l) end
        return
    end
    if verb == "part" then
        local known = {}
        for _, part in ipairs(PARTS.list) do known[part] = true end
        if known[class] and (role == "on" or role == "off") then
            ForeverBotsDB.partsOff = ForeverBotsDB.partsOff or {}
            ForeverBotsDB.partsOff[class] = (role == "off") or nil
            Print(class .. " " .. role .. " after /reload.")
            return
        end
        local states = {}
        for _, part in ipairs(PARTS.list) do states[#states + 1] = part .. " " .. (PARTS.On(part) and "on" or "off") end
        Print("parts: " .. table.concat(states, ", ") .. "  (/fb part <name> on|off, then /reload)")
        return
    end
    if verb == "debug" then
        if class == "on" then debugOn = true
        elseif class == "off" then debugOn = false
        else debugOn = not debugOn end
        Print("debug output: " .. (debugOn and "on (until you log out or /reload)" or "off") .. "  (/fb debug on|off)")
        return
    end
    if verb == "tabicons" then
        for i, t in ipairs(frameTabs) do
            Print("tab " .. i .. " icon = " .. tostring(t.fbIcon and t.fbIcon:GetTexture()))
        end
        return
    end
    if verb == "chatdiag" then
        -- Read-only dump of what the client thinks about chat, for finding why its own /w handler is missing (doc/whisper-feature-bisect.md)
        local function show(label, fn, ...)
            local ok, v = pcall(fn, ...)
            if not ok then v = "error: " .. tostring(v) elseif IsSecret(v) then v = "<secret>" end
            Print("diag " .. label .. " = " .. tostring(v))
        end
        local _, _, _, tocv = GetBuildInfo()
        Print("diag build " .. tostring(select(2, GetBuildInfo())) .. " toc " .. tostring(tocv))
        show("SlashCmdList.SMART_WHISPER", function() return type(SlashCmdList.SMART_WHISPER) end)
        show("SlashCmdList.WHISPER", function() return type(SlashCmdList.WHISPER) end)
        show("SlashCmdList.REPLY", function() return type(SlashCmdList.REPLY) end)
        show("SLASH_SMART_WHISPER1", function() return SLASH_SMART_WHISPER1 end)
        show("SLASH_WHISPER1", function() return SLASH_WHISPER1 end)
        show("hash_SlashCmdList[/w]", function() return type(hash_SlashCmdList and hash_SlashCmdList["/W"]) end)
        show("ChatTypeInfo.WHISPER", function() return type(ChatTypeInfo and ChatTypeInfo.WHISPER) end)
        show("RegionalUniqueNamesEnabled()", function() return RegionalUniqueNamesEnabled and tostring(RegionalUniqueNamesEnabled()) or "missing" end)
        show("autocomplete results for 'Name '", function()
            local n = UnitName("target")
            if not (n and C_AutoComplete and C_AutoComplete.GetAutoCompleteResults) then return "no target / api" end
            return tostring(#C_AutoComplete.GetAutoCompleteResults(n .. " ", 1, 0, true)) .. " for '" .. n .. " ', " ..
                tostring(#C_AutoComplete.GetAutoCompleteResults(n, 1, 0, true)) .. " for '" .. n .. "'"
        end)
        show("hash_ChatTypeInfoList[/cw]", function() return hash_ChatTypeInfoList and tostring(hash_ChatTypeInfoList["/CW"]) end)
        show("hash_ChatTypeInfoList[/r]", function() return hash_ChatTypeInfoList and tostring(hash_ChatTypeInfoList["/R"]) end)
        show("hash_ChatTypeInfoList[/w]", function() return hash_ChatTypeInfoList and tostring(hash_ChatTypeInfoList["/W"]) end)
        show("IsInInstance", function() local i, k = IsInInstance(); return tostring(i) .. " " .. tostring(k) end)
        if C_SocialRestrictions then
            for _, fn in ipairs({ "IsChatDisabled", "IsMuted", "IsSilenced", "IsSquelched", "IsRestrictedAccount", "IsRestricted" }) do
                if C_SocialRestrictions[fn] then show("C_SocialRestrictions." .. fn, C_SocialRestrictions[fn]) end
            end
        else
            Print("diag C_SocialRestrictions = nil")
        end
        if C_ChatInfo then
            for _, fn in ipairs({ "InChatMessagingLockdown", "CanPlayerSpeak", "IsChatLockdownActive", "IsAddonChatRestricted" }) do
                if C_ChatInfo[fn] then show("C_ChatInfo." .. fn, C_ChatInfo[fn]) end
            end
        end
        if C_Secrets then
            for _, fn in ipairs({ "HasSecretRestrictions", "ShouldCooldownsBeSecret" }) do
                if C_Secrets[fn] then show("C_Secrets." .. fn, C_Secrets[fn]) end
            end
        end
        show("ChatFrame1EditBox text secret", function() return IsSecret(ChatFrame1EditBox and ChatFrame1EditBox:GetText()) end)
        -- the client's unit-menu Whisper/Invite refuse units for which UnitIsHumanPlayer is false (unitpopupsharedbuttonmixins.lua:469/483)
        for _, fn in ipairs({ "UnitIsHumanPlayer", "UnitIsPlayer", "UnitIsConnected", "UnitIsFriend" }) do
            if _G[fn] then show(fn .. '("target")', _G[fn], "target") end
        end
        show("SendChatMessage", function() return type(SendChatMessage) end)
        show("C_ChatInfo.SendChatMessage", function() return type(C_ChatInfo and C_ChatInfo.SendChatMessage) end)
        return
    end
    if verb == "bagdebug" then
        -- Read-only: the texture regions of one empty and one filled bag slot (open the bags first), to find which region holds the
        -- empty-slot frame that client 70685's bag code replaces (see EMPTY_BAG_SLOT_EFFECT_COLOR at the top of this file)
        local frame = (ContainerFrameCombinedBags and ContainerFrameCombinedBags:IsShown() and ContainerFrameCombinedBags) or ContainerFrame1
        if not (frame and frame:IsShown() and frame.EnumerateValidItems) then Print("bagdebug: open your bags first, then type /fb bagdebug") return end
        local empty, filled
        for _, b in frame:EnumerateValidItems() do
            local has = type(b) == "table" and b.GetBagID and C_Container.GetContainerItemID(b:GetBagID(), b:GetID()) ~= nil
            if has then filled = filled or b else empty = empty or b end
        end
        local function dump(label, b)
            if not b then Print("bagdebug " .. label .. ": none") return end
            local keys = {}
            for k, v in pairs(b) do if type(v) == "table" and v.GetObjectType then keys[v] = tostring(k) end end
            Print("bagdebug " .. label .. " " .. tostring(b:GetDebugName()) .. " bag " .. tostring(b.GetBagID and b:GetBagID()) .. " slot " .. b:GetID()
                .. " normal=" .. tostring(keys[b:GetNormalTexture() or false] or (b:GetNormalTexture() and "yes" or "nil")))
            for _, r in ipairs({ b:GetRegions() }) do
                if r:GetObjectType() == "Texture" then
                    local ok, line = pcall(function()
                        local cr, cg, cb, ca = r:GetVertexColor()
                        return string.format("  %s %s layer=%s tex=%s atlas=%s shown=%s a=%.2f col=%.2f,%.2f,%.2f,%.2f", keys[r] or "?", tostring(r:GetDebugName()),
                            tostring(r:GetDrawLayer()), tostring(r:GetTexture()), tostring(r:GetAtlas()), tostring(r:IsShown()), r:GetAlpha(), cr, cg, cb, ca)
                    end)
                    Print(ok and line or ("  error " .. tostring(line)))
                end
            end
        end
        dump("empty", empty)
        dump("filled", filled)
        return
    end
    if verb == "frames" then
        local list = ForeverBotsListIssueFrames and ForeverBotsListIssueFrames() or {}
        Print("issue reporter frames hidden: " .. (#list > 0 and table.concat(list, ", ") or "none found") .. " (a new search runs over the next second; type /fb frames again for its result)")
        return
    end
    if verb == "chattest" then
        -- Run this in a fight (type it, or bind it to a macro): says what the client does with a group order sent as an addon message
        -- and as a plain chat line, so the order buttons can use whichever works in combat. The text "fbtest" is no bot order.
        local channel = (IsInRaid and IsInRaid()) and "RAID" or "PARTY"
        local function show(label, fn, ...)
            local ok, v = pcall(fn, ...)
            if not ok then v = "error: " .. tostring(v) elseif IsSecret(v) then v = "<secret>" end
            Print("chattest " .. label .. " = " .. tostring(v))
        end
        Print("chattest: in combat " .. tostring(InCombatLockdown and InCombatLockdown()) .. ", channel " .. channel .. ", prefix registered " .. tostring(prefixOk))
        if C_ChatInfo then
            for _, fn in ipairs({ "InChatMessagingLockdown", "IsAddonChatRestricted" }) do
                if C_ChatInfo[fn] then show("C_ChatInfo." .. fn, C_ChatInfo[fn]) end
            end
        end
        show("SendAddonMessage result", C_ChatInfo and C_ChatInfo.SendAddonMessage or SendAddonMessage, ORDER_PREFIX, "fbtest", channel)
        local seen = false
        local ev = CreateFrame("Frame")
        ev:RegisterEvent(channel == "RAID" and "CHAT_MSG_RAID" or "CHAT_MSG_PARTY")
        ev:RegisterEvent(channel == "RAID" and "CHAT_MSG_RAID_LEADER" or "CHAT_MSG_PARTY_LEADER")
        ev:SetScript("OnEvent", function(_, _, text) if text == "fbtest" then seen = true end end)
        show("SendChatMessage call", SendChatMessage, "fbtest", channel)
        if C_Timer and C_Timer.After then
            C_Timer.After(2, function()
                ev:UnregisterAllEvents()
                Print("chattest: the chat line " .. (seen and "WAS" or "was NOT") .. " delivered to " .. channel .. " chat (a blocked call shows no error, only a missing line)")
            end)
        end
        return
    end
    if verb == "menu" then
        if class == "on" then ForeverBotsDB.menuEntries = true
        elseif class == "off" then ForeverBotsDB.menuEntries = false
        else ForeverBotsDB.menuEntries = (ForeverBotsDB.menuEntries == false) end
        Print(L["Whisper/Invite entries in the portrait right-click menu: %s (takes effect after /reload)"]:format(ForeverBotsDB.menuEntries == false and L["off"] or L["on"])
            .. "  (/fb menu on|off)")
        return
    end
    if verb == "bgw" then
        ForeverBotsDB.bgWhisper = (class == "dot") and "dot" or "native"
        Print("battleground whisper mode: " .. ForeverBotsDB.bgWhisper .. "  (/fb bgw native|dot)")
        return
    end
    if verb == "assign" then
        -- /fb assign <name> <marker> [tank|cc|kill], /fb assign clear [marker|name], /fb assign list
        if class == "list" then
            printNextList = true
            RequestList()
        elseif class == "clear" then
            GroupOrder((role and role ~= "") and ("assign clear " .. role) or "assign clear")
            RequestListSoon()
        elseif class and class ~= "" and role and role ~= "" then
            GroupOrder("assign " .. class .. " " .. role .. ((extra and extra ~= "") and (" " .. extra) or ""))
            RequestListSoon()
        else
            Print("/fb assign <name|target> <star|circle|diamond|triangle|moon|square|cross|skull> [tank|cc|kill]; /fb assign clear [marker|name]; /fb assign list")
        end
        return
    end
    if verb == "bot" then
        -- /fb bot on|off: the floating per-bot panel that follows your target when it is one of your group's bots.
        if class == "on" or class == "off" then
            ForeverBotsDB.botPanel = (class == "on")
            if not botPanel then BuildBotPanel() end
            UpdateBotPanel()
        end
        Print(L["bot panel is %s."]:format(BotPanelEnabled() and L["on"] or L["off"]) .. " /fb bot on|off")
        return
    end
    if verb == "tank" and class and class ~= "" then
        -- /fb tank <name> <marker>, /fb tank <name> clear, /fb tank clear
        if class == "clear" then GroupOrder("clear tanks")
        elseif role and role ~= "" then GroupOrder("tank " .. class .. " " .. role)
        else Print("/fb tank <name> <skull|cross|star|circle|diamond|triangle|moon|square|clear>; /fb tank clear") end
        return
    end
    if verb == "add" and class and class ~= "" then
        AddBot(class, role ~= "" and role or nil)
    elseif verb == "call" or verb == "dismiss" or verb == "save" or verb == "unsave" or verb == "regear" then
        ServerCommand(".bot " .. verb .. " " .. ((class and class ~= "") and class or "all"))
    elseif verb == "delete" and class and class ~= "" then
        ServerCommand(".bot delete " .. class)
    elseif verb == "list" then
        ServerCommand(".bot list")
    elseif verb == "raidready" or verb == "ready" then
        RaidReady()
    elseif verb == "fallback" then
        GroupOrder("fallback")
    elseif verb == "rally" then
        GroupOrder("rally here")
    elseif verb == "pullback" then
        GroupOrder("pull back")
    elseif verb == "formation" and class and class ~= "" then
        GroupOrder("formation " .. class)
    elseif verb == "spacing" and class and class ~= "" then
        GroupOrder("spacing " .. class)
    elseif verb == "distance" then
        local n = tonumber(class)
        if n then SetDistance(n)
        else Print(L["distance: %s. /fb distance <1-20> sets how far the bots follow you."]:format(ForeverBotsDB.distance and L["%d yards"]:format(ForeverBotsDB.distance) or L["auto"])) end
    elseif verb == "resume" then
        GroupOrder("resume")
    elseif verb == "flee" then
        GroupOrder("flee")
    elseif verb == "quick" then
        QuickParty()
    elseif verb == "dungeon" or verb == "dungeons" then
        UI.DungeonQueue((class and class ~= "") and class or "list")
    elseif verb == "maintank" then
        GroupOrder((class and class ~= "") and ("set main tank " .. class) or "set main tank")
    elseif verb == "" or verb == nil then
        Toggle()
    else
        Print(L["unknown /fb word \"%s\" (add-on %s)"]:format(tostring(verb), UI.version or "?"))
        Print("/fb opens the window; /fb add <class> [tank|healer|dps]; /fb raidready; /fb call|dismiss|save|unsave|regear [name|all]; /fb delete <name>; /fb list; /fb fallback; /fb pullback; /fb rally; /fb formation circle|arrow|line|none; /fb spacing tight|close|spread; /fb distance <1-20>; /fb maintank [name]; /fb bot on|off; /fb tank <name> <marker>|clear; /fb assign <name> <marker> [role]|clear|list; /fb minimap on|off|reset; /fb scale <0.7-1.5>; /fb resume; /fb flee; /fb quick; /fb dungeon [name|any|leave|cancel]; /fb debug on|off; /fb perf [reset]; /fb part [name on|off]")
    end
end
UI.slash = SlashCmdList.FOREVERBOTS

-- Tells the server this client runs the add-on (a self-whisper under the order prefix), so it does not send the download hint.
-- Tried a few times: the first send can be refused while the client is still loading.
local function SayHello(attempt)
    attempt = attempt or 1
    Step("hello to the server")
    local sent = Cmd.SendSelf("hello")
    -- Chat to SAY is protected and a timer is not a click, so the silent ".bot addon" greeting is not sent from here; it rides
    -- along with the first command the player sends (see ServerCommand).
    Step("")
    if not sent and attempt < 4 and C_Timer and C_Timer.After then
        C_Timer.After(4, function() SayHello(attempt + 1) end)
    end
end

local loader = CreateFrame("Frame")
loader:RegisterEvent("PLAYER_LOGIN")
loader:SetScript("OnEvent", function()
    -- the version in the greeting, so a screenshot shows which copy the client loaded
    local getMeta = (C_AddOns and C_AddOns.GetAddOnMetadata) or GetAddOnMetadata
    local okV, ver = pcall(getMeta, ADDON, "Version")
    if okV and type(ver) == "string" then UI.version = ver end
    Print(L["type /fb for the bot window."] .. (UI.version and (" (" .. UI.version .. ")") or ""))
    -- A second copy of this add-on under another folder name (an old one) sets /fb too, and the copy loaded last owns it: say which.
    if SlashCmdList.FOREVERBOTS ~= UI.slash then
        local okS, secure, who = pcall(issecurevariable, SlashCmdList, "FOREVERBOTS")
        Print("|cffff4040" .. L["/fb belongs to another add-on (%s), not to this %s %s: remove the old copy from Interface\\AddOns."]:format(
            tostring(okS and not secure and who or "?"), ADDON, UI.version or "") .. "|r")
    end
    if C_Timer and C_Timer.After then C_Timer.After(2, SayHello) else SayHello() end
    PerfWrap("login: menu entries", AddPopupEntry)()
    PerfWrap("login: minimap button", BuildMinimapButton)()
    lastGroupSize = GroupSize()
    ShowDistance()
    if ForeverBotsDB.distance and lastGroupSize > 0 then distFrame.groupAt = Now() + 3 end
    -- built at login (hidden unless it was open), so the secure window key can show it in a fight
    if InCombatLockdown and InCombatLockdown() then
        UI.buildLater = true
        if ForeverBotsDB.open then UI.buildAfterCombat = true end
    else
        if not frame then PerfWrap("login: build window", BuildFrame)() end
        if ForeverBotsDB.open then frame:Show() end
    end
end)

-- On client 70291 a battleground leaves the chat box broken: every typed slash line (also /fb, /w) then throws
-- ChatFrameEditBox.lua:206 "table cannot be indexed with secret keys" until /reload (playtest 2026-10-09). Say so on leaving one.
do
    local pvpWas = false
    local pvpWatch = CreateFrame("Frame")
    pvpWatch:RegisterEvent("PLAYER_ENTERING_WORLD")
    pvpWatch:SetScript("OnEvent", function()
        local ok, inInstance, kind = pcall(IsInInstance)
        local now = ok and inInstance and (kind == "pvp" or kind == "arena") or false
        if pvpWas and not now then Debug("left the battleground: if typed / commands now show a Lua error, type /reload to clear the chat box.") end
        pvpWas = now
    end)
end

-- Diagnostic (70291 home party frame): print what the client itself believes about the group whenever it changes, so it can be read
-- in a battleground where typed slash commands fail. Remove once the party frame works on 70291.
do
    local lastLine
    local function Report(why)
        local function call(fn, ...)
            if type(fn) ~= "function" then return "n/a" end
            local ok, v = pcall(fn, ...)
            if not ok then return "err" end
            return tostring(v)
        end
        local line = string.format("group (%s): IsInGroup=%s IsInRaid=%s members=%s", why, call(IsInGroup), call(IsInRaid), call(GetNumGroupMembers))
        if line:gsub("^group %b():", "") ~= (lastLine or "") then
            lastLine = line:gsub("^group %b():", "")
            Debug(line)
        end
    end
    local groupWatch = CreateFrame("Frame")
    groupWatch:RegisterEvent("PLAYER_ENTERING_WORLD")
    pcall(groupWatch.RegisterEvent, groupWatch, "GROUP_ROSTER_UPDATE")
    groupWatch:SetScript("OnEvent", function(_, ev) Report(ev == "PLAYER_ENTERING_WORLD" and "entering world" or "roster update") end)
end

------------------------------------------------------------------------------------------------------ issue reporter
-- The beta client's "Issue Reporter" bug button (bottom right, also the "F6 to submit an issue" hint) is hidden (playtest 2026-10-12).
-- The 70338 interface source is not at hand, so the frame is found by name: any global frame whose name contains "IssueReport"
-- or "BugReport" (and the common names tried directly). Hide() plus a Show hook that hides it again: nothing is reparented or
-- replaced, the frame is not protected, so there is no taint and it works in combat. The known names are tried again on
-- PLAYER_ENTERING_WORLD and every ADDON_LOADED (the button may belong to a load-on-demand Blizzard addon). "/fb frames" lists
-- what was found.
do
    local hooked, found = {}, {}
    local function HideFrame(name, f)
        if hooked[f] then return end
        hooked[f] = true
        found[#found + 1] = name
        -- alpha 0, not Hide(): Hide() on a protected frame from add-on code is a blocked call; SetAlpha is not
        pcall(f.SetAlpha, f, 0)
        pcall(hooksecurefunc, f, "Show", function(self) pcall(self.SetAlpha, self, 0) end)
    end
    local KNOWN = { "IssueReporterButton", "IssueReporterFrame", "IssueReporter", "BetaIssueReporter", "BugReportButton", "BugReportFrame" }
    local function ScanKnown()
        Step("issue reporter hide")
        for _, name in ipairs(KNOWN) do
            local f = _G[name]
            if type(f) == "table" and f.Hide and f.GetObjectType then HideFrame(name, f) end
        end
        Step("")
    end
    local function Match(name, f)
        if type(name) == "string" and not hooked[f] and (name:find("IssueReport", 1, true) or name:find("BugReport", 1, true)) then
            if type(f) == "table" and f.Hide and f.GetObjectType and f.IsProtected then HideFrame(name, f) end
        end
    end
    -- The walk over every global name is spread over frames, SCAN_STEP names a frame: on client 70630 one walk per ADDON_LOADED
    -- (about a hundred of them at login) raised "Script from ForeverBots has exceeded its execution time limit" (BRS 2026-10-27).
    -- It runs once after login or /reload, and on "/fb frames"; ADDON_LOADED and zoning only try the known names.
    local SCAN_STEP = 2000
    local walker = CreateFrame("Frame")
    walker:Hide()
    walker:SetScript("OnUpdate", function(self)
        Step("issue reporter scan")
        local ok, done = pcall(function()
            local k, v = self.key, nil
            for _ = 1, SCAN_STEP do
                k, v = next(_G, k)
                if k == nil then return true end
                Match(k, v)
            end
            self.key = k
            return false
        end)
        Step("")
        if not ok or done then
            self.key = nil
            self:Hide()
        end
    end)
    local function ScanAll()
        ScanKnown()
        walker.key = nil
        walker:Show()
    end
    AddPart("reporter", function()
        local watch = CreateFrame("Frame")
        watch:RegisterEvent("PLAYER_ENTERING_WORLD")
        watch:RegisterEvent("ADDON_LOADED")
        watch:SetScript("OnEvent", function(_, event, isLogin, isReload)
            pcall(ScanKnown)
            if event == "PLAYER_ENTERING_WORLD" and (isLogin or isReload) then ScanAll() end
        end)
        pcall(ScanKnown)
    end)
    ForeverBotsListIssueFrames = function() ScanAll(); return found end
end

------------------------------------------------------------------------------------------------------ forbidden-action guard
-- "ForeverBots has been blocked from an action only available to the Blizzard UI" (ADDON_ACTION_FORBIDDEN) names the protected
-- function but only in the popup. This records that name (ForeverBotsDB.lastForbidden, and one chat line, so a player can report
-- it) and, when it is not one of our own chat sends, switches the portrait-menu entries off for the next load: the entries are the
-- only part of the add-on that runs inside the client's own menus, the place such taint comes from (reported 2026-10-15 when
-- interacting with a bot portrait). /fb menu on re-enables them.
do
    -- The add-on itself never calls SetBinding (its keys come from Bindings.xml and the player's own choice in Key Bindings), yet
    -- Warsong Gulch 2026-10-25 showed "the client blocked SetBinding() for ForeverBots" four times: client code ran with our taint
    -- and called it in a fight. A secure post-hook (it does not taint the caller) keeps who called it last, so the report names the
    -- caller instead of a guess; ForeverBotsDB.lastForbidden.stack keeps it for the SavedVariables file.
    local lastBinding, bindingPending
    AddPart("bindhook", function()
        if not (hooksecurefunc and SetBinding) then return end
        pcall(hooksecurefunc, "SetBinding", function(key, action)
            pcall(function()
                lastBinding = {
                    key = SafeStr(key) or "?", action = SafeStr(action) or "nil",
                    combat = (InCombatLockdown and InCombatLockdown()) and true or false,
                    stack = debugstack and (SafeStr(debugstack(3, 6, 0)) or "") or "",
                }
            end)
        end)
    end)
    local function BindingReport()
        local b = lastBinding
        if not b then return "no SetBinding call seen by the add-on" end
        local callers = {}
        for line in string.gmatch(b.stack, "[^\n]+") do
            callers[#callers + 1] = (line:gsub("^%s+", ""):gsub("Interface\\AddOns\\", ""):gsub("Interface/AddOns/", ""))
            if #callers == 3 then break end
        end
        return "SetBinding(" .. b.key .. ", " .. b.action .. ")" .. (b.combat and " in combat" or "") .. " from " ..
            (#callers > 0 and table.concat(callers, " < ") or "?")
    end

    local guard = CreateFrame("Frame")
    guard:RegisterEvent("ADDON_ACTION_FORBIDDEN")
    pcall(guard.RegisterEvent, guard, "ADDON_ACTION_BLOCKED")
    guard:SetScript("OnEvent", function(_, event, addon, func)
        -- the client passes the name with its brackets ("SetBinding()"); every line below adds them itself
        func = (type(func) == "string" and not IsSecret(func)) and func:gsub("%(%)$", "") or "?"
        -- "Interface action failed because of an AddOn" in the chat frame right after login: say which add-on and which call, for
        -- ours and for any other add-on, so the line can be read instead of guessed at (the pop-up names them only for a moment).
        if addon ~= ADDON then
            if type(addon) == "string" and not IsSecret(addon) then Print("the client blocked " .. func .. "() for the add-on " .. addon .. " (" .. (event == "ADDON_ACTION_BLOCKED" and "blocked" or "forbidden") .. ")") end
            return
        end
        if event == "ADDON_ACTION_BLOCKED" then
            local rec = { func = func, at = date and date("%Y-%m-%d %H:%M:%S") or "", blocked = true, step = currentStep }
            ForeverBotsDB.lastForbidden = rec
            local step = currentStep ~= "" and currentStep or "no step marked"
            if func == "SetBinding" then
                -- the post-hook runs once the blocked call returns: read it a moment later, once per burst of blocked calls
                if bindingPending then return end
                bindingPending = true
                local function Report()
                    bindingPending = nil
                    rec.stack = lastBinding and lastBinding.stack or ""
                    Print("the client blocked SetBinding() for ForeverBots during: " .. step .. ". " .. BindingReport() .. ". Please report this line.")
                end
                if C_Timer and C_Timer.After then C_Timer.After(0, Report) else Report() end
                return
            end
            if step == "dot command" and Cmd.lastSaid then
                -- the whisper to yourself that carries a dot command was refused: say what to type instead of only that it failed
                Print(L["could not send \"%s\"%s: type it in chat."]:format(Cmd.lastSaid, " (" .. func .. "() blocked)"))
                return
            end
            Print("the client blocked " .. func .. "() for ForeverBots (blocked) during: " .. step)
            return
        end
        ForeverBotsDB.lastForbidden = { func = func, at = date and date("%Y-%m-%d %H:%M:%S") or "" }
        -- which client globals carry our taint (issecurevariable is false and names us), so a report says where it came from
        local tainted = {}
        for _, n in ipairs({ "SlashCmdList", "hash_SlashCmdList", "ChatFrame1EditBox", "DEFAULT_CHAT_FRAME", "ChatEdit_SendText", "ChatEdit_ParseText",
            "ChatFrame_OpenChat", "SendChatMessage", "RunMacroText", "UnitPopup_ShowMenu" }) do
            local ok, secure, who = pcall(issecurevariable, n)
            if ok and secure == false then tainted[#tainted + 1] = n .. "<-" .. tostring(who) end
        end
        ForeverBotsDB.lastForbidden.tainted = table.concat(tainted, ", ")
        if func == "RunMacroText" then
            Print("the client blocked RunMacroText() (an order button's macro) during: " .. (currentStep ~= "" and currentStep or "no step marked")
                .. "; tainted globals: " .. (#tainted > 0 and table.concat(tainted, ", ") or "none"))
            return
        end
        if func:find("SendChatMessage", 1, true) or func:find("SendAddonMessage", 1, true) then
            Print("the client blocked " .. func .. "() (not allowed in this situation)")
            return
        end
        Print("the client blocked " .. func .. "() because of ForeverBots; the portrait menu entries are switched off after /reload (/fb menu on to bring them back). Please report this line.")
        ForeverBotsDB.menuEntries = false
    end)
end

------------------------------------------------------------------------------------------------------ movement keys kept
-- "I set strafe left/right to Q and E, and every time I reopen the game (or after a loading screen) they are back to default"
-- (fdshiga, pennywhi, cruborg and others, 2026-11-08). The server keeps the key bindings like any other (the rest of them stay), and
-- the WTF file has Q and E: it is the client's own turn/strafe code (Blizzard_RPE_TurnStrafe calls
-- C_KeyBindings.UpdateTurnStrafeBindingsForCharacter after PLAYER_ENTERING_WORLD) that puts its turn/strafe style back on A, D, Q, E
-- and the arrow keys each time the world loads. This keeps what the player chose: the keys below and the keys of the movement
-- actions are remembered whenever the bindings change outside a loading screen (per binding set: account, or character when
-- "Character Specific Key Bindings" is ticked), and put back once the world has loaded. Never in combat (SetBinding is protected
-- there): then it waits for the fight to end. "/fb part movekeys off" and /reload switch it off.
-- (A function, not a do block: the file's main chunk is at Lua's limit of 200 local variables.)
;(function()
    local MOVE_ACTIONS = { "MOVEFORWARD", "MOVEBACKWARD", "TURNLEFT", "TURNRIGHT", "STRAFELEFT", "STRAFERIGHT" }
    local WATCH_KEYS = { "W", "S", "A", "D", "Q", "E", "UP", "DOWN", "LEFT", "RIGHT" }
    local GUARD_SECONDS = 10           -- after a loading screen, binding changes are the client's, not the player's
    local guardUntil, restorePending = 0, false
    local frame = CreateFrame("Frame")

    local function SetKey()
        local ok, set = pcall(GetCurrentBindingSet)
        if ok and Enum and Enum.BindingSet and set == Enum.BindingSet.Character then
            return "char:" .. (SafeName("player") or "?") .. "-" .. (GetRealmName and (SafeStr(GetRealmName()) or "") or "")
        end
        return "account"
    end

    -- key -> action ("" when the key is unbound) for the watched keys and every key bound to a movement action
    local function Snapshot()
        local map = {}
        for _, key in ipairs(WATCH_KEYS) do map[key] = GetBindingAction(key) or "" end
        for _, action in ipairs(MOVE_ACTIONS) do
            local keys = { GetBindingKey(action) }
            for _, key in ipairs(keys) do
                if type(key) == "string" and key ~= "" then map[key] = action end
            end
        end
        return map
    end

    local function Remember()
        ForeverBotsDB.moveKeys = ForeverBotsDB.moveKeys or {}
        ForeverBotsDB.moveKeys[SetKey()] = Snapshot()
    end

    local function Restore()
        restorePending = false
        local saved = ForeverBotsDB.moveKeys and ForeverBotsDB.moveKeys[SetKey()]
        if not saved then Remember(); return end
        if InCombatLockdown and InCombatLockdown() then
            restorePending = true
            frame:RegisterEvent("PLAYER_REGEN_ENABLED")
            return
        end
        local changed = 0
        for key, action in pairs(saved) do
            local now = GetBindingAction(key) or ""
            if now ~= action then
                local ok = pcall(SetBinding, key, action ~= "" and action or nil)
                if ok then changed = changed + 1 end
            end
        end
        if changed > 0 then
            -- keep the guard up while our own change fires UPDATE_BINDINGS, then save the set the player uses
            guardUntil = math.max(guardUntil, GetTime() + 1)
            pcall(SaveBindings, GetCurrentBindingSet())
            Debug("movement keys: " .. changed .. " key(s) put back after the client changed them")
        end
    end

    local function After(sec, fn) if C_Timer and C_Timer.After then C_Timer.After(sec, fn) else fn() end end

    local function WorldLoaded()
        guardUntil = GetTime() + GUARD_SECONDS
        -- the client's change comes right after the world is shown; check a few times while it settles
        After(1, Restore); After(3, Restore); After(GUARD_SECONDS - 1, Restore)
    end

    AddPart("movekeys", function()
        if not (GetBindingAction and GetBindingKey and SetBinding and SaveBindings and GetCurrentBindingSet) then return end
        frame:RegisterEvent("PLAYER_ENTERING_WORLD")
        frame:RegisterEvent("LOADING_SCREEN_ENABLED")
        frame:RegisterEvent("UPDATE_BINDINGS")
        pcall(frame.RegisterEvent, frame, "NOTIFY_TURN_STRAFE_CHANGE")
        frame:SetScript("OnEvent", function(self, event)
            if event == "LOADING_SCREEN_ENABLED" then
                guardUntil = GetTime() + 600   -- until the world is loaded (PLAYER_ENTERING_WORLD sets the real end)
            elseif event == "PLAYER_ENTERING_WORLD" then
                WorldLoaded()
            elseif event == "NOTIFY_TURN_STRAFE_CHANGE" then
                guardUntil = math.max(guardUntil, GetTime() + 2)
                After(0.5, Restore)
            elseif event == "PLAYER_REGEN_ENABLED" then
                self:UnregisterEvent("PLAYER_REGEN_ENABLED")
                if restorePending then Restore() end
            elseif event == "UPDATE_BINDINGS" then
                if GetTime() >= guardUntil and not restorePending then Remember() end
            end
        end)
    end)
end)()

PERF.running = nil   -- the file loaded to the end
