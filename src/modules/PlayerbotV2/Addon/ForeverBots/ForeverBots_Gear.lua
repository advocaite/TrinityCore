--[[
ForeverBots_Gear: the Gear page of the bot manager. What the picked bot wears and carries, with the real item tooltips.
Click a bag item to have the bot wear it; shift-click puts its link in chat. Equip upgrades, Sell grey and Repair are the
bot's own orders, sent hidden; the bot confirms by whisper as it does for a typed order.
]]

local ADDON, ns = ...
local Net, W = ns.Net, ns.Windows
local L = ns.L or setmetatable({}, { __index = function(_, k) return (k:gsub("^short:", "")) end })   -- text by client language
local G = {}
ns.Gear = G

local SLOT_ORDER = { 0, 1, 2, 14, 4, 3, 18, 8, 9, 5, 6, 7, 10, 11, 12, 13, 15, 16, 17 }
local SLOT_ART = { [0] = "Head", "Neck", "Shoulder", "Shirt", "Chest", "Waist", "Legs", "Feet", "Wrists", "Hands", "Finger", "Finger",
                   "Trinket", "Trinket", "Chest", "MainHand", "SecondaryHand", "Ranged", "Tabard" }
local SLOT_NAME = { [0] = "Head", "Neck", "Shoulders", "Shirt", "Chest", "Waist", "Legs", "Feet", "Wrists", "Hands", "Finger", "Finger",
                    "Trinket", "Trinket", "Back", "Main hand", "Off hand", "Ranged", "Tabard" }
for k, v in pairs(SLOT_NAME) do SLOT_NAME[k] = L[v] end
local WORN_SIZE, WORN_STEP, WORN_PER_ROW = 32, 36, 8
local BAG_SIZE, BAG_STEP, BAG_PER_ROW, BAG_ROWS = 28, 31, 9, 4

