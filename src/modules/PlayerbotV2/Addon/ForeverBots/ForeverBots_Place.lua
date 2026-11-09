--[[
ForeverBots_Place: the Positions tab of the main window (after World bots). Pick a shape and the bots, then send them: they run to
their spots in that shape where you stand (or round your target) and stay there, also after a fight (they walk back to their own
spot when it is over). Follow lets the ticked bots follow you again. Karrz 2026-11-03: "select the formation, move specific bots,
set them in a formation: close, circle, far apart, line, spear, arrow".
Sent as a hidden add-on message (PLACE_REQ); without that line, as the group order "place <shape> <gap> <here|target> <names>".
The bot list is your group (the server only moves your own bots and the world bots you lead). Buttons only: nothing here shows or
hides a frame, so it all works in a fight.
]]

local ADDON, ns = ...
local Net = ns.Net
local L = ns.L or setmetatable({}, { __index = function(_, k) return (k:gsub("^short:", "")) end })   -- text by client language
local P = {}
ns.Place = P

-- word sent to the server, button text, tooltip
P.SHAPES = {
    { "close", L["Close"], L["A tight clump round the spot."] },
    { "circle", L["Circle"], L["One ring round the spot."] },
    { "arc", L["Arc"], L["Half a ring on the front side: round a boss you target, or ahead of you."] },
    { "spread", L["Far apart"], L["A wide grid, about 10 yards between bots on a normal gap: for fights where bots must spread out."] },
    { "line", L["Line"], L["Side by side across the way you face (a second row past 10 bots)."] },
    { "column", L["Column"], L["Single file the way you face (two or three abreast for many bots)."] },
    { "spear", L["Spear"], L["A point, a head of three, then a shaft (wider for many bots). Tanks at the point."] },
    { "arrow", L["Arrow"], L["A V pointing the way you face. Tanks at the tip."] },
}
P.GAPS = {
    { "tight", L["Tight"], L["About 2.5 yards between bots."] },
    { "normal", L["Normal"], L["About 4 yards between bots."] },
    { "wide", L["Wide"], L["About 8 yards between bots."] },
}
P.ROWS = 15            -- bot buttons on the page: 3 columns of 5
P.GROUPS = 8           -- raid groups 1 to 8, one button each
P.shape, P.gap = "circle", "normal"
P.picked = {}          -- name -> true
P.page = 1
P.roles = {}           -- name -> "tank" | "healer" | "dps" from the server's roster, when it answers
P.melee = {}           -- name -> true for a melee damage dealer (by class when the roster says nothing better)

local MELEE_CLASS = { WARRIOR = true, ROGUE = true, PALADIN = true, DRUID = true, SHAMAN = false }

