--[[
ForeverBots_Net: hidden requests to the server's bot module, for the windows (roster, gear, quests, spec).

The bot module answers add-on messages under the PBC prefix (Session/AddonControl.cpp): the add-on whispers itself a frame
"1|<seq hex>|<part>|<parts>|<TYPE>|<fields>" and the server sends the answer back the same way, hidden from chat. Fields are
joined by "|"; inside a field "\" is written "\\" and "|" is written "\p". Long frames come in parts of 240 bytes.
Requests carry seq below 0x80000000; the server's own pushes (EVENT_PUSH) count from 0x80000000.

Kept out of ForeverBots.lua: that file is at Lua 5.1's limit of 200 locals in one chunk.
]]

local ADDON, ns = ...
local Net = {}
ns.Net = Net

Net.PREFIX = "PBC"
Net.CHUNK = 200          -- payload bytes per part; the envelope adds about 30, the client limit is 255
Net.TIMEOUT = 6          -- seconds before a request's callback gets (nil, "timeout")
Net.GAP, Net.WAIT, Net.TRIES = 0.15, 1.0, 10

local prefixOk = false
do
    local reg = C_ChatInfo and C_ChatInfo.RegisterAddonMessagePrefix or RegisterAddonMessagePrefix
    if reg then
        local ok, res = pcall(reg, Net.PREFIX)
        prefixOk = ok and res ~= false
    end
end
function Net.Ready() return prefixOk end

local function Print(msg)
    if ns.Print then ns.Print(msg) elseif DEFAULT_CHAT_FRAME then DEFAULT_CHAT_FRAME:AddMessage("WoW Forever Bots: " .. msg) end
end

local function IsSecret(v) return issecretvalue ~= nil and issecretvalue(v) end

function Net.Escape(s)
    s = tostring(s or "")
    return (s:gsub("\\", "\\\\"):gsub("|", "\\p"))
end

