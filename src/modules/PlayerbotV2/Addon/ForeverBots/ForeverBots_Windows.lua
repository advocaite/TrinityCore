--[[
ForeverBots_Windows: the bot manager window (/fb bots, or "Manage bots" on the Bots tab).

Left: every companion you own, online or not (class, level, role, where it is). Right: the picked bot, with buttons for it.
Everything is asked with hidden requests (ForeverBots_Net.lua); the buttons send the same ".bot" lines as the main window,
as hidden add-on messages, so nothing is said in chat. The server's answer to a button still shows in chat.
]]

local ADDON, ns = ...
local Net = ns.Net
local L = ns.L or setmetatable({}, { __index = function(_, k) return (k:gsub("^short:", "")) end })   -- text by client language
local W = {}
ns.Windows = W

local ROWS, ROW_H = 14, 22
local LIST_W, PANE_W = 236, 300
local CLASS_ICON_TEX = "Interface\\GLUES\\CHARACTERCREATE\\UI-CHARACTERCREATE-CLASSES"
local ROLE_TEXT = { TANK = L["Tank"], HEALER = L["Healer"], DPS = L["Damage"] }

local function Print(msg) if ns.Print then ns.Print(msg) end end
local function Queue(text) if ns.Queue then ns.Queue(text) end end

local function ClassName(token)
    local name = LOCALIZED_CLASS_NAMES_MALE and LOCALIZED_CLASS_NAMES_MALE[token]
    return name or (token and (token:sub(1, 1) .. token:sub(2):lower())) or "?"
end
local function ClassColorText(token, text)
    local c = RAID_CLASS_COLORS and RAID_CLASS_COLORS[token]
    if not c then return text end
    return string.format("|cff%02x%02x%02x%s|r", (c.r or 1) * 255, (c.g or 1) * 255, (c.b or 1) * 255, text)
end
local function SpecName(id)
    id = tonumber(id)
    if not id or id == 0 or not GetSpecializationInfoByID then return nil end
    local ok, _, name = pcall(GetSpecializationInfoByID, id)
    return ok and type(name) == "string" and name ~= "" and name or nil
end
local function AreaName(id)
    id = tonumber(id)
    if not id or id == 0 then return nil end
    if C_Map and C_Map.GetAreaInfo then
        local ok, name = pcall(C_Map.GetAreaInfo, id)
        if ok and type(name) == "string" and name ~= "" then return name end
    end
    return nil
end

