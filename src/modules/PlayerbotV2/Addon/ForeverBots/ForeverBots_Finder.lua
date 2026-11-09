--[[
ForeverBots_Finder: the Group finder tab of the main window (after Marks), the bot dungeon finder without typing.
Pick your role and a dungeon (Any for my level, or one by one with < and >), Queue: your bots fill the missing tank, healer and
damage places at your level and the group is put inside the dungeon's door. Cancel stops a queue that is still forming, Leave
dungeon takes you back to where you queued. Fill my group fills the party here without going anywhere (.bot lfg).
The lines go to the server as hidden add-on messages; the status line asks the server every 2 seconds while the tab is open.
]]

local ADDON, ns = ...
local Net = ns.Net
local L = ns.L or setmetatable({}, { __index = function(_, k) return (k:gsub("^short:", "")) end })   -- text by client language
local F = {}
ns.Finder = F

local ROLES = { { nil, L["Auto"], L["Your role is left to the server: the bots fill what your group is missing around you."] },
                { "tank", L["Tank"], L["You tank: the bots bring a healer and damage."] },
                { "healer", L["Heal"], L["You heal: the bots bring a tank and damage."] },
                { "dps", L["DPS"], L["You deal damage: the bots bring a tank, a healer and the rest."] } }

-- The roles your class can play (the server checks the same: a rogue who picks Tank still gets a tank bot).
local CAN = { WARRIOR = { tank = true }, DEATHKNIGHT = { tank = true }, DEMONHUNTER = { tank = true },
              PALADIN = { tank = true, healer = true }, MONK = { tank = true, healer = true }, DRUID = { tank = true, healer = true },
              PRIEST = { healer = true }, SHAMAN = { healer = true }, EVOKER = { healer = true } }

function F.CanPlay(classToken, role)
    if not role or role == "dps" then return true end
    local c = CAN[classToken or ""]
    return c and c[role] or false
end

-- The role that goes to the server: a saved pick your class cannot play (Tank kept from another character) is Auto.
function F.Role()
    local role = ForeverBotsDB.finderRole
    local token = UnitClass and select(2, UnitClass("player"))
    if F.CanPlay(token, role) then return role end
    return nil
end