-- Group members other than you: { name, class token, subgroup } in raid order.
function P.Members()
    local out = {}
    local function add(unit, sub)
        if not (UnitExists and UnitExists(unit)) then return end
        if UnitIsUnit and UnitIsUnit(unit, "player") then return end
        local name = ns.SafeName and ns.SafeName(unit) or UnitName(unit)
        if not name then return end
        local _, cls = UnitClass(unit)
        out[#out + 1] = { name = name, class = cls or "", sub = sub or 1, unit = unit }
    end
    if IsInRaid and IsInRaid() then
        local n = GetNumGroupMembers and GetNumGroupMembers() or 0
        for i = 1, n do
            local sub = 1
            if GetRaidRosterInfo then
                local _, _, s = GetRaidRosterInfo(i)
                sub = s or 1
            end
            add("raid" .. i, sub)
        end
    else
        for i = 1, 4 do add("party" .. i, 1) end
    end
    return out
end

local function RoleOf(m)
    -- The raid Main Tank flag (Main tank button, Raid ready) wins: that bot tanks whatever its spec.
    if m.unit and GetPartyAssignment then
        local ok, mt = pcall(GetPartyAssignment, "MAINTANK", m.unit)
        if ok and mt then return "tank" end
    end
    local r = P.roles[m.name]
    if r then return r end
    if UnitGroupRolesAssigned then
        local ok, gr = pcall(UnitGroupRolesAssigned, m.name)
        if ok and gr == "TANK" then return "tank" elseif ok and gr == "HEALER" then return "healer" elseif ok and gr == "DAMAGER" then return "dps" end
    end
    return "dps"
end

-- Which members a quick button ticks.
function P.Matches(m, word)
    local role = RoleOf(m)
    if word == "all" then return true end
    if word == "tank" then return role == "tank" end
    if word == "healer" then return role == "healer" end
    local melee = P.melee[m.name]
    if melee == nil then melee = MELEE_CLASS[m.class] == true end
    if word == "melee" then return role == "dps" and melee end
    if word == "ranged" then return role == "dps" and not melee end
    return false
end

-- The fields of a PLACE_REQ: shape, gap, here|target, then the ticked names (only members still in the group).
function P.Fields(anchor, members)
    local f = { P.shape, P.gap, anchor }
    for _, m in ipairs(members or P.Members()) do
        if P.picked[m.name] then f[#f + 1] = m.name end
    end
    return f
end

local pane

local function Lit(list, word)
    for _, b in ipairs(list) do
        if b.word == word then b:LockHighlight() else b:UnlockHighlight() end
    end
end

-- Raid group g: how many members are in it and how many of them are ticked.
function P.GroupCount(members, g)
    local inG, ticked = 0, 0
    for _, m in ipairs(members) do
        if (m.sub or 1) == g then
            inG = inG + 1
            if P.picked[m.name] then ticked = ticked + 1 end
        end
    end
    return inG, ticked
end

-- A group button: ticks the whole group, or unticks it when every bot in it is ticked already (owner request 2026-11-04: pick
-- by role, by group and by name, all mixed).
function P.ToggleGroup(g)
    local members = P.Members()
    local inG, ticked = P.GroupCount(members, g)
    if inG == 0 then return end
    local on = ticked < inG
    for _, m in ipairs(members) do
        if (m.sub or 1) == g then P.picked[m.name] = on or nil end
    end
    P.Refresh()
end

function P.Refresh()
    if not pane then return end
    Lit(pane.shapeButtons, P.shape)
    Lit(pane.gapButtons, P.gap)
    local members = P.Members()
    local pages = math.max(1, math.ceil(#members / P.ROWS))
    if P.page > pages then P.page = pages end
    local n = 0
    for _, m in ipairs(members) do if P.picked[m.name] then n = n + 1 end end
    for i, b in ipairs(pane.rows) do
        local m = members[(P.page - 1) * P.ROWS + i]
        b.member = m
        if m then
            local c = RAID_CLASS_COLORS and RAID_CLASS_COLORS[m.class]
            local hex = c and string.format("|cff%02x%02x%02x", c.r * 255, c.g * 255, c.b * 255) or "|cffffffff"
            b.fbText:SetText((P.picked[m.name] and "|cff40ff40+|r " or "|cff606060-|r ") .. hex .. m.name .. "|r")
            b:SetEnabled(true)
        else
            b.fbText:SetText("")
            b:SetEnabled(false)
        end
    end
    for g, b in ipairs(pane.groupButtons or {}) do
        local inG, ticked = P.GroupCount(members, g)
        b:SetEnabled(inG > 0)
        if inG > 0 and ticked == inG then b:LockHighlight() else b:UnlockHighlight() end
    end
    pane.pageText:SetText(pages > 1 and (P.page .. "/" .. pages) or "")
    pane.prev:SetEnabled(P.page > 1)
    pane.next:SetEnabled(P.page < pages)
    pane.count:SetText(#members == 0 and ("|cff9d9d9d" .. L["No bots"] .. "|r")
                       or L["%d of %d ticked"]:format(n, #members))
end

-- The server's roster names the roles (tank / healer / dps) and specs; asked when the tab opens.
function P.AskRoles()
    if not (Net and Net.Ready() and ns.Windows and ns.Windows.ParseRoster) then return end
    Net.Request("ROSTER_REQ", { "1" }, function(fields)
        if not fields then return end
        -- The roster's role field is a word: TANK, HEALER or DPS (AddonControl RoleToken). Matching it against "1"/"2"/"3" left
        -- every bot a damage dealer, so Tanks and Heals ticked nobody (owner report 2026-11-04).
        local ROLE = { TANK = "tank", HEALER = "healer", DPS = "dps" }
        local roles = {}
        for _, b in ipairs(ns.Windows.ParseRoster(fields)) do
            local r = ROLE[string.upper(tostring(b.role or ""))]
            if r then roles[b.name] = r end
        end
        P.roles = roles
        P.Refresh()
    end)
end

P.MARKER = 8           -- the world marker the Set button puts down (8 = white skull); the server reads its spot
P.lastSend = 0

-- anchor: "here", "target", "cursor" (the next world marker or ping is the spot) or "follow" (let the ticked bots go).
-- The server answers in a system line itself, so nothing here waits for a reply: with 39 bots and a busy PC the hidden reply came
-- after the 6 s timeout and "no answer from the server (timeout)" showed although the bots had moved (owner report 2026-11-04).
function P.Send(anchor)
    local now = GetTime and GetTime() or 0
    if now - P.lastSend < 1 then return end   -- a double click (or the down and up of the cursor button) sends once
    P.lastSend = now
    local members = P.Members()
    if #members == 0 then
        if ns.Print then ns.Print(L["you have no group yet: add a bot first."]) end
        return
    end
    local fields = P.Fields(anchor == "follow" and "here" or anchor, members)
    if anchor == "follow" then fields[1] = "follow" end
    if #fields <= 3 then
        if ns.Print then ns.Print(L["tick the bots first (All ticks every bot)."]) end
        return
    end
    if Net and Net.Ready() then
        Net.Request("PLACE_REQ", fields)
        return
    end
    -- no add-on line: the group order (an add-on message holds about 250 letters, so a long list is cut to what fits)
    local text = "place " .. fields[1] .. " " .. fields[2] .. " " .. fields[3]
    for i = 4, #fields do
        if #text + #fields[i] + 1 > 250 then
            if ns.Print then ns.Print(L["too many names for one order: only the first bots were sent."]) end
            break
        end
        text = text .. " " .. fields[i]
    end
    if ns.GroupOrder then ns.GroupOrder(text) end
end

function ns.BuildPlace(p, helpers)
    pane = p
    local Btn, Section, Tip = helpers.Btn, helpers.Section, helpers.Tip
    Section(p, 0, L["Shape"])
    p.shapeButtons = {}
    for i, d in ipairs(P.SHAPES) do
        local col, row = (i - 1) % 4, math.floor((i - 1) / 4)
        local b = Btn(p, col * 60, 20 + row * 22, 58, 20, d[2], { size = 10 })
        b.word = d[1]
        b:SetScript("OnClick", function() P.shape = d[1]; ForeverBotsDB.placeShape = d[1]; P.Refresh() end)
        Tip(b, d[2], d[3])
        p.shapeButtons[i] = b
    end
    p.gapButtons = {}
    for i, d in ipairs(P.GAPS) do
        local b = Btn(p, (i - 1) * 80, 66, 78, 20, d[2], { size = 10 })
        b.word = d[1]
        b:SetScript("OnClick", function() P.gap = d[1]; ForeverBotsDB.placeGap = d[1]; P.Refresh() end)
        Tip(b, L["%s gap"]:format(d[2]), d[3])
        p.gapButtons[i] = b
    end

    Section(p, 90, L["Bots"])
    p.count = p:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    p.count:SetPoint("TOPLEFT", p, "TOPLEFT", 44, -93)
    p.count:SetWidth(110)
    p.count:SetJustifyH("LEFT")
    if p.count.SetWordWrap then p.count:SetWordWrap(false) end   -- one line: the role buttons sit right below
    local QUICK = {
        { "all", L["All"], L["Tick every bot in your group."] },
        { "none", L["None"], L["Untick every bot."] },
        { "tank", L["Tanks"], L["Tick the tanks too."] },
        { "healer", L["Heals"], L["Tick the healers too."] },
        { "melee", L["short:Melee"], L["Tick the melee damage dealers too."] },
        { "ranged", L["short:Ranged"], L["Tick the ranged damage dealers too."] },
    }
    for i, d in ipairs(QUICK) do
        local b = Btn(p, (i - 1) * 40, 110, 38, 18, d[2], { size = 9 })
        b:SetScript("OnClick", function()
            for _, m in ipairs(P.Members()) do
                if d[1] == "none" then P.picked[m.name] = nil
                elseif d[1] == "all" or P.Matches(m, d[1]) then P.picked[m.name] = true end
            end
            P.Refresh()
        end)
        Tip(b, d[2], d[3])
    end
    -- one button per raid group (a party is group 1)
    p.groupButtons = {}
    for g = 1, P.GROUPS do
        local b = Btn(p, (g - 1) * 30, 130, 28, 18, L["short:G%d"]:format(g), { size = 9 })
        b:SetScript("OnClick", function() P.ToggleGroup(g) end)
        Tip(b, L["Group %d"]:format(g), L["Tick every bot in this raid group. Click again when they are all ticked to untick them."])
        p.groupButtons[g] = b
    end
    p.rows = {}
    for i = 1, P.ROWS do
        local col, row = (i - 1) % 3, math.floor((i - 1) / 3)
        local b = CreateFrame("Button", nil, p)
        b:SetSize(78, 15)
        b:SetPoint("TOPLEFT", p, "TOPLEFT", col * 80, -(151 + row * 15))
        b:SetHighlightTexture("Interface\\QuestFrame\\UI-QuestTitleHighlight", "ADD")
        local fs = b:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
        fs:SetPoint("LEFT", b, "LEFT", 2, 0)
        fs:SetWidth(76)
        fs:SetJustifyH("LEFT")
        b.fbText = fs
        b:SetScript("OnClick", function(self)
            local m = self.member
            if not m then return end
            P.picked[m.name] = (not P.picked[m.name]) or nil
            P.Refresh()
        end)
        b:SetScript("OnEnter", function(self)
            local m = self.member
            if not m then return end
            GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
            GameTooltip:SetText(m.name)
            GameTooltip:AddLine(L["Click to tick or untick."] .. " " .. L["Group %d"]:format(m.sub or 1), 1, 1, 1, true)
            GameTooltip:Show()
        end)
        b:SetScript("OnLeave", function() GameTooltip:Hide() end)
        p.rows[i] = b
    end
    -- the pages of names: on the Bots heading line, right
    p.prev = Btn(p, 160, 90, 24, 18, "<", { size = 10 })
    p.prev:SetScript("OnClick", function() P.page = math.max(1, P.page - 1); P.Refresh() end)
    p.next = Btn(p, 214, 90, 24, 18, ">", { size = 10 })
    p.next:SetScript("OnClick", function() P.page = P.page + 1; P.Refresh() end)
    p.pageText = p:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    p.pageText:SetPoint("CENTER", p, "TOPLEFT", 199, -99)

    -- Set: a secure world marker button. The click first tells the server to wait for a spot, then the client shows its
    -- ground circle; where the player clicks the marker lands, and the server sends the bots there (also a ping on the ground works).
    local okC, cur = pcall(CreateFrame, "Button", nil, p, "UIPanelButtonTemplate,SecureActionButtonTemplate")
    if not (okC and cur) then cur = CreateFrame("Button", nil, p, "UIPanelButtonTemplate") end
    cur:SetSize(58, 22)
    cur:SetPoint("TOPLEFT", p, "TOPLEFT", 0, -228)
    cur:SetText(L["short:Set"])
    if cur:GetFontString() then cur:GetFontString():SetFontObject("GameFontNormalSmall") end
    cur:RegisterForClicks("AnyUp", "AnyDown")
    cur:SetAttribute("type", "worldmarker")
    cur:SetAttribute("action", "set")
    cur:SetAttribute("marker", P.MARKER)
    cur:SetScript("PreClick", function()
        if P.lastSend > 0 and (GetTime and GetTime() or 0) - P.lastSend < 1 then return end
        P.Send("cursor")
        if ns.Print then ns.Print(L["click the ground where the bots should stand (or ping the spot)."]) end
    end)
    Tip(cur, L["Set with the mouse"], L["Click this, then click the ground: the ticked bots stand in the shape on that spot, facing away from you, and stay there. A white skull marks the spot. You can also ping the spot instead."])
    local here = Btn(p, 60, 228, 58, 22, L["Place here"], { size = 10 })
    here:SetScript("OnClick", function() P.Send("here") end)
    Tip(here, L["Place here"], L["The ticked bots stand in the shape just in front of you, pointing the way you face (Circle and Arc go round you), and stay there. After a fight they walk back to their spot."])
    local tgt = Btn(p, 120, 228, 58, 22, L["At target"], { size = 10 })
    tgt:SetScript("OnClick", function() P.Send("target") end)
    Tip(tgt, L["At target"], L["The same at your target (a mob or an NPC, not one of your group), facing you. No bot stands inside it; Arc and Circle keep out of a boss's reach."])
    local back = Btn(p, 180, 228, 58, 22, L["Follow"], { size = 10 })
    back:SetScript("OnClick", function() P.Send("follow") end)
    Tip(back, L["Follow"], L["The ticked bots leave their spots and follow you again."])

    if ForeverBotsDB then
        if ForeverBotsDB.placeShape then P.shape = ForeverBotsDB.placeShape end
        if ForeverBotsDB.placeGap then P.gap = ForeverBotsDB.placeGap end
    end
    p:HookScript("OnShow", function() P.AskRoles(); P.Refresh() end)
    p:HookScript("OnUpdate", function(self, elapsed)
        self.fbTick = (self.fbTick or 0) + elapsed
        if self.fbTick >= 1 then self.fbTick = 0; P.Refresh() end
    end)
    P.Refresh()
end