-- One roster record: "id,name,level,class,race,role,zone,online,hp,mana,dist,intent,rule,group,spec,headless,saved,grouped,specid".
-- Fields 17 to 19 come from servers that know the ForeverBots window; without them saved and grouped are unknown (nil).
function W.ParseRoster(fields)
    local bots = {}
    local n = tonumber(fields and fields[1]) or 0
    for i = 2, n + 1 do
        local rec = fields[i]
        if type(rec) == "string" then
            local f = { strsplit(",", rec) }
            local b = {
                id = tonumber(f[1]), name = f[2], level = tonumber(f[3]) or 0, class = f[4], race = f[5], role = f[6],
                online = f[8] == "1", hp = tonumber(f[9]) or 0, mana = tonumber(f[10]) or 0, dist = tonumber(f[11]) or -1,
                headless = f[16] == "1",
            }
            if f[17] then b.saved = f[17] == "1" end
            if f[18] then b.grouped = f[18] == "1" end
            b.specId = tonumber(f[19])
            if b.id and b.name then bots[#bots + 1] = b end
        end
    end
    -- in your group first, then online, then saved, then the rest; by name inside each
    local function rank(b) return (b.grouped and 0) or (b.online and 1) or (b.saved and 2) or 3 end
    table.sort(bots, function(a, b)
        local ra, rb = rank(a), rank(b)
        if ra ~= rb then return ra < rb end
        return a.name < b.name
    end)
    return bots
end

-- BOT_DETAIL_RESP: "<id>|key=value|key=value...".
function W.ParseDetail(fields)
    local d = { id = tonumber(fields and fields[1]) }
    for i = 2, #(fields or {}) do
        local k, v = fields[i]:match("^([%w_]+)=(.*)$")
        if k then d[k] = v end
    end
    return d
end

local function StatusText(b)
    if b.grouped then return "|cff40ff40" .. L["in your group"] .. "|r" end
    if b.online then return "|cffffd100" .. L["online"] .. "|r" .. ((b.dist and b.dist >= 0) and (", " .. L["%d yd"]:format(b.dist)) or "") end
    if b.saved then return "|cff9d9d9d" .. L["offline, saved"] .. "|r" end
    return "|cff9d9d9d" .. L["offline"] .. "|r"
end

------------------------------------------------------------------------------------------------ window
local win
W.bots, W.offset, W.selected, W.detail = {}, 0, nil, nil
W.tab, W.tabDefs = "details", {}

-- A page of the right side beside Details, from another file of the add-on: { key, label, build(pane), request(bot),
-- refresh(bot), every = seconds between requests while the page is open }. Added before the window is first opened.
function W.AddTab(def) W.tabDefs[#W.tabDefs + 1] = def end
local function TabDef(key)
    for _, t in ipairs(W.tabDefs) do
        if t.key == key then return t end
    end
    return nil
end

local function MakeButton(parent, text, w)
    local b = CreateFrame("Button", nil, parent, "UIPanelButtonTemplate")
    b:SetSize(w or 90, 22)
    b:SetText(text)
    return b
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

local function SelectedBot()
    for _, b in ipairs(W.bots) do
        if b.id == W.selected then return b end
    end
    return nil
end
W.SelectedBot, W.MakeButton, W.Tip, W.ClassColorText = SelectedBot, MakeButton, Tip, ClassColorText

function W.RefreshList()
    if not win then return end
    local max = math.max(0, #W.bots - ROWS)
    if W.offset > max then W.offset = max end
    for i = 1, ROWS do
        local row = win.rows[i]
        local b = W.bots[i + W.offset]
        row.bot = b
        if b then
            local c = CLASS_ICON_TCOORDS and CLASS_ICON_TCOORDS[b.class]
            if c then row.icon:SetTexCoord(c[1], c[2], c[3], c[4]); row.icon:Show() else row.icon:Hide() end
            row.name:SetText(ClassColorText(b.class, b.name))
            row.info:SetText(b.level .. " " .. (ROLE_TEXT[b.role] or ""))
            row.icon:SetDesaturated(not b.online)
            row.name:SetAlpha(b.online and 1 or 0.6)
            if b.id == W.selected then row:LockHighlight() else row:UnlockHighlight() end
            row:Show()
        else
            row:Hide()
        end
    end
    local inGroup, online = 0, 0
    for _, b in ipairs(W.bots) do
        if b.grouped then inGroup = inGroup + 1 end
        if b.online then online = online + 1 end
    end
    win.count:SetText(L["%d bots, %d online, %d with you"]:format(#W.bots, online, inGroup))
end

function W.RefreshDetail()
    if not win then return end
    local p = win.pane
    local b = SelectedBot()
    if not b then
        p.title:SetText("")
        p.lines:SetText(#W.bots == 0 and L["You have no bots yet. Add one on the Bots tab of the main window."] or L["Pick a bot on the left."])
        for _, btn in ipairs(p.buttons) do btn:Hide() end
        return
    end
    local d = (W.detail and W.detail.id == b.id) and W.detail or {}
    p.title:SetText(ClassColorText(b.class, b.name))
    local spec = SpecName(d.spec_id or b.specId)
    local lines = {}
    lines[#lines + 1] = L["Level %s %s"]:format(b.level, ClassName(b.class)) .. (spec and (", " .. spec) or "") .. ", " .. (ROLE_TEXT[b.role] or "?")
    lines[#lines + 1] = StatusText(b)
    if b.online then
        local where = AreaName(d.zone_id)
        local area = AreaName(d.area_id)
        if where then lines[#lines + 1] = L["In %s"]:format(where .. ((area and area ~= where) and (", " .. area) or "")) end
        local hp, hpMax = tonumber(d.hp), tonumber(d.hp_max)
        if hp and hpMax and hpMax > 0 then
            lines[#lines + 1] = L["Health %d / %d (%d%%)"]:format(hp, hpMax, math.floor(100 * hp / hpMax + 0.5))
        else
            lines[#lines + 1] = L["Health %s%%"]:format(b.hp)
        end
        local mana, manaMax = tonumber(d.mana), tonumber(d.mana_max)
        if mana and manaMax and manaMax > 0 then
            lines[#lines + 1] = L["Mana %d / %d (%d%%)"]:format(mana, manaMax, math.floor(100 * mana / manaMax + 0.5))
        end
        local xp, xpMax = tonumber(d.xp), tonumber(d.xp_for_level)
        if xp and xpMax and xpMax > 0 then
            lines[#lines + 1] = L["Experience %d / %d (%d%%)"]:format(xp, xpMax, math.floor(100 * xp / xpMax))
        end
        if d.last_rule and d.last_rule ~= "" and d.last_rule ~= "-" then
            -- the bot's last rule, "idle:travel_to_hub" -> "travel to hub"
            lines[#lines + 1] = "|cff9d9d9d" .. L["Doing: %s"]:format((d.last_rule:gsub("^%a+:", ""):gsub("_", " "))) .. "|r"
        end
    end
    if b.saved == true then lines[#lines + 1] = L["On your saved list"] end
    p.lines:SetText(table.concat(lines, "\n"))

    local bt = p.byKey
    bt.call:SetText(b.online and L["Summon"] or L["Call"])
    bt.call:Show()
    if b.online then bt.dismiss:Show() else bt.dismiss:Hide() end
    if b.saved == nil then bt.save:Hide() else bt.save:SetText(b.saved and L["Unsave"] or L["Save"]); bt.save:Show() end
    if b.online then bt.regear:Show() else bt.regear:Hide() end
    bt.delete:Show()
end

function W.RequestRoster()
    Net.Request("ROSTER_REQ", { "1" }, function(fields, why)
        if not win then return end
        if not fields then
            win.count:SetText("|cffff6060" .. L["No answer from the server (%s)."]:format(tostring(why)) .. "|r")
            return
        end
        W.bots = W.ParseRoster(fields)
        local before = W.selected
        if W.selected and not SelectedBot() then W.selected, W.detail = nil, nil end
        if not W.selected and W.bots[1] then W.selected = W.bots[1].id end
        W.RefreshList()
        W.RefreshDetail()
        W.RefreshTab()
        if W.selected ~= before then W.RequestTab() end
    end)
end

function W.RequestDetail()
    local b = SelectedBot()
    if not (b and b.online) then return end
    local id = b.id
    Net.Request("BOT_DETAIL_REQ", { tostring(id) }, function(fields)
        if not fields or W.selected ~= id then return end
        W.detail = W.ParseDetail(fields)
        W.RefreshDetail()
    end)
end

-- The open page asks for the picked bot (Details: BOT_DETAIL_REQ; the other pages their own request).
function W.RequestTab()
    if W.tab == "details" then W.RequestDetail() return end
    local t = TabDef(W.tab)
    if t and t.request then t.request(SelectedBot()) end
end
function W.RefreshTab()
    local t = TabDef(W.tab)
    if t and t.refresh then t.refresh(SelectedBot()) end
end

function W.ShowTab(key)
    if not win then return end
    W.tab = key
    for k, pane in pairs(W.panes) do
        if k == key then pane:Show() else pane:Hide() end
    end
    for k, b in pairs(win.tabs) do
        if k == key then b:LockHighlight() else b:UnlockHighlight() end
    end
    win.tDetail = 0
    W.RefreshTab()
    W.RequestTab()
end

local function Select(id)
    W.selected, W.detail = id, nil
    W.RefreshList()
    W.RefreshDetail()
    W.RefreshTab()
    W.RequestTab()
end

-- A button's ".bot" line, then a fresh roster a moment later so the window shows what it did.
local function BotLine(verb)
    local b = SelectedBot()
    if not b then return end
    Queue(".bot " .. verb .. " " .. b.name)
    if C_Timer and C_Timer.After then C_Timer.After(1.5, W.RequestRoster) end
end

-- Only a key of the client's table is ours (never assign StaticPopupDialogs itself: see ForeverBots_Quests.lua).
StaticPopupDialogs.FOREVERBOTS_MGR_DELETE = {
    text = L["Delete %s for good? The character and its gear are gone."],
    button1 = YES or "Yes", button2 = NO or "No",
    OnAccept = function(_, data) if data then Queue(".bot delete " .. data); if C_Timer then C_Timer.After(1.5, W.RequestRoster) end end end,
    timeout = 0, whileDead = true, hideOnEscape = true, preferredIndex = 3,
}

local function Build()
    local f
    for _, tmpl in ipairs({ "PortraitFrameTemplate", "ButtonFrameTemplate" }) do
        local ok, fr = pcall(CreateFrame, "Frame", "ForeverBotsManager", UIParent, tmpl)
        if ok and fr then f = fr; break end
    end
    if not f then
        f = CreateFrame("Frame", "ForeverBotsManager", UIParent, BackdropTemplateMixin and "BackdropTemplate" or nil)
        if f.SetBackdrop then
            f:SetBackdrop({ bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background", edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
                            tile = true, tileSize = 32, edgeSize = 24, insets = { left = 6, right = 6, top = 6, bottom = 6 } })
        end
        local close = CreateFrame("Button", nil, f, "UIPanelCloseButton")
        close:SetPoint("TOPRIGHT", -4, -4)
    end
    f:SetSize(LIST_W + PANE_W + 30, 30 + ROWS * ROW_H + 60)
    f:SetFrameStrata("MEDIUM")
    f:SetToplevel(true)
    f:SetClampedToScreen(true)
    f:SetMovable(true)
    f:EnableMouse(true)
    f:RegisterForDrag("LeftButton")
    f:SetScript("OnDragStart", f.StartMoving)
    f:SetScript("OnDragStop", function(self)
        self:StopMovingOrSizing()
        local point, _, rel, x, y = self:GetPoint()
        ForeverBotsDB.managerPos = { point, rel, x, y }
    end)
    local pos = ForeverBotsDB and ForeverBotsDB.managerPos
    if pos then f:SetPoint(pos[1], UIParent, pos[2], pos[3], pos[4]) else f:SetPoint("CENTER", UIParent, "CENTER", 0, 40) end
    if f.SetTitle then pcall(f.SetTitle, f, L["Your bots"])
    elseif f.TitleText then f.TitleText:SetText(L["Your bots"])
    else
        local t = f:CreateFontString(nil, "OVERLAY", "GameFontNormal")
        t:SetPoint("TOP", 0, -10)
        t:SetText(L["Your bots"])
    end
    local portrait = f.PortraitContainer and f.PortraitContainer.portrait or f.portrait
    if portrait and portrait.SetTexture then portrait:SetTexture("Interface\\Icons\\INV_Misc_GroupNeedMore") end
    tinsert(UISpecialFrames, "ForeverBotsManager")

    -- the roster list
    f.rows = {}
    local list = CreateFrame("Frame", nil, f)
    list:SetPoint("TOPLEFT", f, "TOPLEFT", 12, -60)
    list:SetSize(LIST_W, ROWS * ROW_H)
    list:EnableMouseWheel(true)
    list:SetScript("OnMouseWheel", function(_, delta)
        W.offset = math.max(0, W.offset - delta * 3)
        W.RefreshList()
    end)
    for i = 1, ROWS do
        local row = CreateFrame("Button", nil, list)
        row:SetSize(LIST_W, ROW_H)
        row:SetPoint("TOPLEFT", list, "TOPLEFT", 0, -(i - 1) * ROW_H)
        row:SetHighlightTexture("Interface\\QuestFrame\\UI-QuestTitleHighlight", "ADD")
        row.icon = row:CreateTexture(nil, "ARTWORK")
        row.icon:SetSize(18, 18)
        row.icon:SetPoint("LEFT", 2, 0)
        row.icon:SetTexture(CLASS_ICON_TEX)
        row.name = row:CreateFontString(nil, "OVERLAY", "GameFontNormal")
        row.name:SetPoint("LEFT", row.icon, "RIGHT", 6, 0)
        row.name:SetWidth(130)
        row.name:SetJustifyH("LEFT")
        row.name:SetWordWrap(false)
        row.info = row:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
        row.info:SetPoint("RIGHT", -4, 0)
        row.info:SetJustifyH("RIGHT")
        row:SetScript("OnClick", function(self) if self.bot then Select(self.bot.id) end end)
        row:SetScript("OnEnter", function(self)
            local b = self.bot
            if not b then return end
            GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
            GameTooltip:SetText(ClassColorText(b.class, b.name))
            GameTooltip:AddLine(L["Level %s %s"]:format(b.level, ClassName(b.class)) .. ", " .. (ROLE_TEXT[b.role] or "?"), 1, 1, 1)
            GameTooltip:AddLine(StatusText(b), 1, 1, 1)
            GameTooltip:Show()
        end)
        row:SetScript("OnLeave", function() GameTooltip:Hide() end)
        f.rows[i] = row
    end
    f.count = f:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    f.count:SetPoint("TOPLEFT", f, "TOPLEFT", 14, -40)
    f.count:SetWidth(LIST_W - 70)
    f.count:SetJustifyH("LEFT")
    local refresh = MakeButton(f, L["Refresh"], 66)
    refresh:SetPoint("TOPLEFT", f, "TOPLEFT", 12 + LIST_W - 66, -34)
    refresh:SetScript("OnClick", W.RequestRoster)
    Tip(refresh, L["Refresh"], L["Asks the server for your bots again (the window also does it every few seconds)."])

    -- the picked bot
    local p = CreateFrame("Frame", nil, f)
    p:SetPoint("TOPLEFT", f, "TOPLEFT", 24 + LIST_W, -66)
    p:SetSize(PANE_W - 12, ROWS * ROW_H - 6)
    p.title = p:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    p.title:SetPoint("TOPLEFT", 0, 0)
    p.lines = p:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    p.lines:SetPoint("TOPLEFT", p.title, "BOTTOMLEFT", 0, -8)
    p.lines:SetWidth(PANE_W - 16)
    p.lines:SetJustifyH("LEFT")
    p.lines:SetJustifyV("TOP")
    p.lines:SetSpacing(3)
    p.buttons, p.byKey = {}, {}
    local defs = {
        { "call", L["Call"], L["Offline: logs the bot in and brings it to your group. Online: brings it to you."], function()
            local b = SelectedBot()
            if b then BotLine(b.online and "summon" or "call") end
        end },
        { "dismiss", L["Dismiss"], L["Takes the bot out of your group and logs it out. Its gear is kept."], function() BotLine("dismiss") end },
        { "save", L["Save"], L["Puts the bot on your saved list (Call saved brings the list), or takes it off."], function()
            local b = SelectedBot()
            if b then BotLine(b.saved and "unsave" or "save") end
        end },
        { "regear", L["Regear"], L["Re-rolls the bot's gear. Better pieces it already wears stay; replaced ones go to its bags."], function() BotLine("regear") end },
        { "delete", L["Delete"], L["Deletes the bot's character for good (asks first)."], function()
            local b = SelectedBot()
            if b then StaticPopup_Show("FOREVERBOTS_MGR_DELETE", b.name, nil, b.name) end
        end },
    }
    for i, d in ipairs(defs) do
        local b = MakeButton(p, d[2], 90)
        b:SetPoint("BOTTOMLEFT", p, "BOTTOMLEFT", ((i - 1) % 3) * 96, (i <= 3) and 28 or 0)
        b:SetScript("OnClick", d[4])
        Tip(b, d[2], d[3])
        b:Hide()
        p.buttons[#p.buttons + 1] = b
        p.byKey[d[1]] = b
    end
    f.pane = p

    -- tabs over the right side: Details, then the pages the other files added (W.AddTab)
    W.panes = { details = p }
    local tabs = { { key = "details", label = L["Details"] } }
    for _, t in ipairs(W.tabDefs) do
        local pane = CreateFrame("Frame", nil, f)
        pane:SetAllPoints(p)
        pane:Hide()
        t.build(pane)
        W.panes[t.key] = pane
        tabs[#tabs + 1] = t
    end
    f.tabs = {}
    for i, t in ipairs(tabs) do
        local b = MakeButton(f, t.label, 68)
        b:SetPoint("TOPLEFT", f, "TOPLEFT", 24 + LIST_W + (i - 1) * 72, -34)
        b:SetScript("OnClick", function() W.ShowTab(t.key) end)
        f.tabs[t.key] = b
    end
    f.tabs.details:LockHighlight()

    -- while open: the roster every 5 seconds, the open page of the picked bot every 2 (Details) or as the page says
    f:SetScript("OnUpdate", function(self, elapsed)
        self.tRoster = (self.tRoster or 0) + elapsed
        self.tDetail = (self.tDetail or 0) + elapsed
        if self.tRoster >= 5 then self.tRoster = 0; W.RequestRoster() end
        local t = W.tab ~= "details" and TabDef(W.tab)
        if self.tDetail >= ((t and t.every) or 2) then self.tDetail = 0; W.RequestTab() end
    end)
    f:SetScript("OnShow", function(self) self.tRoster, self.tDetail = 0, 0; W.RequestRoster() end)
    f:Hide()
    return f
end

function W.Toggle()
    if not win then win = Build(); W.frame = win end
    if win:IsShown() then win:Hide() else win:Show() end
end
