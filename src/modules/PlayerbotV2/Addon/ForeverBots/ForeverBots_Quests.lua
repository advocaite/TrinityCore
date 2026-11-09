--[[
ForeverBots_Quests: the Quests page of the bot manager. The picked bot's quest log with each objective's progress.
Click a quest to pick it, then Drop (asks first) or Share. Accept and Turn in work on the quest givers next to the bot.
All are the bot's own quest orders, sent hidden; the bot answers by whisper as it does for a typed order.
]]

local ADDON, ns = ...
local Net, W = ns.Net, ns.Windows
local L = ns.L or setmetatable({}, { __index = function(_, k) return (k:gsub("^short:", "")) end })   -- text by client language
local Q = {}
ns.Quests = Q

local LINES, LINE_H = 12, 20

local function Title(id, fallback)
    if C_QuestLog and C_QuestLog.GetTitleForQuestID then
        local ok, t = pcall(C_QuestLog.GetTitleForQuestID, id)
        if ok and type(t) == "string" and t ~= "" then return t end
    end
    return fallback ~= "" and fallback or L["Quest %s"]:format(id)
end

-- QUESTS_RESP: "<id>|Q,quest,status,level,title|O,quest,have,need,text|...". Title and text are last (they may hold commas).
function Q.Parse(fields)
    local d = { id = tonumber(fields and fields[1]), quests = {} }
    local byId = {}
    for i = 2, #(fields or {}) do
        local rec = fields[i]
        local qid, status, level, title = rec:match("^Q,(%d+),(%d+),(%-?%d+),(.*)$")
        if qid then
            local q = { id = tonumber(qid), status = tonumber(status), level = tonumber(level) or 0, title = Title(tonumber(qid), title), objectives = {} }
            d.quests[#d.quests + 1] = q
            byId[q.id] = q
        else
            local oq, have, need, text = rec:match("^O,(%d+),(%-?%d+),(%-?%d+),(.*)$")
            local q = oq and byId[tonumber(oq)]
            if q then q.objectives[#q.objectives + 1] = { have = tonumber(have) or 0, need = tonumber(need) or 0, text = text } end
        end
    end
    return d
end

-- The page shows one line per quest and one per objective under it.
function Q.Lines(d)
    local lines = {}
    for _, q in ipairs(d and d.quests or {}) do
        lines[#lines + 1] = { quest = q }
        for _, o in ipairs(q.objectives) do lines[#lines + 1] = { quest = q, obj = o } end
    end
    return lines
end

local function QuestColor(level)
    if GetQuestDifficultyColor then
        local ok, c = pcall(GetQuestDifficultyColor, level)
        if ok and type(c) == "table" and c.r then return string.format("|cff%02x%02x%02x", c.r * 255, c.g * 255, c.b * 255) end
    end
    return "|cffffd100"
end

local pane
Q.data, Q.offset, Q.picked = nil, 0, nil

local function Picked()
    if not (Q.data and Q.picked) then return nil end
    for _, q in ipairs(Q.data.quests) do
        if q.id == Q.picked then return q end
    end
    return nil
end

function Q.Refresh(bot)
    if not pane then return end
    local have = Q.data and bot and Q.data.id == bot.id
    local lines = have and Q.Lines(Q.data) or {}
    if not bot then pane.summary:SetText("")
    elseif not bot.online then pane.summary:SetText("|cff9d9d9d" .. L["Offline: call the bot to see its quests."] .. "|r")
    elseif not have then pane.summary:SetText("|cff9d9d9d" .. L["Asking the server..."] .. "|r")
    else
        local done = 0
        for _, q in ipairs(Q.data.quests) do if q.status == 1 then done = done + 1 end end
        pane.summary:SetText(#Q.data.quests == 0 and L["No quests."] or L["%d quests, %d ready to hand in"]:format(#Q.data.quests, done))
    end
    local max = math.max(0, #lines - LINES)
    if Q.offset > max then Q.offset = max end
    for i, row in ipairs(pane.rows) do
        local l = lines[i + Q.offset]
        row.line = l
        if not l then
            row:Hide()
        elseif l.obj then
            local o = l.obj
            local full = o.need > 0 and o.have >= o.need
            local text = o.text ~= "" and o.text or L["Objective"]
            row.text:SetText((full and "|cff80ff80" or "|cffbbbbbb") .. "    " .. (o.need > 1 and (o.have .. "/" .. o.need .. " ") or "") .. text .. "|r")
            row:UnlockHighlight()
            row:Show()
        else
            local q = l.quest
            local tag = q.status == 1 and (" |cff40ff40" .. L["(Complete)"] .. "|r") or q.status == 5 and (" |cffff4040" .. L["(Failed)"] .. "|r") or ""
            row.text:SetText(QuestColor(q.level) .. "[" .. q.level .. "] " .. q.title .. "|r" .. tag)
            if q.id == Q.picked then row:LockHighlight() else row:UnlockHighlight() end
            row:Show()
        end
    end
    local online = bot and bot.online
    pane.accept:SetEnabled(online and true or false)
    pane.turnin:SetEnabled(online and true or false)
    local picked = online and Picked()
    pane.drop:SetEnabled(picked and true or false)
    pane.share:SetEnabled(picked and true or false)
end

function Q.Request(bot)
    if not (bot and bot.online) then Q.Refresh(bot) return end
    local id = bot.id
    Net.Request("QUESTS_REQ", { tostring(id) }, function(fields)
        if not fields then return end
        local d = Q.Parse(fields)
        if d.id ~= id then return end
        Q.data = d
        local now = W.SelectedBot()
        if now and now.id == id then Q.Refresh(now) end
    end)
end

local function Order(verb, ...)
    local bot = W.SelectedBot()
    if not (bot and bot.online) then return end
    Net.Cmd(bot.name, verb, ...)
    if C_Timer and C_Timer.After then C_Timer.After(1.5, function() Q.Request(W.SelectedBot()) end) end
end

-- Only a key of the client's table is ours: assigning StaticPopupDialogs itself taints every pop-up the client shows, and the
-- death pop-up's Release Spirit (RepopMe) is then refused as "an action only available to the Blizzard UI" (UBRS wipe 2026-10-28).
StaticPopupDialogs.FOREVERBOTS_MGR_DROPQUEST = {
    text = L["Have %s drop \"%s\"?"],
    button1 = YES or "Yes", button2 = NO or "No",
    OnAccept = function(_, data) if data then Order("drop", "quest", data) end end,
    timeout = 0, whileDead = true, hideOnEscape = true, preferredIndex = 3,
}

local function Build(p)
    pane = p
    p.summary = p:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    p.summary:SetPoint("TOPLEFT", 0, 0)
    p.summary:SetWidth(288)
    p.summary:SetJustifyH("LEFT")
    local list = CreateFrame("Frame", nil, p)
    list:SetPoint("TOPLEFT", p, "TOPLEFT", 0, -16)
    list:SetSize(288, LINES * LINE_H)
    list:EnableMouseWheel(true)
    list:SetScript("OnMouseWheel", function(_, delta)
        Q.offset = math.max(0, Q.offset - delta * 2)
        Q.Refresh(W.SelectedBot())
    end)
    p.rows = {}
    for i = 1, LINES do
        local row = CreateFrame("Button", nil, list)
        row:SetSize(288, LINE_H)
        row:SetPoint("TOPLEFT", list, "TOPLEFT", 0, -(i - 1) * LINE_H)
        row:SetHighlightTexture("Interface\\QuestFrame\\UI-QuestTitleHighlight", "ADD")
        row.text = row:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
        row.text:SetPoint("LEFT", 2, 0)
        row.text:SetWidth(284)
        row.text:SetJustifyH("LEFT")
        row.text:SetWordWrap(false)
        row:SetScript("OnClick", function(self)
            if not self.line then return end
            local q = self.line.quest
            if IsModifiedClick and IsModifiedClick("CHATLINK") and ChatEdit_InsertLink and GetQuestLink then
                local ok, link = pcall(GetQuestLink, q.id)
                if ok and link then ChatEdit_InsertLink(link) return end
            end
            Q.picked = (Q.picked ~= q.id) and q.id or nil
            Q.Refresh(W.SelectedBot())
        end)
        row:SetScript("OnEnter", function(self)
            local l = self.line
            if not l then return end
            GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
            GameTooltip:SetText(l.quest.title, 1, 0.82, 0, true)
            for _, o in ipairs(l.quest.objectives) do
                GameTooltip:AddLine((o.need > 1 and (o.have .. "/" .. o.need .. " ") or "") .. o.text, 1, 1, 1, true)
            end
            GameTooltip:AddLine(L["Click to pick it for Drop or Share."], 0.5, 1, 0.5, true)
            GameTooltip:Show()
        end)
        row:SetScript("OnLeave", function() GameTooltip:Hide() end)
        p.rows[i] = row
    end
    local defs = {
        { "accept", L["Accept"], L["The bot takes the quests the quest givers next to it offer (for the next 45 seconds)."], function() Order("accept") end },
        { "turnin", L["Turn in"], L["The bot hands in its finished quests at the quest giver in reach."], function() Order("turn", "in") end },
        { "share", L["Share"], L["The bot shares the picked quest with your group."], function()
            local q = Picked()
            if q then Order("share", "quest", q.id) end
        end },
        { "drop", L["Drop"], L["The bot abandons the picked quest (asks first)."], function()
            local q, bot = Picked(), W.SelectedBot()
            if q and bot then StaticPopup_Show("FOREVERBOTS_MGR_DROPQUEST", bot.name, q.title, q.id) end
        end },
    }
    for i, d in ipairs(defs) do
        local b = W.MakeButton(p, d[2], 70)
        b:SetPoint("BOTTOMLEFT", p, "BOTTOMLEFT", (i - 1) * 73, 0)
        b:SetScript("OnClick", d[4])
        W.Tip(b, d[2], d[3])
        p[d[1]] = b
    end
end

W.AddTab({
    key = "quests", label = L["Quests"], every = 5, build = Build,
    request = Q.Request,
    refresh = function(bot)
        if not (Q.data and bot and Q.data.id == bot.id) then Q.picked, Q.offset = nil, 0 end
        Q.Refresh(bot)
    end,
})