local function Unescape(s)
    if not s:find("\\", 1, true) then return s end
    local out, i, n = {}, 1, #s
    while i <= n do
        local c = s:sub(i, i)
        if c == "\\" and i < n then
            local nx = s:sub(i + 1, i + 1)
            out[#out + 1] = (nx == "p") and "|" or nx
            i = i + 2
        else
            out[#out + 1] = c
            i = i + 1
        end
    end
    return table.concat(out)
end

-- Split on the "|" that are not escaped, then unescape each field (the server's SplitPipes does the same).
function Net.Split(s)
    local fields, start, i, n = {}, 1, 1, #s
    while i <= n do
        local c = s:sub(i, i)
        if c == "\\" and i < n then
            i = i + 2
        elseif c == "|" then
            fields[#fields + 1] = Unescape(s:sub(start, i - 1))
            start = i + 1
            i = i + 1
        else
            i = i + 1
        end
    end
    fields[#fields + 1] = Unescape(s:sub(start))
    return fields
end
Net.Unescape = Unescape

-- Frames the server sends. Anything else under PBC is our own request coming back to us (the server delivers a whisper to
-- yourself to you as well), which is skipped.
local ANSWERS = { ROSTER_RESP = true, BOT_DETAIL_RESP = true, STATS_RESP = true, PONG = true, ALTS_RESP = true,
                  EVENT_PUSH = true, SUMMON_RESP = true, LOGIN_RESP = true, LOGOUT_RESP = true, SELF_RESP = true,
                  GEAR_RESP = true, BAGS_RESP = true, QUESTS_RESP = true, SPEC_RESP = true, LFG_RESP = true,
                  ASSIGN_RESP = true }
Net.ANSWERS = ANSWERS

Net.seq = math.random(1, 0x3fffffff)
Net.pending = {}         -- seq -> { cb, mtype, at }
Net.parts = {}           -- seq -> { total, got, mtype, [i] = text, at }
Net.listeners = {}       -- mtype -> { fn, ... }
Net.queue, Net.pumping = {}, false

local function Now() return GetTime and GetTime() or 0 end

-- Where a message to the server goes: the group channel when you are in a group, else a whisper to yourself. Client 70658's
-- add-on whispers never reach the server's whisper handling (/fb ping on RC5 and RC6, 2026-10-28), its group add-on messages
-- do (the order buttons); the server takes these prefixes from the sender's own group message as meant for itself.
function ns.SelfChannel()
    if IsInRaid and IsInRaid() then return "RAID" end
    if (IsInGroup and IsInGroup()) or (GetNumPartyMembers and GetNumPartyMembers() > 0) then return "PARTY" end
    return "WHISPER", ns.SafeName and ns.SafeName("player") or UnitName("player")
end

-- One part on the wire, to the server. Returns sent, result code.
local function SendPart(text)
    local send = C_ChatInfo and C_ChatInfo.SendAddonMessage or SendAddonMessage
    local channel, me = ns.SelfChannel()
    if not (prefixOk and send and (me or channel ~= "WHISPER")) then return false, nil end
    local ok, res = pcall(send, Net.PREFIX, text, channel, me)
    if ok and (res == nil or res == true or res == 0 or (Enum and Enum.SendAddonMessageResult and res == Enum.SendAddonMessageResult.Success)) then
        return true
    end
    return false, ok and tonumber(res) or nil
end

-- The client lets about ten add-on messages out at once, then one a second (result 3 or 8 = throttled): parts wait their turn.
local THROTTLED = { [3] = true, [8] = true }
function Net.Pump()
    local head = Net.queue[1]
    if not head then Net.pumping = false return end
    local sent, code = SendPart(head.text)
    if not sent and code and THROTTLED[code] and head.tries < Net.TRIES and C_Timer and C_Timer.After then
        head.tries = head.tries + 1
        C_Timer.After(Net.WAIT, Net.Pump)
        return
    end
    table.remove(Net.queue, 1)
    if not sent then
        local p = Net.pending[head.seq]
        Net.pending[head.seq] = nil
        if p and p.cb then pcall(p.cb, nil, "not sent" .. (code and (" (" .. code .. ")") or "")) end
    end
    if Net.queue[1] and C_Timer and C_Timer.After then
        C_Timer.After(Net.GAP, Net.Pump)
    else
        Net.pumping = false
    end
end

-- Net.Request(mtype, fields, cb): cb(fields, mtype) with the answer's fields, or cb(nil, reason). Returns the seq, or nil when
-- the prefix could not be registered.
function Net.Request(mtype, fields, cb)
    if not prefixOk then
        if cb then pcall(cb, nil, "no prefix") end
        return nil
    end
    Net.seq = (Net.seq % 0x7ffffffe) + 1
    local seq = Net.seq
    local esc = {}
    for i, f in ipairs(fields or {}) do esc[i] = Net.Escape(f) end
    local body = table.concat(esc, "|")
    local total = math.max(1, math.ceil(#body / Net.CHUNK))
    for i = 1, total do
        local piece = body:sub((i - 1) * Net.CHUNK + 1, i * Net.CHUNK)
        Net.queue[#Net.queue + 1] = { seq = seq, tries = 0,
            text = string.format("1|%08x|%d|%d|%s|%s", seq, i, total, mtype, piece) }
    end
    if cb then Net.pending[seq] = { cb = cb, mtype = mtype, at = Now() } end
    if not Net.pumping then
        Net.pumping = true
        Net.Pump()
    end
    return seq
end

-- A bot order without chat: "<bot name|all|tank|healer|dps|class>", verb, args... The bot answers the way it answers a whisper.
function Net.Cmd(addr, verb, ...)
    local fields = { addr, verb }
    for i = 1, select("#", ...) do fields[#fields + 1] = tostring((select(i, ...))) end
    return Net.Request("CMD", fields)
end

-- Every frame of a type, whoever asked (EVENT_PUSH has no request).
function Net.Listen(mtype, fn)
    local l = Net.listeners[mtype] or {}
    l[#l + 1] = fn
    Net.listeners[mtype] = l
end

local function Deliver(seq, mtype, body)
    local fields = Net.Split(body)
    local p = Net.pending[seq]
    if p then
        Net.pending[seq] = nil
        if p.cb then
            local ok, err = pcall(p.cb, fields, mtype)
            if not ok then Print("window error: " .. tostring(err)) end
        end
    end
    for _, fn in ipairs(Net.listeners[mtype] or {}) do pcall(fn, fields, seq) end
end

-- Parses one received part. Returns true when it was a server frame.
function Net.OnMessage(text)
    if type(text) ~= "string" or IsSecret(text) then return false end
    local seqHex, idx, tot, mtype, payload = text:match("^1|(%x+)|(%d+)|(%d+)|([%u_]+)|(.*)$")
    if not seqHex or not ANSWERS[mtype] then return false end
    local seq = tonumber(seqHex, 16)
    idx, tot = tonumber(idx), tonumber(tot)
    if not (seq and idx and tot) or idx < 1 or idx > tot or tot > 64 then return false end
    if tot == 1 then
        Deliver(seq, mtype, payload)
        return true
    end
    local buf = Net.parts[seq]
    if not buf or buf.total ~= tot then
        buf = { total = tot, got = 0, mtype = mtype, at = Now() }
        Net.parts[seq] = buf
    end
    if not buf[idx] then
        buf[idx] = payload
        buf.got = buf.got + 1
    end
    if buf.got == buf.total then
        Net.parts[seq] = nil
        local all = {}
        for i = 1, buf.total do all[i] = buf[i] end
        Deliver(seq, buf.mtype, table.concat(all))
    end
    return true
end

-- Requests nobody answered (an older server without the type, a lost part) get (nil, "timeout") so a window never waits forever.
function Net.Sweep()
    local now = Now()
    -- collected first: a callback may send the next request, and a table may not grow while pairs walks it
    local late = {}
    for seq, p in pairs(Net.pending) do
        if now - p.at > Net.TIMEOUT then late[#late + 1] = seq end
    end
    for _, seq in ipairs(late) do
        local p = Net.pending[seq]
        Net.pending[seq] = nil
        if p and p.cb then pcall(p.cb, nil, "timeout") end
    end
    for seq, buf in pairs(Net.parts) do
        if now - buf.at > Net.TIMEOUT then Net.parts[seq] = nil end
    end
end

-- Answers come as hidden add-on whispers, or, when this client never raises CHAT_MSG_ADDON for them (Classic 70658: /fb ping
-- timed out although the server got the request, 2026-10-28), as system lines starting with SYS_MARKER that the chat frames hide.
-- Net.mode is what the last ping found: nil (not asked yet), "addon", "sys" or "none". Net.seen counts what arrived of each kind.
local SYS_MARKER = "PBC~"
Net.mode, Net.seen = nil, { addon = 0, sys = 0, echo = 0 }
local function IsMarked(msg) return type(msg) == "string" and not IsSecret(msg) and msg:sub(1, #SYS_MARKER) == SYS_MARKER end

local netFrame = CreateFrame("Frame")
netFrame:RegisterEvent("CHAT_MSG_ADDON")
netFrame:RegisterEvent("CHAT_MSG_SYSTEM")
netFrame:RegisterEvent("PLAYER_LOGIN")
netFrame:SetScript("OnEvent", function(_, event, a1, a2)
    if event == "CHAT_MSG_ADDON" then
        if a1 == Net.PREFIX then
            if Net.OnMessage(a2) then Net.seen.addon = Net.seen.addon + 1 else Net.seen.echo = Net.seen.echo + 1 end
        end
    elseif event == "CHAT_MSG_SYSTEM" then
        if IsMarked(a1) and Net.OnMessage(a1:sub(#SYS_MARKER + 1)) then Net.seen.sys = Net.seen.sys + 1 end
    elseif event == "PLAYER_LOGIN" and C_Timer and C_Timer.After then
        -- find the way answers reach this client, quietly, a moment after login
        C_Timer.After(6, function() Net.Negotiate(false) end)
    end
end)
if ChatFrame_AddMessageEventFilter then
    ChatFrame_AddMessageEventFilter("CHAT_MSG_SYSTEM", function(_, _, msg) return IsMarked(msg) end)
end
if C_Timer and C_Timer.NewTicker then C_Timer.NewTicker(2, Net.Sweep) end

-- Asks for answers as add-on whispers first, then as system lines; the first that comes back is kept (the server remembers the
-- choice for this account). verbose: /fb ping prints what it found.
-- Net.modeGrouped: the last answer was asked for from inside a group (on the group channel). A "none" found alone says little:
-- clients from 70658 on never pass an add-on whisper to yourself to the server, so alone the ping cannot get through at all.
Net.negotiating, Net.modeGrouped, Net.waiters = false, false, {}
local function Grouped() return ns.SelfChannel() ~= "WHISPER" end
local function Settled(mode)
    Net.mode, Net.negotiating = mode, false
    local waiters = Net.waiters
    Net.waiters = {}
    for _, fn in ipairs(waiters) do pcall(fn) end
end
function Net.Negotiate(verbose)
    local t0 = debugprofilestop and debugprofilestop() or (Now() * 1000)
    local function ms() return (debugprofilestop and debugprofilestop() or (Now() * 1000)) - t0 end
    Net.negotiating, Net.modeGrouped = true, Grouped()
    Net.Request("PING", { "0", "addon" }, function(fields)
        if fields then
            Settled("addon")
            if verbose then Print(string.format("server answers window requests (%.0f ms, add-on messages).", ms())) end
            return
        end
        Net.Request("PING", { "0", "sys" }, function(fields2, why)
            if fields2 then
                Settled("sys")
                if verbose then Print(string.format("server answers window requests (%.0f ms, hidden system lines: this client drops add-on whispers).", ms())) end
                return
            end
            Settled("none")
            if verbose then
                Print("the server did not answer the window request (" .. tostring(why) .. "): it may be older than this add-on.")
                Print(string.format("seen since login: %d add-on answers, %d system answers, %d own requests echoed.", Net.seen.addon, Net.seen.sys, Net.seen.echo))
            end
        end)
    end)
end

-- The route is settled: a working one, or "none" found from inside a group (an older server without the window requests).
function Net.ModeKnown()
    if not prefixOk then return true end   -- no prefix, no route: asking again would never change that
    return Net.mode == "addon" or Net.mode == "sys" or (Net.mode == "none" and Net.modeGrouped)
end

-- Calls fn once the route is settled: at once when it is, after the ping running now, or after a new ping (on the group channel
-- when grouped). fn runs at most once per call.
function Net.EnsureMode(fn)
    if Net.ModeKnown() and not Net.negotiating then
        pcall(fn)
        return
    end
    Net.waiters[#Net.waiters + 1] = fn
    if not Net.negotiating then Net.Negotiate(false) end
end

-- /fb ping: one round trip to the bot module, so a missing or old server shows at once.
function Net.Ping() Net.Negotiate(true) end