-- Any for my level first, then the dungeons of your level, then every other one by level; Upper Blackrock Spire (10) only from 58.
function F.Choices(h, level)
    local list = { { word = "any", name = L["Any for my level"] } }
    local seen = {}
    for _, d in ipairs(h.DungeonsForLevel(level)) do
        list[#list + 1] = { word = d[1], name = L[d[2]], min = d[3], max = d[4], fits = true }
        seen[d[1]] = true
    end
    for _, d in ipairs(h.DUNGEONS) do
        if not seen[d[1]] and (not d[5] or level >= 58) then
            list[#list + 1] = { word = d[1], name = L[d[2]] .. (d[5] and (" (" .. d[5] .. ")") or ""), min = d[3], max = d[4] }
        end
    end
    return list
end

-- LFG_RESP: phase, dungeon, waited s, form wait s, members, size, can leave.
function F.Parse(fields)
    fields = fields or {}
    return { phase = fields[1] or "idle", name = fields[2] or "", waited = tonumber(fields[3]) or 0, form = tonumber(fields[4]) or 90,
             members = tonumber(fields[5]) or 0, size = tonumber(fields[6]) or 0, canLeave = fields[7] == "1" }
end

function F.StatusText(st)
    if not st then return "|cff9d9d9d" .. L["Asking the server..."] .. "|r" end
    local name = "|cffffd100" .. L[st.name] .. "|r"
    if st.phase == "forming" then
        return L["Queued for %s: %d of %d in the group, waiting %d s (up to %d)."]:format(name, st.members, st.size, st.waited, st.form)
    elseif st.phase == "entering" then
        return L["Going into %s..."]:format(name)
    elseif st.phase == "leaving" then
        return L["On the way out, your bots follow..."]
    elseif st.phase == "inside" then
        return L["In %s."]:format(name) .. (st.canLeave and "" or L[" No known way out: walk out or use your hearthstone."])
    end
    return L["Not queued."] .. ((st.members or 0) > 1 and L[" Your group has %d."]:format(st.members) or "")
end

local pane, h
F.state, F.pick = nil, 1

local function Refresh()
    if not pane then return end
    local level = (UnitLevel and UnitLevel("player")) or 1
    F.list = F.Choices(h, level)
    if F.pick > #F.list then F.pick = 1 end
    local c = F.list[F.pick]
    pane.choice:SetText(c.name)
    pane.range:SetText(c.min and (((c.fits and "|cff40ff40" or "|cffbbbbbb") .. L["Levels %d-%d"]:format(c.min, c.max) .. "|r")) or ("|cffbbbbbb" .. L["A dungeon for level %d"]:format(level) .. "|r"))
    local role = F.Role()
    local token = UnitClass and select(2, UnitClass("player"))
    for _, b in ipairs(pane.roleButtons) do
        b:SetEnabled(F.CanPlay(token, b.role))
        if b.role == role then b:LockHighlight() else b:UnlockHighlight() end
    end
    local st = F.state
    pane.status:SetText(F.StatusText(st))
    local phase = st and st.phase or "idle"
    pane.queue:SetEnabled(phase == "idle")
    pane.cancel:SetEnabled(phase == "forming")
    pane.leave:SetEnabled(phase == "inside" and st.canLeave or false)
    pane.fill:SetEnabled(phase == "idle")
end
F.Refresh = Refresh

function F.Request()
    if not (Net and Net.Ready()) then
        F.state = nil
        if pane then pane.status:SetText("|cffff6060" .. L["The server's status cannot be asked (add-on prefix not registered)."] .. "|r") end
        return
    end
    Net.Request("LFG_REQ", {}, function(fields)
        if fields then F.state = F.Parse(fields) end
        Refresh()
    end)
end

-- After a button: the status again once the server has acted.
local function Soon()
    if C_Timer and C_Timer.After then C_Timer.After(1, F.Request) end
end

function ns.BuildFinder(p, helpers)
    pane, h = p, helpers
    local Btn, Section, Tip = h.Btn, h.Section, h.Tip
    Section(p, 0, L["Your role"])
    p.roleButtons = {}
    for i, r in ipairs(ROLES) do
        local b = Btn(p, (i - 1) * 60, 22, 58, 24, r[2], { size = 11 })
        b.role = r[1]
        b:SetScript("OnClick", function() ForeverBotsDB.finderRole = r[1]; Refresh() end)
        Tip(b, r[2], r[3])
        p.roleButtons[i] = b
    end

    Section(p, 54, L["Dungeon"])
    local prev = Btn(p, 0, 76, 28, 24, "<", { size = 13 })
    prev:SetScript("OnClick", function() F.pick = (F.pick - 2) % #F.list + 1; Refresh() end)
    local nextBtn = Btn(p, 210, 76, 28, 24, ">", { size = 13 })
    nextBtn:SetScript("OnClick", function() F.pick = F.pick % #F.list + 1; Refresh() end)
    p.choice = p:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    p.choice:SetPoint("TOP", p, "TOPLEFT", 119, -78)
    p.choice:SetWidth(176)
    p.choice:SetWordWrap(false)
    p.range = p:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    p.range:SetPoint("TOP", p.choice, "BOTTOM", 0, -2)
    p.range:SetWidth(176)
    p.queue = Btn(p, 0, 106, 117, 24, L["Queue"], {})
    p.queue:SetScript("OnClick", function()
        local c = F.list and F.list[F.pick]
        if c then h.Queue(c.word, F.Role(), true); Soon() end
    end)
    Tip(p.queue, L["Queue"], L["Your bots fill the missing tank, healer and damage places at your level (wearing the gear picked on the Bots tab) and the group is put inside the dungeon. Only the group leader can queue."])
    p.cancel = Btn(p, 121, 106, 117, 24, L["Cancel"], {})
    p.cancel:SetScript("OnClick", function() h.Queue("cancel", nil, true); Soon() end)
    Tip(p.cancel, L["Cancel"], L["Stops a queue that is still forming. The bots that joined stay in your group."])

    Section(p, 140, L["Status"])
    p.status = p:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    p.status:SetPoint("TOPLEFT", p, "TOPLEFT", 2, -162)
    p.status:SetWidth(234)
    p.status:SetJustifyH("LEFT")
    p.status:SetJustifyV("TOP")

    p.leave = Btn(p, 0, 212, 117, 24, L["Leave dungeon"], {})
    p.leave:SetScript("OnClick", function() h.Queue("leave", nil, true); Soon() end)
    Tip(p.leave, L["Leave dungeon"], L["Back to where you queued (else the dungeon's way out); your bots follow. Not in combat or while dead."])
    p.fill = Btn(p, 121, 212, 117, 24, L["Fill my group"], {})
    p.fill:SetScript("OnClick", function() h.Lfg(F.Role()); Soon() end)
    Tip(p.fill, L["Fill my group"], L["Fills the missing tank, healer and damage places of your group here and now (1 tank, 1 healer, 3 damage), without going anywhere."])

    -- the status while the tab is open
    p:HookScript("OnShow", function() F.Request() end)
    p:HookScript("OnUpdate", function(self, elapsed)
        self.fbAsk = (self.fbAsk or 0) + elapsed
        if self.fbAsk >= 2 then self.fbAsk = 0; F.Request() end
    end)
    Refresh()
end