-- An item string the client's tooltip reads: item:id:enchant:gem1:gem2:gem3:gem4:suffix:unique:linkLevel:spec:modifiers:context:
-- numBonus:bonus...
function G.ItemString(entry, enchant, bonuses)
    local list = {}
    for b in tostring(bonuses or ""):gmatch("%d+") do list[#list + 1] = b end
    local ench = (tonumber(enchant) or 0) > 0 and tostring(enchant) or ""
    local s = "item:" .. entry .. ":" .. ench .. "::::::::::" .. ":" .. (#list > 0 and #list or "")
    if #list > 0 then s = s .. ":" .. table.concat(list, ":") end
    return s
end

-- GEAR_RESP: "<id>|<avg ilvl>|<copper>|<free>|E,slot,entry,ilvl,dur,ench,bonus,quality|...|B,bag,slot,entry,count,quality,ilvl,ench,bonus|...".
function G.Parse(fields)
    local g = { id = tonumber(fields and fields[1]), worn = {}, bags = {} }
    if not fields or not fields[2] then return g end
    g.avg, g.money, g.free = tonumber(fields[2]) or 0, tonumber(fields[3]) or 0, tonumber(fields[4]) or 0
    for i = 5, #fields do
        local f = { strsplit(",", fields[i]) }
        if f[1] == "E" then
            local slot = tonumber(f[2])
            if slot then
                g.worn[slot] = { entry = tonumber(f[3]), ilvl = tonumber(f[4]) or 0, dur = tonumber(f[5]) or 100,
                                 link = G.ItemString(f[3], f[6], f[7]), quality = tonumber(f[8]) }
            end
        elseif f[1] == "B" then
            g.bags[#g.bags + 1] = { bag = tonumber(f[2]), slot = tonumber(f[3]), entry = tonumber(f[4]), count = tonumber(f[5]) or 1,
                                    quality = tonumber(f[6]) or 1, ilvl = tonumber(f[7]) or 0, link = G.ItemString(f[4], f[8], f[9]) }
        end
    end
    -- best first: quality, then item level
    table.sort(g.bags, function(a, b)
        if a.quality ~= b.quality then return a.quality > b.quality end
        if a.ilvl ~= b.ilvl then return a.ilvl > b.ilvl end
        return (a.entry or 0) < (b.entry or 0)
    end)
    return g
end

local function Money(copper)
    copper = tonumber(copper) or 0
    local gold, silver, c = math.floor(copper / 10000), math.floor(copper / 100) % 100, copper % 100
    if gold > 0 then return L["%dg %ds %dc"]:format(gold, silver, c) end
    if silver > 0 then return L["%ds %dc"]:format(silver, c) end
    return L["%dc"]:format(c)
end
local function ItemIcon(entry)
    if C_Item and C_Item.GetItemIconByID then
        local ok, icon = pcall(C_Item.GetItemIconByID, entry)
        if ok and icon then return icon end
    end
    if GetItemIcon then
        local ok, icon = pcall(GetItemIcon, entry)
        if ok and icon then return icon end
    end
    return "Interface\\Icons\\INV_Misc_QuestionMark"
end
local function Equippable(entry)
    local fn = (C_Item and C_Item.IsEquippableItem) or IsEquippableItem
    if not fn then return true end
    local ok, res = pcall(fn, entry)
    return not ok or res and true or false
end
local function QualityColor(q)
    local c = q and ITEM_QUALITY_COLORS and ITEM_QUALITY_COLORS[q]
    if c then return c.r, c.g, c.b end
    return nil
end

local pane
G.data, G.offset = nil, 0

local function SetSlot(btn, item, emptyArt)
    btn.item = item
    if item then
        btn.icon:SetTexture(ItemIcon(item.entry))
        btn.icon:SetDesaturated(false)
        if item.dur and item.dur <= 25 then btn.icon:SetVertexColor(1, 0.35, 0.35) else btn.icon:SetVertexColor(1, 1, 1) end
        local r, g, b = QualityColor(item.quality)
        if r and (item.quality or 0) >= 2 then btn.border:SetVertexColor(r, g, b); btn.border:Show() else btn.border:Hide() end
        if btn.count then btn.count:SetText((item.count or 1) > 1 and item.count or "") end
    else
        btn.icon:SetTexture(emptyArt or "Interface\\PaperDoll\\UI-Backpack-EmptySlot")
        btn.icon:SetVertexColor(1, 1, 1)
        btn.border:Hide()
        if btn.count then btn.count:SetText("") end
    end
end

function G.Refresh(bot)
    if not pane then return end
    local g = G.data
    if not bot then
        pane.summary:SetText("")
    elseif not bot.online then
        pane.summary:SetText("|cff9d9d9d" .. L["Offline: call the bot to see its gear."] .. "|r")
    elseif not (g and g.id == bot.id) then
        pane.summary:SetText("|cff9d9d9d" .. L["Asking the server..."] .. "|r")
    elseif not g.avg then
        pane.summary:SetText("|cffff6060" .. L["The server sent no gear for this bot."] .. "|r")
    else
        pane.summary:SetText(L["Item level %d, %d free bag slots, %s"]:format(g.avg, g.free, Money(g.money)))
    end
    local have = g and bot and g.id == bot.id
    for i, slot in ipairs(SLOT_ORDER) do
        SetSlot(pane.worn[i], have and g.worn[slot] or nil, "Interface\\PaperDoll\\UI-PaperDoll-Slot-" .. SLOT_ART[slot])
    end
    local bags = have and g.bags or {}
    local maxOffset = math.max(0, math.ceil(#bags / BAG_PER_ROW) - BAG_ROWS)
    if G.offset > maxOffset then G.offset = maxOffset end
    for i, btn in ipairs(pane.bag) do
        local item = bags[i + G.offset * BAG_PER_ROW]
        SetSlot(btn, item)
        if item or have then btn:Show() else btn:Hide() end
    end
    pane.bagLabel:SetText(L["Bags"] .. ((have and #bags > 0) and (" (" .. #bags .. ")" .. (maxOffset > 0 and L[", scroll for more"] or "")) or ""))
    for _, b in ipairs(pane.buttons) do
        if bot and bot.online then b:Enable() else b:Disable() end
    end
end

function G.Request(bot)
    if not (bot and bot.online) then G.Refresh(bot) return end
    local id = bot.id
    Net.Request("GEAR_REQ", { tostring(id) }, function(fields)
        if not fields then return end
        local g = G.Parse(fields)
        if g.id ~= id then return end
        G.data = g
        local now = W.SelectedBot()
        if now and now.id == id then G.Refresh(now) end
    end)
end

-- An order to the picked bot, then its gear again once it has had time to act.
local function Order(verb, ...)
    local bot = W.SelectedBot()
    if not (bot and bot.online) then return end
    Net.Cmd(bot.name, verb, ...)
    if C_Timer and C_Timer.After then C_Timer.After(1.5, function() G.Request(W.SelectedBot()) end) end
end

local function ItemTooltip(btn, slotName)
    local item = btn.item
    GameTooltip:SetOwner(btn, "ANCHOR_RIGHT")
    if not item then
        GameTooltip:SetText(slotName or L["Empty"])
        GameTooltip:Show()
        return
    end
    local ok = pcall(GameTooltip.SetHyperlink, GameTooltip, item.link)
    if not ok then GameTooltip:SetText(L["Item %s"]:format(tostring(item.entry))) end
    if item.ilvl and item.ilvl > 0 then GameTooltip:AddLine(L["Item level %s on this bot"]:format(item.ilvl), 0.6, 0.8, 1) end
    if item.dur and item.dur < 100 then GameTooltip:AddLine(L["Durability %s%%"]:format(item.dur), 1, item.dur <= 25 and 0.3 or 1, item.dur <= 25 and 0.3 or 1) end
    if btn.isBag then
        GameTooltip:AddLine(Equippable(item.entry) and L["Click: the bot wears it. Shift-click: link in chat."] or L["Shift-click: link in chat."], 0.5, 1, 0.5, true)
    end
    GameTooltip:Show()
end

local function MakeSlot(parent, size)
    local b = CreateFrame("Button", nil, parent)
    b:SetSize(size, size)
    b.icon = b:CreateTexture(nil, "ARTWORK")
    b.icon:SetAllPoints()
    b.border = b:CreateTexture(nil, "OVERLAY")
    b.border:SetAllPoints()
    b.border:SetTexture("Interface\\Common\\WhiteIconFrame")
    b.border:Hide()
    b:SetHighlightTexture("Interface\\Buttons\\ButtonHilight-Square", "ADD")
    b:RegisterForClicks("LeftButtonUp")
    return b
end

local function Build(p)
    pane = p
    p.summary = p:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    p.summary:SetPoint("TOPLEFT", 0, 0)
    p.summary:SetWidth(288)
    p.summary:SetJustifyH("LEFT")
    local worn = p:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    worn:SetPoint("TOPLEFT", 0, -16)
    worn:SetText(L["Worn"])
    p.worn = {}
    for i, slot in ipairs(SLOT_ORDER) do
        local b = MakeSlot(p, WORN_SIZE)
        b:SetPoint("TOPLEFT", p, "TOPLEFT", ((i - 1) % WORN_PER_ROW) * WORN_STEP, -32 - math.floor((i - 1) / WORN_PER_ROW) * WORN_STEP)
        b:SetScript("OnEnter", function(self) ItemTooltip(self, SLOT_NAME[slot]) end)
        b:SetScript("OnLeave", function() GameTooltip:Hide() end)
        b:SetScript("OnClick", function(self)
            if self.item and IsModifiedClick and IsModifiedClick("CHATLINK") and ChatEdit_InsertLink then
                local _, link = GetItemInfo(self.item.link)
                if link then ChatEdit_InsertLink(link) end
            end
        end)
        p.worn[i] = b
    end
    local bagTop = -32 - 3 * WORN_STEP - 4
    p.bagLabel = p:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    p.bagLabel:SetPoint("TOPLEFT", 0, bagTop)
    p.bag = {}
    local bagArea = CreateFrame("Frame", nil, p)
    bagArea:SetPoint("TOPLEFT", p, "TOPLEFT", 0, bagTop - 16)
    bagArea:SetSize(BAG_PER_ROW * BAG_STEP, BAG_ROWS * BAG_STEP)
    bagArea:EnableMouseWheel(true)
    bagArea:SetScript("OnMouseWheel", function(_, delta)
        G.offset = math.max(0, G.offset - delta)
        G.Refresh(W.SelectedBot())
    end)
    for i = 1, BAG_PER_ROW * BAG_ROWS do
        local b = MakeSlot(bagArea, BAG_SIZE)
        b.isBag = true
        b:SetPoint("TOPLEFT", bagArea, "TOPLEFT", ((i - 1) % BAG_PER_ROW) * BAG_STEP, -math.floor((i - 1) / BAG_PER_ROW) * BAG_STEP)
        b.count = b:CreateFontString(nil, "OVERLAY", "NumberFontNormal")
        b.count:SetPoint("BOTTOMRIGHT", -1, 1)
        b:SetScript("OnEnter", function(self) ItemTooltip(self) end)
        b:SetScript("OnLeave", function() GameTooltip:Hide() end)
        b:SetScript("OnClick", function(self)
            local item = self.item
            if not item then return end
            if IsModifiedClick and IsModifiedClick("CHATLINK") and ChatEdit_InsertLink then
                local _, link = GetItemInfo(item.link)
                if link then ChatEdit_InsertLink(link) end
                return
            end
            if Equippable(item.entry) then Order("equip", item.entry) end
        end)
        p.bag[i] = b
    end
    p.buttons = {}
    local defs = {
        { L["Equip upgrades"], L["The bot wears every bag item that is better for it, and says what it changed."], function() Order("equip", "upgrades") end },
        { L["Sell grey"], L["The bot sells its grey items to your selected vendor, or the nearest one it sees within 60 yards."], function() Order("sell", "grey") end },
        { L["Repair"], L["The bot repairs at your selected vendor, or the nearest repair vendor it sees."], function() Order("repair") end },
    }
    for i, d in ipairs(defs) do
        local b = W.MakeButton(p, d[1], 94)
        b:SetPoint("BOTTOMLEFT", p, "BOTTOMLEFT", (i - 1) * 97, 0)
        b:SetScript("OnClick", d[3])
        W.Tip(b, d[1], d[2])
        p.buttons[i] = b
    end
end

W.AddTab({
    key = "gear", label = L["short:Gear"], every = 10, build = Build,
    request = G.Request,
    refresh = function(bot)
        if not (G.data and bot and G.data.id == bot.id) then G.offset = 0 end
        G.Refresh(bot)
    end,
})
