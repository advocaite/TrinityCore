--[[
ForeverBots_Spec: the Spec page of the bot manager. The picked bot's spec and role, buttons to give it another role (its
talents follow the role) or to hand the role back to its talents, and Re-pick talents (the starter build of its spec).
The bot's own orders setrole and talents apply, sent hidden.
]]

local ADDON, ns = ...
local Net, W = ns.Net, ns.Windows
local L = ns.L or setmetatable({}, { __index = function(_, k) return (k:gsub("^short:", "")) end })   -- text by client language

-- The module keys roles and rotations on the retail spec ids (Api/ClassicSpec.h), also for the Forever client's one spec per class.
local SPEC_NAME = {
    [71] = "Arms", [72] = "Fury", [73] = "Protection",
    [65] = "Holy", [66] = "Protection", [70] = "Retribution",
    [253] = "Beast Mastery", [254] = "Marksmanship", [255] = "Survival",
    [259] = "Assassination", [260] = "Combat", [261] = "Subtlety",
    [256] = "Discipline", [257] = "Holy", [258] = "Shadow",
    [262] = "Elemental", [263] = "Enhancement", [264] = "Restoration",
    [62] = "Arcane", [63] = "Fire", [64] = "Frost",
    [265] = "Affliction", [266] = "Demonology", [267] = "Destruction",
    [102] = "Balance", [103] = "Feral", [104] = "Feral (bear)", [105] = "Restoration",
}
for k, v in pairs(SPEC_NAME) do SPEC_NAME[k] = L[v] end
local CLASS_ROLES = {
    WARRIOR = { "tank", "dps" }, PALADIN = { "tank", "healer", "dps" }, SHAMAN = { "healer", "dps" }, HUNTER = { "dps" },
    ROGUE = { "dps" }, PRIEST = { "healer", "dps" }, MAGE = { "dps" }, WARLOCK = { "dps" }, DRUID = { "tank", "healer", "dps" },
}
local ROLE_LABEL = { tank = L["Tank"], healer = L["Healer"], dps = L["Damage"] }
local ROLE_TOKEN = { TANK = "tank", HEALER = "healer", DPS = "dps" }

local pane

local function Order(verb, ...)
    local bot = W.SelectedBot()
    if not (bot and bot.online) then return end
    Net.Cmd(bot.name, verb, ...)
    -- the roster carries spec and role: ask for it again once the bot has changed
    if C_Timer and C_Timer.After then C_Timer.After(1.5, W.RequestRoster) end
end

local function Refresh(bot)
    if not pane then return end
    if not bot then
        pane.text:SetText("")
    else
        local spec = SPEC_NAME[bot.specId or 0]
        local role = ROLE_TOKEN[bot.role or ""]
        local lines = {
            W.ClassColorText(bot.class, bot.name),
            L["Spec: %s"]:format(spec or L["unknown"]),
            L["Role: %s"]:format(role and ROLE_LABEL[role] or L["unknown"]),
        }
        if not bot.online then lines[#lines + 1] = "|cff9d9d9d" .. L["Offline: call the bot to change it."] .. "|r" end
        pane.text:SetText(table.concat(lines, "\n"))
    end
    local roles = bot and CLASS_ROLES[bot.class] or {}
    for i, b in ipairs(pane.roleButtons) do
        local r = roles[i]
        b.role = r
        if r then
            b:SetText(ROLE_LABEL[r])
            b:SetEnabled(bot.online and true or false)
            if ROLE_TOKEN[bot.role or ""] == r then b:LockHighlight() else b:UnlockHighlight() end
            b:Show()
        else
            b:Hide()
        end
    end
    local online = bot and bot.online or false
    pane.auto:SetEnabled(online)
    pane.talents:SetEnabled(online)
end

local function Build(p)
    pane = p
    p.text = p:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    p.text:SetPoint("TOPLEFT", 0, 0)
    p.text:SetWidth(288)
    p.text:SetJustifyH("LEFT")
    p.text:SetSpacing(4)
    local head = p:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    head:SetPoint("TOPLEFT", 0, -84)
    head:SetText(L["Give it a role"])
    p.roleButtons = {}
    for i = 1, 3 do
        local b = W.MakeButton(p, "", 90)
        b:SetPoint("TOPLEFT", p, "TOPLEFT", (i - 1) * 96, -102)
        b:SetScript("OnClick", function(self) if self.role then Order("setrole", self.role) end end)
        W.Tip(b, L["Role"], L["The bot plays this role from now on and picks the talents for it. Kept across relog and Call."])
        p.roleButtons[i] = b
    end
    p.auto = W.MakeButton(p, L["From talents"], 112)
    p.auto:SetPoint("TOPLEFT", p, "TOPLEFT", 0, -134)
    p.auto:SetScript("OnClick", function() Order("setrole", "clear") end)
    W.Tip(p.auto, L["From talents"], L["Drops the role you gave it: the bot's talents decide its role again."])
    p.talents = W.MakeButton(p, L["Re-pick talents"], 112)
    p.talents:SetPoint("TOPLEFT", p, "TOPLEFT", 118, -134)
    p.talents:SetScript("OnClick", function() Order("talents", "apply") end)
    W.Tip(p.talents, L["Re-pick talents"], L["The bot takes the starter talent build of its spec again (not in combat)."])
end

W.AddTab({
    key = "spec", label = L["Spec"], every = 5, build = Build,
    request = function(bot) Refresh(bot) end,
    refresh = Refresh,
})
