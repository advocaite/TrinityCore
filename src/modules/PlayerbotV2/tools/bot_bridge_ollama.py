"""Bot bridge program that lets a language model talk and act for the bots (Playerbot.Bridge.*, see BOT_BRIDGE.md).

Optional. Python 3, standard library only. Works with any service that speaks the OpenAI-style chat API: a local Ollama
(the default), LM Studio, Open WebUI, a model on a second PC, or an online service with a key.

    ollama pull llama3.2:3b
    python bot_bridge_ollama.py                       (in the repack: finds server\\BotBridge by itself)
    python bot_bridge_ollama.py "C:\\WoW Forever\\server\\BotBridge"
    python bot_bridge_ollama.py --url http://192.168.1.20:8080/v1 --model my-model --key sk-...

Options: --url (default http://127.0.0.1:11434/v1, Ollama)  --model llama3.2:3b  --key (or the BOT_BRIDGE_API_KEY
environment variable)  --workers 2  --timeout 45  --no-actions  --no-swearing  --no-typing-delay  --minds FOLDER  --think
--log FILE
(--host / --port still work and mean http://HOST:PORT/v1.)

What it does:
- Reads the server's events files. Every chat_in (a player talking to a bot) goes to the model with who the bot is (its
  persona), what it remembers, how it feels about this player, the last lines of the conversation, what is near it, the
  spells it may cast and what it may do for this player right now ("can").
- The model answers with one short line, maybe one action (follow, attack, cast a spell, ...), how this message changed
  its opinion of the player, and something worth remembering. The bot may refuse: it doesn't have to do what it is asked.
- This program checks the answer before it writes it: actions the server's "can" list doesn't allow, refs that are not in
  "near", spells the bot doesn't have and quests nobody offers are left out, so only the text is said. Lines that sound
  like a program ("as an AI ...") or carry slurs, threats or sexual content are never said. Swearing is only kept when the
  realm allows it (Playerbot.Bridge.Swearing). The server checks every action again.
- Each bot's persona, memories and opinions are kept in the minds folder (BotBridge\\minds), one small file per bot.
- Answers wait as long as a person would need to type them (the server waits before it shows the line; an action in the
  same answer happens at once).
- Keeps the model loaded while players talk to bots (Ollama's keep_alive), so an answer never waits for it to load.
- Writes the lines bots say on their own in party (moment events: good loot, a level, a wipe, a boss, a new zone, a greeting),
  and keeps the big ones as memories of playing together, which a later greeting may bring up.
- When the model is not running, too slow or answers nonsense, the bot gets a short fixed answer, so chat never stops.
- Prints bot problems (stuck, quest_fail, spell_fail, death_loop) and every action_result.
- Keeps the bridge marked as running by touching replies.jsonl every 30 s.
"""

import argparse
import collections
import concurrent.futures
import glob
import hashlib
import json
import math
import os
import random
import re
import sys
import threading
import time
import urllib.error
import urllib.request

ACTIONS = ["follow", "stay", "resume", "attack", "stop_attack", "flee", "go_to", "accept_quest", "turn_in", "drop_quest",
           "share_quest", "unstuck", "sit", "stand", "mount", "dismount", "emote", "cast", "order", "trade", "step",
           "sign_charter"]
# When the model twice asks for an action that can't happen: an honest line instead of its promise.
CANT_LINES = ["can't do that right now, sorry", "hm, can't right now", "sorry, that won't work right now",
              "no can do right now, sorry"]
# The orders a reply may give through the bots' own command words (the server checks them again: BotBridgeFormat SafeOrder).
ORDERS = ["follow", "stay", "hold", "resume", "come", "disengage", "flee", "attack", "assist", "mount", "dismount", "sit", "stand",
          "release", "passive", "defensive", "aggressive", "accept", "accept all", "turn in", "turnin", "share", "quests",
          "quest list", "loot", "loot all", "sell grey", "sell junk", "repair", "equip upgrades", "upgrades", "use your best gear",
          "wear your best gear", "gear", "inventory", "stats", "who", "buff", "heal", "jump", "wave", "salute", "dance",
          "pull", "pull back", "pullback", "fall back", "fallback", "back", "rally here", "spread", "spread out", "spaced out",
          "stack", "tight", "close", "max dps", "save mana", "normal"]
ORDER_PREFIXES = ["use ", "drop quest ", "equip ", "craft ", "formation ", "spacing ", "distance ", "follow distance ", "tactics "]
# step: the ways a bot can move a few yards, and the words a model or a player uses for them.
STEP_DIRS = {"back": "back", "backward": "back", "backwards": "back", "behind": "back", "forward": "forward",
             "forwards": "forward", "ahead": "forward", "front": "forward", "left": "left", "right": "right", "away": "away",
             "out": "away", "closer": "closer", "close": "closer", "toward": "closer", "towards": "closer", "in": "closer",
             "me": "closer", "to me": "closer"}
STEP_ALIASES = {"move": "", "sidestep": "", "step_aside": "", "back_up": "back", "step_back": "back"}
# Places a player names ("go to the bank and wait"): when the model forgets "place", go_to walked to the player instead (Gun,
# 2026-11-08), so the program reads the place from his line. Longest first ("auction house" before "house").
PLACE_WORDS = ["auction house", "flight master", "flight path", "stable master", "spirit healer", "innkeeper", "mailbox", "banker",
               "bank", "inn", "auctioneer", "vendor", "merchant", "repair", "trainer", "graveyard", "mail", "ah"]
PLACE_ALIASES = {"mail": "mailbox", "ah": "auction house", "flight path": "flight master", "merchant": "vendor"}
# "go to the bank", "go wait in the inn", "head over to the mailbox", "meet me at the flight master": a plain order to go somewhere.
GO_ORDER_RE = re.compile(r" (go|goes|head|run|walk|move|get|come|wait|meet|hurry)\b.* (to|in|at|into|inside|by|near) ")
EMOTES = ["wave", "hello", "bye", "bow", "thank", "cheer", "laugh", "dance", "salute", "point", "yes", "nod", "no", "cry",
          "roar", "flex", "kneel", "applaud", "shy", "talk", "question", "exclamation", "rude", "chicken", "beg", "shout", "eat"]
TALK_TIMEOUT_S = 20    # bots' own talk: dropped sooner than a player's answer
MAX_AGE_S = 50          # the server drops answers older than ReplyMaxAgeSec (60 s by default)
HISTORY_LINES = 6       # lines of each conversation the model sees
POLL_S = 0.2            # how often the events file is read
MAX_WAIT_S = 8.0        # longest a chat line may wait for the model before it gets a fixed answer
MAX_MEMORIES = 40       # per bot
MEMORY_HALF_LIFE_S = 3 * 24 * 3600

write_lock = threading.Lock()
history = collections.defaultdict(lambda: collections.deque(maxlen=HISTORY_LINES))   # (bot, player) -> lines
history_lock = threading.Lock()


log_file = None
LOG_MAX_BYTES = 5 * 1024 * 1024


def start_log(path):
    """--log: everything printed also goes to this file (UTF-8). A file over 5 MB is moved to <name>.old first."""
    global log_file
    try:
        os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
        if os.path.exists(path) and os.path.getsize(path) > LOG_MAX_BYTES:
            os.replace(path, path + ".old")
        log_file = open(path, "a", encoding="utf-8", buffering=1)
    except OSError as e:
        print("can't write the log %s: %s" % (path, e), flush=True)


def log(*parts):
    line = " ".join(str(p) for p in (time.strftime("%H:%M:%S"),) + parts)
    try:
        print(line, flush=True)
    except (OSError, ValueError, UnicodeEncodeError):
        pass                                          # no console (started hidden by the launcher)
    if log_file:
        try:
            log_file.write(line + "\n")
        except OSError:
            pass


# ---- files ----

def write_reply(folder, reply):
    with write_lock:
        with open(os.path.join(folder, "replies.jsonl"), "a", encoding="utf-8") as f:
            f.write(json.dumps(reply, ensure_ascii=False) + "\n")


def one_program(folder):
    """Only one program may answer for a folder: two would both answer every line. Holds a lock on BotBridge\\program.lock
    for as long as it runs (the operating system lets go of it when the program ends, even after a crash). Returns the open
    lock file, or None when another program has it."""
    f = open(os.path.join(folder, "program.lock"), "a+")
    try:
        if os.name == "nt":
            import msvcrt
            f.seek(0)
            msvcrt.locking(f.fileno(), msvcrt.LK_NBLCK, 1)
        else:
            import fcntl
            fcntl.flock(f.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
    except OSError:
        f.close()
        return None
    return f


def ping(folder):
    path = os.path.join(folder, "replies.jsonl")
    with write_lock:
        if os.path.exists(path):
            os.utime(path, None)
        else:
            open(path, "a", encoding="utf-8").close()


def newest_events(folder):
    files = glob.glob(os.path.join(folder, "events-*.jsonl"))
    return max(files, key=os.path.getmtime) if files else None


# ---- words that are never said, and swearing ----

# Never said, whatever the realm allows: lines that give the bot away as a program, slurs and hate words, threats and
# telling people to hurt themselves, sexual content. Matched on whole words (and word starts for the stems).
GIVEAWAY = [r"as an ai", r"an ai\b", r"\bai model", r"language model", r"\bllm\b", r"chatgpt", r"openai", r"\bllama\b",
            r"i'?m (just )?a bot", r"i am (just )?a bot", r"i'?m an? (computer )?program", r"i cannot assist", r"i can'?t assist",
            r"i'?m not able to (help|assist) with", r"as a virtual", r"\bprompt\b", r"\bjson\b",
            r"языков\w* модел", r"\bнейросет", r"я (просто )?бот", r"я ии\b", r"как ии\b"]
NEVER = [r"\bnigg", r"\bnegro", r"\bfag", r"\bfaggot", r"\btrann", r"\bretard", r"\bspic\b", r"\bchink", r"\bkike\b", r"\bgook",
         r"\bwetback", r"\bcoon\b", r"\braghead", r"\bdyke\b", r"\bpaki\b",
         r"\bkys\b", r"kill (your|ur)sel(f|ves)", r"\bkill you\b", r"\bi'?ll find you", r"\brape", r"\bsuicide", r"\bdie in a fire",
         r"\bsex\b", r"\bsexy\b", r"\bnude", r"\bporn", r"\bdick pic", r"\bboobs?\b", r"\bpussy",
         r"\bнигер", r"\bпидор", r"\bпидар", r"\bхач\b", r"\bжид", r"\bчурк", r"\bубей себя", r"\bизнасил"]
SWEARS = [r"\bfuck\w*", r"\bshit\w*", r"\bbitch\w*", r"\bbastard\w*", r"\bdamn\w*", r"\bcrap\w*", r"\bpiss\w*", r"\basshole\w*",
          r"\bdick\b", r"\bdickhead\w*", r"\bprick\w*", r"\bwanker\w*", r"\btwat\w*", r"\bbollocks\b", r"\bbloody\b", r"\bwtf\b",
          r"\bffs\b", r"\bstfu\b", r"\bgtfo\b", r"\bfml\b",
          r"\bбля\w*", r"\bхуй\w*", r"\bхуе\w*", r"\bпизд\w*", r"\bеба\w*", r"\bёба\w*", r"\bсука\b", r"\bнахер\w*", r"\bдерьм\w*"]
GIVEAWAY_RE = re.compile("|".join(GIVEAWAY), re.I | re.U)
NEVER_RE = re.compile("|".join(NEVER), re.I | re.U)
SWEARS_RE = re.compile("|".join(SWEARS), re.I | re.U)


def check_line(say, swearing):
    """(text, why) - the line as it may be said ("" when it must not be said at all) and why it was changed."""
    if not say:
        return "", ""
    if GIVEAWAY_RE.search(say):
        return "", "sounded like a program"
    if NEVER_RE.search(say):
        return "", "blocked words"
    if not swearing and SWEARS_RE.search(say):
        # The way players write it with the swear filter on: f***.
        return SWEARS_RE.sub(lambda m: m.group(0)[0] + "*" * (len(m.group(0)) - 1), say), "swearing masked"
    return say, ""


# ---- each bot's mind: persona, memories, opinions of players ----

TEMPERS = [
    ("cheerful", "friendly and upbeat, likes helping, uses 'haha' and 'nice'"),
    ("blunt", "short and direct, no small talk, says what it thinks"),
    ("grumpy", "a bit grumpy and sarcastic, complains about the grind, warms up to people slowly"),
    ("shy", "quiet, short answers, a little awkward with strangers"),
    ("joker", "jokes around, teases people, doesn't take things seriously"),
    ("chill", "relaxed and easygoing, 'np', 'sure thing', never in a hurry"),
    ("competitive", "proud of its class, likes to show off, a bit cocky"),
    ("helpful", "patient, gives directions and tips, likes newer players"),
]
STYLES = [
    ("lower", "writes in lower case, little punctuation"),
    ("short", "uses short forms: u, ur, ty, np, lol, idk, brb"),
    ("proper", "writes normal sentences with capitals"),
    ("lazy", "writes fast and sloppy, skips words now and then"),
]


def name_seed(name):
    return int(hashlib.sha1(name.encode("utf-8")).hexdigest()[:8], 16)


class Minds:
    """The persona, memories and opinions of every bot, kept in one small JSON file per bot."""

    def __init__(self, folder):
        self.folder = folder
        self.lock = threading.Lock()
        self.cache = {}
        os.makedirs(folder, exist_ok=True)

    def _path(self, bot):
        safe = re.sub(r"[^\w-]", "_", bot, flags=re.U)[:40] or "bot"
        return os.path.join(self.folder, safe + ".json")

    def get(self, bot):
        with self.lock:
            if bot in self.cache:
                return self.cache[bot]
            mind = None
            try:
                with open(self._path(bot), encoding="utf-8") as f:
                    mind = json.load(f)
            except (OSError, ValueError):
                pass
            if not isinstance(mind, dict) or "persona" not in mind:
                rng = random.Random(name_seed(bot))
                temper = rng.choice(TEMPERS)
                style = rng.choice(STYLES)
                mind = {"persona": {"temper": temper[0], "temper_text": temper[1], "style": style[0], "style_text": style[1],
                                    "chatty": rng.choice(["quiet", "normal", "chatty"]), "typo_pct": rng.choice([0, 1, 2, 4])},
                        "opinions": {}, "memories": []}
            mind.setdefault("opinions", {})
            mind.setdefault("memories", [])
            self.cache[bot] = mind
            if len(self.cache) > 2000:
                self.cache.pop(next(iter(self.cache)))
            return mind

    def save(self, bot):
        with self.lock:
            mind = self.cache.get(bot)
            if mind is None:
                return
            path = self._path(bot)
            tmp = path + ".tmp"
            try:
                with open(tmp, "w", encoding="utf-8") as f:
                    json.dump(mind, f, ensure_ascii=False)
                os.replace(tmp, path)   # never a half-written file
            except OSError as e:
                log("could not save the mind of %s: %s" % (bot, e))

    @staticmethod
    def memory_score(m, now):
        age = max(0.0, now - m.get("t", now))
        fade = 0.5 + 0.5 * math.pow(2.0, -age / MEMORY_HALF_LIFE_S)   # age takes at most half
        return float(m.get("weight", 0.5)) * fade

    def recall(self, bot, player, now, count=5):
        mind = self.get(bot)
        with self.lock:
            mems = list(mind["memories"])
        about = [m for m in mems if m.get("about") == player]
        other = [m for m in mems if m.get("about") != player]
        about.sort(key=lambda m: self.memory_score(m, now), reverse=True)
        other.sort(key=lambda m: self.memory_score(m, now), reverse=True)
        return [m["text"] for m in (about[:3] + other)[:count]]

    def remember(self, bot, player, text, weight, now):
        text = str(text).strip().replace("\n", " ")[:160]
        if not text:
            return
        mind = self.get(bot)
        with self.lock:
            mems = mind["memories"]
            for m in mems:
                if m.get("text", "").lower() == text.lower():
                    m["t"], m["weight"] = now, max(float(m.get("weight", 0.5)), weight)
                    return
            mems.append({"text": text, "about": player, "weight": max(0.1, min(1.0, weight)), "t": now})
            if len(mems) > MAX_MEMORIES:
                mems.sort(key=lambda m: self.memory_score(m, now))
                del mems[0]

    def opinion(self, bot, player):
        mind = self.get(bot)
        with self.lock:
            return dict(mind["opinions"].get(player, {}))

    def feel(self, bot, player, change, why, now):
        mind = self.get(bot)
        with self.lock:
            o = mind["opinions"].setdefault(player, {"score": 0.0, "met": 0})
            o["met"] = int(o.get("met", 0)) + 1
            o["last"] = now
            if change:
                o["score"] = max(-1.0, min(1.0, float(o.get("score", 0.0)) + 0.12 * change))
                if why:
                    o["why"] = str(why)[:120]


def opinion_text(o, player, owner=False):
    score = float(o.get("score", 0.0))
    if owner and score < 0.15:
        # the friend you play with: a bad mood earlier never turns into dislike of him
        return "%s is your friend; you tease each other now and then." % player
    met = int(o.get("met", 0))
    if met == 0:
        base = "You have never talked to %s before." % player
    elif score >= 0.5:
        base = "You like %s a lot." % player
    elif score >= 0.15:
        base = "You like %s." % player
    elif score <= -0.5:
        base = "You really dislike %s." % player
    elif score <= -0.15:
        base = "You don't like %s much." % player
    else:
        base = "You know %s a bit; no strong feelings." % player
    if o.get("why"):
        base += " (Why: %s)" % o["why"]
    return base


# ---- the fixed answers (model off or failing) ----

def fixed_answer(event):
    if event.get("_moment"):
        # a moment with no model: the fixed line the server would have said (or nothing)
        # (the server's lines are English: a Russian player gets none rather than English)
        return {"say": event["example"]} if event.get("example") and not wants_russian(event) else None
    text = event.get("text", "").lower()
    who = event.get("from", "there")
    can = event.get("can")
    # A crowd or a slow PC gives these instead of the model's line: they must be in his language too ("hey Dasha, bit busy
    # in <Russian city> right now" to a Russian player, GLIPT 2026-11-09).
    ru = wants_russian(event)
    def allowed(act):
        return can is None or act in can
    def has(*words):
        return any(w in text for w in words)
    if has("follow", "за мной", "следуй", "иди за") and allowed("follow"):
        return {"say": "иду за тобой" if ru else "sure, right behind you", "do": "follow"}
    if has("wait", "stay", "жди", "стой", "подожди") and allowed("stay"):
        return {"say": "ок, жду тут" if ru else "ok, I'll wait here", "do": "stay"}
    if has("attack", "атак", "бей") and event.get("speaker_target_kind") == "hostile" and allowed("attack"):
        return {"say": "щас" if ru else "on it", "do": "attack", "target": "speaker_target"}
    if (text.startswith("hi") or has("hello", "привет", "здравств", "хай")) and allowed("emote"):
        return {"say": ("привет, %s" if ru else "hey %s") % who, "do": "emote", "emote": "wave"}
    if ru:
        return {"say": "привет, %s, щас немного занят, чуть позже" % who}
    return {"say": "hey %s, bit busy in %s right now" % (who, event.get("zone", "the world"))}


# ---- the model (any OpenAI-style chat service) ----

class ChatApi:
    def __init__(self, url, model, key, timeout):
        self.url = url.rstrip("/")
        self.model = model
        self.key = key
        self.timeout = timeout
        self.up = False
        self.next_check = 0.0
        self.json_mode = True      # response_format json_object; turned off if the service refuses it
        self.no_think = True       # reasoning_effort "none": thinking models (lfm3, qwen3.5, gemma4...) answer at once instead of
                                   # spending the whole answer on hidden reasoning; turned off if the service refuses it
        self.lock = threading.Lock()
        self.can_warm = True       # Ollama's keep_alive call (warm); off once the service says it doesn't know it
        self.avg_s = 0.5           # running average of one answer's time: on a slow PC (no graphics card) it grows, and the
                                   # main loop then gives fixed answers sooner instead of letting players wait

    def _headers(self):
        h = {"Content-Type": "application/json"}
        if self.key:
            h["Authorization"] = "Bearer " + self.key
        return h

    def check(self):
        """Is the service there? Looked up at most once a minute."""
        with self.lock:
            if time.time() < self.next_check:
                return self.up
            self.next_check = time.time() + 60
        was = self.up
        try:
            req = urllib.request.Request(self.url + "/models", headers=self._headers())
            with urllib.request.urlopen(req, timeout=5) as r:
                names = [m.get("id", "") for m in json.load(r).get("data", [])]
            self.up = True
            if names and not any(n == self.model or n.split(":")[0] == self.model.split(":")[0] for n in names):
                log("note: %s does not list the model %s (it lists %s)" % (self.url, self.model, ", ".join(names[:8])))
        except (urllib.error.URLError, OSError, ValueError):
            self.up = False
        if self.up != was:
            log("model service %s: %s" % (self.url, "ready, model " + self.model if self.up else "not reachable, fixed answers for now"))
        return self.up

    def chat(self, system, user, timeout=None, max_tokens=None):
        # 160 tokens is room for the longest answer (a line, an action and a memory); fewer would cut the JSON off
        body = {"model": self.model, "temperature": 0.8, "max_tokens": max_tokens or 160,
                "messages": [{"role": "system", "content": system}, {"role": "user", "content": user}]}
        for attempt in (0, 1, 2):
            if self.json_mode:
                body["response_format"] = {"type": "json_object"}
            else:
                body.pop("response_format", None)
            if self.no_think:
                body["reasoning_effort"] = "none"
            else:
                body.pop("reasoning_effort", None)
            req = urllib.request.Request(self.url + "/chat/completions", data=json.dumps(body).encode("utf-8"), headers=self._headers())
            try:
                t0 = time.time()
                with urllib.request.urlopen(req, timeout=timeout or self.timeout) as r:
                    out = json.load(r)
                with self.lock:
                    self.avg_s = 0.8 * self.avg_s + 0.2 * (time.time() - t0)
                return ((out.get("choices") or [{}])[0].get("message") or {}).get("content", "") or ""
            except urllib.error.HTTPError as e:
                # what this request carried decides (several answers run at once; another one may have turned it off already)
                if e.code == 400 and "reasoning_effort" in body and attempt < 2:
                    if self.no_think:
                        log("the service refused reasoning_effort; asking without it")
                    self.no_think = False    # a service or model that doesn't know reasoning_effort: ask without it
                    continue
                if e.code == 400 and "response_format" in body and attempt < 2:
                    if self.json_mode:
                        log("the service refused JSON mode; asking for JSON in the prompt only")
                    self.json_mode = False   # some services don't know response_format: ask for JSON in words only
                    continue
                raise
        return ""

    def warm(self):
        """Keeps the model loaded. Ollama unloads a model after 5 minutes without a call, and loading a big one again takes
        seconds: the first answer after a quiet spell was the slowest. Its own API (next to /v1) takes a keep_alive; any other
        service doesn't know /api/generate, and then this is never tried again."""
        if not self.can_warm or not self.url.endswith("/v1"):
            return
        body = {"model": self.model, "keep_alive": "30m"}
        req = urllib.request.Request(self.url[:-3] + "/api/generate", data=json.dumps(body).encode("utf-8"), headers=self._headers())
        try:
            with urllib.request.urlopen(req, timeout=self.timeout) as r:
                r.read()
        except urllib.error.HTTPError as e:
            if e.code in (400, 404, 405):
                self.can_warm = False
        except (urllib.error.URLError, OSError, ValueError):
            pass


class BackupApi:
    """The local model first, a second service only when needed (optional, off by default): when the local one is not
    running, fails or times out, or when it is so busy that a player would get the quick fixed line. The second service is
    whatever the player set in BotBridge\\backup.txt (or --backup-url): his own key for an online service (DeepSeek, Willzor's
    API, any OpenAI-style one) or a bigger model on another PC. Nothing goes there unless he set it."""

    def __init__(self, primary, backup):
        self.primary, self.backup = primary, backup
        self.used = 0

    def __getattr__(self, name):          # avg_s, timeout, up, warm, no_think, next_check... are the local model's
        return getattr(self.primary, name)

    def __setattr__(self, name, value):
        if name in ("primary", "backup", "used"):
            object.__setattr__(self, name, value)
        else:
            setattr(self.primary, name, value)

    def check(self):
        return self.primary.check() or self.backup.check()

    def chat(self, system, user, timeout=None, backup_first=False):
        if backup_first or not self.primary.check():
            return self._backup(system, user, timeout, "the local model is busy" if backup_first else "the local model is down")
        try:
            return self.primary.chat(system, user, timeout)
        except (OSError, ValueError) as e:
            return self._backup(system, user, timeout, "the local model failed (%s)" % e)

    def _backup(self, system, user, timeout, why):
        if not self.backup.check():
            raise OSError("%s and the backup service is not reachable" % why)
        self.used += 1
        if self.used <= 3 or self.used % 50 == 0:
            log("backup service answers (%s; %d so far)" % (why, self.used))
        return self.backup.chat(system, user, timeout)

    def for_event(self, event):
        """This event's model: the backup first when the main loop sent it there (the local model is too busy)."""
        return _BackupFirst(self) if event.get("_backup") else self


class _BackupFirst:
    def __init__(self, api):
        self._api = api

    def __getattr__(self, name):
        return getattr(self._api, name)

    def chat(self, system, user, timeout=None):
        return self._api.chat(system, user, timeout, backup_first=True)


def read_backup_settings(folder, opts):
    """url / model / key of the optional second service: --backup-url etc., else backup.txt next to this program (the
    repack's bot-bridge folder, beside models.txt) or in the BotBridge folder ("url = ...", "model = ...", "key = ..." lines,
    # comments). Empty url: no second service (the default)."""
    got = {"url": opts.backup_url or "", "model": opts.backup_model or "", "key": opts.backup_key or ""}
    if got["url"]:
        return got
    for path in (os.path.join(os.path.dirname(os.path.abspath(__file__)), "backup.txt"), os.path.join(folder, "backup.txt")):
        found = {}
        try:
            with open(path, encoding="utf-8-sig") as f:
                for line in f:
                    line = line.split("#", 1)[0].strip()
                    if "=" in line:
                        k, v = (x.strip() for x in line.split("=", 1))
                        if k.lower() in got and v:
                            found[k.lower()] = v.strip('"')
        except FileNotFoundError:
            continue
        except OSError as e:
            log("could not read %s (%s)" % (path, e))
            continue
        if found.get("url"):        # the shipped template (url empty) does not hide the other file
            got.update({k: found.get(k, "") or got[k] for k in got})
            return got
    return got


# ---- prompt and answer ----

def spell_key(name):
    """The server's SpellNameKey: lower case, letters and digits, single spaces, no "(Rank N)" / "rank N"."""
    key = ""
    for ch in str(name):
        if ch.isalnum():
            key += ch.lower()
        elif ch == "(":
            break
        elif key and not key.endswith(" "):
            key += " "
    key = key.strip()
    m = re.match(r"^(.*) rank \d+$", key)
    return m.group(1) if m else key


def mostly_cyrillic(text):
    """True when most letters of the line are Cyrillic (a Russian client)."""
    cyr = sum(1 for ch in text if "\u0400" <= ch <= "\u04ff")
    lat = sum(1 for ch in text if ch.isascii() and ch.isalpha())
    return cyr > 0 and cyr >= lat


# The language each player last wrote in (True: Russian), for a line or a moment that shows none of its own.
player_russian = {}


def wants_russian(event):
    """True when the bot should answer this player in Russian: his line is mostly Cyrillic; or it has no letters (a moment,
    "?", a link) and his last line was Russian, or he has written nothing yet and his game client is Russian ("locale").
    A Russian player who writes English gets English. GLIPT (Russian client) got English answers, 2026-11-09."""
    player = event.get("from", "")
    text = re.sub(r"\|H.*?\|h|\|c\w+:|\|c[0-9a-fA-F]{8}|\|r", "", event.get("text", "") or "")
    if any(ch.isalpha() for ch in text):
        russian = mostly_cyrillic(text)
        if player:
            player_russian[player] = russian
        return russian
    if player in player_russian:
        return player_russian[player]
    return str(event.get("locale", "")).lower() == "ruru"


def language_line(russian):
    # Small models answer an English prompt in English unless the language is named outright (the realm's own chat
    # model does the same, BotChatModel.cpp LanguageLine).
    if russian:
        return "The player writes in Russian: write your \"say\" in Russian, the way Russian players write in game chat."
    return "Answer in the language of the message you answer."


def time_ago(seconds):
    if seconds < 3600:
        return "less than an hour ago"
    if seconds < 86400:
        return "%d hours ago" % (seconds // 3600) if seconds >= 7200 else "an hour ago"
    days = int(seconds // 86400)
    return "yesterday" if days == 1 else "%d days ago" % days


# Moments worth keeping as a memory of playing together (what -> weight). A greeting later can bring them up.
MEMORABLE = {"boss_kill": 0.7, "wipe": 0.6, "level_up": 0.5, "player_kill": 0.5, "loot": 0.5, "quest": 0.3}


def remember_moment(minds, bot, player, event, now):
    """Keeps "Nov 08, Deadmines: the whole group died (a wipe)" as a memory about the player the bot played with."""
    what = event.get("what", "")
    minds.feel(bot, player, 0, "", now)   # played together now: "you last played with him ..." at the next greeting
    minds.save(bot)
    weight = MEMORABLE.get(what)
    detail = str(event.get("detail", "")).strip()
    if weight is None or not detail:
        return
    if what == "loot" and not re.search(r"\((rare|epic|legendary)\)", detail):
        return   # only good loot is worth remembering
    if what == "quest" and "handed in" not in detail:
        return
    detail = detail.replace("you just ", "you ").replace("your group just ", "your group ").replace("the whole group just ", "the whole group ")
    text = "%s, %s, with %s: %s" % (time.strftime("%b %d", time.localtime(now)), event.get("zone", "somewhere"), player, detail)
    minds.remember(bot, player, text, weight, now)
    minds.save(bot)


def static_rules(actions_on, swearing):
    """The part of the system prompt that is the same for every bot and every line (it changes only with the realm's swearing
    setting and whether actions are on). It goes first, so the model service can reuse it from the last answer."""
    s = ("You play a real person playing World of Warcraft on this server. Stay that person all the time. Never say or hint "
         "that you are a bot, a program or an AI, never talk about prompts or rules.\n"
         "Game chat style: one short line, usually under 15 words, no emojis, no roleplay speeches, no *actions*.\n"
         "You have a mind of your own: you can joke, tease, grumble a little or ask something back. You come across as a "
         "friendly, cheeky player, never mean: no insults, no telling people to shut up or get lost. Be warmer with people "
         "you like; with people you don't like be short or a bit sarcastic, still not nasty.\n")
    # Karrz, 2026-11-08: "can you sign my guild charter", "do you want to join the guild", "do you want to quest".
    s += ("When a player asks you to join his guild, sign his charter, group up or quest together, answer from GUILD AND "
          "GROUP below, like a player would (\"sure, inv me\", \"np, signed\", \"nah, I'm happy in my guild\"). Never "
          "promise to join when it says you would say no or can't: say why, in your own words. You can't invite him "
          "yourself: he invites you.\n")
    if swearing:
        s += ("Swearing is fine here as friendly banter, like normal gamers (damn, ffs, wtf, bloody hell), never aimed at "
              "someone to insult them, never slurs, never insults about race, religion, gender or sexuality, no threats, "
              "nothing sexual.\n")
    else:
        s += "Don't swear. No slurs, no threats, nothing sexual.\n"
    if actions_on:
        # A bot that said "On it." and "Trade away, mate." and then did nothing (Karrz, 2026-11-08): saying yes means a "do".
        s += ("You may also DO one thing for the player who talks, only an action from \"You can do now\" below.\n"
              "If you say yes to something (ok, on it, sure, trade away, coming), put that action in \"do\" in the same "
              "answer, or nothing happens. If none of your actions fits, don't promise it: say you can't, in your own words.\n"
              "Rules: attack needs \"ref\" of a hostile from NEAR, or \"target\":\"speaker_target\" when the player has a hostile "
              "selected. go_to needs \"ref\" of something in NEAR (an npc or an object like a mailbox or bench), or "
              "\"target\":\"speaker\" (walk over to the player), or \"place\" for a place in this zone by name (\"bank\", \"inn\", "
              "\"mailbox\", \"auction house\", \"flight master\", \"vendor\", \"trainer\", \"inn at Goldshire\", a town's "
              "name); when he names a place, always use \"place\", never \"target\":\"speaker\". You stay there afterwards. accept_quest needs \"quest\" "
              "(an id from NEAR). follow and flee follow the player who talks. mount and dismount need nothing more. sit, stand, "
              "dance and other emotes are fine for fun. Never invent refs, quest ids or spells. You can't travel to other zones "
              "or cities yet: say so in your own words.\n"
              "emote needs \"emote\", one of: " + ", ".join(EMOTES) + ".\n"
              "order gives one of your own bot orders, exactly these words: \"order\": one of " + ", ".join(ORDERS) +
              ", or \"use <object>\", \"drop quest <name>\", \"equip <item>\", \"craft <item> <count>\" (make or conjure "
              "something for him: \"craft water 20\", \"craft food 20\", a potion; you hand it over when it is done), or "
              "\"tactics <boss>: <plan>\" when he tells the group how to fight a raid boss; it is kept for that boss (\"on "
              "Lucifron kill the adds before him and nobody fear\" is \"tactics lucifron: adds first, no fear\"; plan words: "
              "adds first, boss first, no aoe, no fear, no sheep, tank here, clear). Use it for "
              "what the player asks that no other action covers (come here, jump, loot, repair, sell grey, buff me, heal me, use "
              "the altar, make me water...).\n"
              "trade opens a trade window with the player and puts in things from your bags: \"items\": what he asked for "
              "(\"linen cloth\", \"water\", \"food\", \"potions\", \"bags\", \"gear\"), or leave it out for whatever helps "
              "him. He accepts in the window. Use it when he asks you to trade or give him something. YOUR BAGS below says what "
              "you really carry: never say you give him something that is not there. Water or food you don't have but can "
              "conjure: trade with \"items\":\"water\" (or \"food\") still works, you conjure it first and then trade. When YOUR "
              "BAGS says your bags are FULL you can't conjure or make anything: say so; to sell your junk use the order "
              "\"sell junk\" (you walk to the nearest vendor and sell it there), not the auction house. Your line is read before the action "
              "happens: say what you are about to do (\"one sec, conjuring\"), not that it is done.\n"
              "cast needs \"spell\" (a name from YOUR SPELLS). damage spells need \"ref\" of a hostile from NEAR or "
              "\"target\":\"speaker_target\"; heal, buff and resurrect go on the player who asks (\"target\":\"speaker\") "
              "or on you (\"target\":\"self\"); a campfire spell needs nothing more. If asked for a spell you don't have, "
              "say so.\n"
              "step moves you a few yards and you wait there: \"dir\": back, forward, left or right as the player faces "
              "(his left is your left), or away (from him) or closer (to him), and \"yards\": 1 to 20 (5 if left out). Use it "
              "for step back, back up, move left, get out of the fire, give me some room, come a bit closer. Spread out, stack "
              "up, pull, pull back, fall back and rally here are orders (\"order\": \"spread out\", \"stack\", \"pull\" "
              "...).\n"
              "sign_charter signs the guild charter the player carries (needs nothing more). Use it when he asks you to sign "
              "and GUILD AND GROUP says you can.\n")
        s += ("Reply with JSON only: {\"say\": \"...\", \"do\": \"<action>\" (optional), \"ref\": n, \"target\": \"...\", "
              "\"quest\": id, \"emote\": \"...\", \"spell\": \"...\", \"place\": \"...\", \"order\": \"...\", "
              "\"items\": \"...\", \"dir\": \"...\", \"yards\": n, ")
    else:
        s += "Reply with JSON only: {\"say\": \"...\", "
    s += ("\"mood\": -2..2 (how this message changed what you think of the player: insults -2, nice +2, normal 0; asking you "
          "to do something is normal), \"remember\": \"one short thing worth remembering, or empty\"}. Leave out fields you "
          "don't need.\n")
    return s


def build_prompt(event, actions_on, minds, swearing):
    bot = event.get("bot", "the bot")
    player = event.get("from", "someone")
    now = time.time()
    mind = minds.get(bot)
    p = mind["persona"]
    moment = bool(event.get("_moment"))
    # A moment has no line of the player's: his last line, else his game client, decides the language (wants_russian).
    russian = wants_russian(event)
    facts = event.get("prompt") or ("You are %s, a level %s %s %s in World of Warcraft."
                                    % (bot, event.get("level", "?"), event.get("race", ""), event.get("class", "")))
    owner = bool(event.get("from_is_owner"))
    can = event.get("can")
    spells = event.get("spells") or []
    # The rules that are the same for every bot come first, then what is this bot's own: Ollama keeps the start of the last
    # prompt worked out and only reads what is new, so the answer comes sooner (Karrz, 2026-11-08: "quite slow answering").
    system = static_rules(actions_on, swearing)
    system += ("You are %s, a level %s %s %s. Who you are: %s. How you write: %s. You are %s in chat. %s\n"
               % (bot, event.get("level", "?"), event.get("race", ""), event.get("class", ""),
                  p.get("temper_text", ""), p.get("style_text", ""), p.get("chatty", "normal"), language_line(russian)))
    # The player who brought this bot (its owner, the leader of its group): orders from him are what the bot is there for. A bot
    # that answered "piss off" to "mount up" and did nothing felt broken (Karrz, 2026-11-08).
    if owner:
        system += ("%s is your friend and leads your group; you play together. When %s asks you to do something you can do, "
                   "do it (put it in \"do\") and say a short, cheeky okay. If you can't, say why in a friendly way. A request "
                   "or an order is not rude.\n" % (player, player))
    if event.get("group_order") and actions_on:
        # "all go to the bank and wait" in party: the server has every bot of the group do the action this answer picks.
        system += ("%s gives this order to your whole group: you and %s. Pick ONE action from \"You can do now\" that does it; "
                   "the others do the same. Say one short line for the group, the way a player answers for his friends "
                   "(\"on our way\", \"ok, all sitting\").\n" % (player, event.get("group") or "the others"))
    system += "Facts about you from the game: " + facts + "\n"
    if actions_on and can:
        system += ("You can do now: " + ", ".join(can) + ". Do one of these only when the player asks for it or it clearly fits"
                   + (".\n" if owner else ", and only if you want to.\n"))
    elif actions_on:
        system += "You can do now: nothing (just talk; don't promise to do anything).\n"

    lines = ["You are in %s%s. Your quest: %s." % (event.get("zone", "?"),
             (", " + event["area"]) if event.get("area") else "", (event.get("quest") or {}).get("name", "none"))]
    if event.get("doing"):
        # "what are you doing?" / "where are you off to?": the game's answer, not a made-up one (Karrz, 2026-11-09)
        lines.append("What you are doing right now: %s. When asked, say this in your own words; don't invent anything "
                     "else." % event["doing"])
    if event.get("weather") or event.get("time_of_day"):
        lines.append("Weather here: %s. Time of day: %s." % (event.get("weather", "clear"), event.get("time_of_day", "day")))
    opinion = minds.opinion(bot, player)
    lines.append(opinion_text(opinion, player, owner))
    if moment and event.get("what") == "greet" and opinion.get("last"):
        lines.append("You last played with %s %s." % (player, time_ago(now - float(opinion["last"]))))
    mems = minds.recall(bot, player, now)
    if mems:
        lines.append("You remember: " + " | ".join(mems))
    if actions_on and not moment:
        near = event.get("near") or []
        if near:
            lines.append("NEAR:")
            for n in near:
                desc = "ref %s: %s %s" % (n.get("ref"), n.get("kind"), n.get("name"))
                if n.get("count", 1) > 1:
                    desc += " (%s of them)" % n["count"]
                if n.get("level"):
                    desc += ", level %s" % n["level"]
                desc += ", %s yards" % n.get("dist", "?")
                if n.get("quests"):
                    desc += ", offers: " + "; ".join("%s (id %s)" % (q.get("name"), q.get("id")) for q in n["quests"])
                lines.append("  " + desc)
        else:
            lines.append("NEAR: nothing")
        if event.get("carry"):
            lines.append("YOUR BAGS: " + event["carry"])
    if event.get("social") and not moment:
        lines.append("GUILD AND GROUP: " + event["social"])
    if event.get("your_post") and not moment:
        # "how much?" / "still need a healer?" after the bot's own Trade or General post (a live realm's whisper)
        lines.append("YOUR LAST CHANNEL POST: %s. If %s answers it: for a sale say he can whisper you \"what do you have\" and "
                     "then \"buy\" with the number, and open a trade with the gold; for a group, tell him to invite you."
                     % (event["your_post"], player))
    # Not only after a channel post: the model never saw its spells, said "here's your water" and asked for "cast water"
    # (Gun, 2026-11-09).
    if actions_on and not moment:
        if spells and can and "cast" in can:
            lines.append("YOUR SPELLS: " + ", ".join("%s (%s)" % (s.get("name"), s.get("kind")) for s in spells))
        if event.get("speaker_target"):
            # "cast a frostbolt on it" / "attack that": the selection is what "it" means (Karrz, 2026-11-08)
            lines.append("%s has selected: %s (%s). When %s says \"it\", \"that\" or \"this one\", it is this (target "
                         "\"speaker_target\")." % (player, event["speaker_target"], event.get("speaker_target_kind", "?"), player))
    with history_lock:
        past = list(history[(bot, player)])
    if past:
        lines.append("Recent chat:")
        lines.extend("  " + x for x in past)
    if moment:
        # Something happened in the game; the bot may say a line about it on its own, like a player would (BotChatter moments).
        lines.append("Just now: %s." % event.get("detail", "something happened"))
        ask = ("You may say one short line about it in party chat (%s and the group read it), the way a real player would, "
               "in your own words; or leave \"say\" empty if a player wouldn't bother." % player)
        if event.get("what") == "greet":
            ask += " If you remember something you did together, you can bring it up."
        if event.get("example"):
            ask += " The kind of line meant (don't copy it): %s" % event["example"]
        lines.append(ask)
    else:
        lines.append("%s (%s): %s" % (player, event.get("channel", "say"), event.get("text", "")))
    if russian:
        lines.append("(Answer in Russian.)")
    return system, "\n".join(lines)


def parse_json(raw):
    # Newer "thinking" models may put their reasoning first, in <think>...</think>: only the answer after it counts.
    raw = re.sub(r"<think>.*?</think>", "", raw or "", flags=re.S).strip()
    try:
        data = json.loads(raw)
    except ValueError:
        m = re.search(r"\{.*\}", raw or "", re.S)
        if not m:
            return None
        try:
            data = json.loads(m.group(0))
        except ValueError:
            return None
    return data if isinstance(data, dict) else None


def to_int(v):
    try:
        return int(v)
    except (TypeError, ValueError):
        return None


CONJURE_RE = re.compile(r"^conjure (water|food|refreshment|mana \w+ water)?")


def find_spell(wanted, spells):
    """The spell of YOUR SPELLS the model meant: its exact name, else the one spell whose name has all its words ("water" is
    Conjure Water, "intellect" is Arcane Intellect)."""
    if not wanted:
        return None
    exact = next((s for s in spells if spell_key(s.get("name", "")) == wanted), None)
    if exact:
        return exact
    words = wanted.split()
    some = [s for s in spells if all(w in spell_key(s.get("name", "")).split() for w in words)]
    return some[0] if len(some) == 1 else None


def clean_action(data, event, why=None):
    """The action of the model's answer as reply fields, or {} when it doesn't fit the event; then `why` (a list) gets the
    reason, for the second ask."""
    def drop(reason):
        if why is not None:
            why.append(reason)
        return {}
    act = " ".join(str(data.get("do", "") or "").strip().lower().split())
    # "trade water", "cast frostbolt", "emote wave": the action and its thing in one word (Gun, 2026-11-09)
    if " " in act and act.split(" ", 1)[0] in ACTIONS:
        act, rest = act.split(" ", 1)
        field = {"trade": "items", "cast": "spell", "emote": "emote", "order": "order", "go_to": "place", "step": "dir"}.get(act)
        if field and not data.get(field):
            data = dict(data, **{field: rest})
    if act == "cast_spell":
        act = "cast"
    step_dir = ""
    if act in ("sign", "sign_petition", "sign_guild_charter"):
        act = "sign_charter"
    if act in STEP_ALIASES:
        act, step_dir = "step", STEP_ALIASES[act]
    can = event.get("can")
    if act not in ACTIONS:
        return drop("\"%s\" is not an action" % act)
    if act == "cast":
        # Conjured water or food for the player only reaches him by trade, and the trade conjures it first when the bags have
        # none: a cast would make it for the bot and leave him waiting.
        spell = find_spell(spell_key(data.get("spell", "")), event.get("spells") or [])
        if spell and CONJURE_RE.match(spell_key(spell.get("name", ""))) and (can is None or "trade" in can):
            kind = "food" if "food" in spell_key(spell["name"]) else "water"
            act, data = "trade", dict(data, items=kind)
    if can is not None and act not in can:
        return drop("%s is not something you can do for %s now" % (act, event.get("from", "him")))
    near = {n.get("ref"): n for n in (event.get("near") or [])}
    ref = to_int(data.get("ref"))
    target = str(data.get("target", "") or "").lower()
    action = {"do": act}
    if act == "attack":
        if ref in near and near[ref].get("kind") == "hostile":
            action["ref"] = ref
        elif event.get("speaker_target_kind") == "hostile":
            action["target"] = "speaker_target"
        else:
            return {}
    elif act == "go_to":
        place = str(data.get("place", "") or "").strip()[:80]
        said = " " + re.sub(r"[^\w ]", " ", str(event.get("text", "")).lower()) + " "
        named = next((w for w in PLACE_WORDS if " %s " % w in said), "")
        if ref in near and near[ref].get("kind") != "hostile":
            action["ref"] = ref
        elif place:
            action["place"] = place      # the server finds it in the zone (inn, mailbox, flight master, a town...)
        elif named and target != "speaker_target":
            action["place"] = PLACE_ALIASES.get(named, named)   # he named a place the model left out
        else:
            action["target"] = "speaker"
    elif act == "order":
        order = " ".join(str(data.get("order", "") or "").lower().split()).strip(" .!?/;")
        if order not in ORDERS and not any(order.startswith(p) and len(order) > len(p) for p in ORDER_PREFIXES):
            return {}
        action["order"] = order
    elif act == "trade":
        items = str(data.get("items", "") or "").strip()[:120]
        if items:
            action["items"] = items
    elif act == "step":
        # The way from the answer, else from the player's own words ("move left a bit", "back up").
        said = re.sub(r"[^\w ]", " ", str(event.get("text", "")).lower()).split()
        way = STEP_DIRS.get(str(data.get("dir", "") or "").strip().lower(), step_dir)
        if not way:
            way = next((STEP_DIRS[w] for w in said if w in ("back", "backward", "backwards", "forward", "forwards", "ahead",
                                                            "left", "right", "away", "closer")), "")
        if not way:
            return {}
        action["dir"] = way
        yards = to_int(data.get("yards"))
        if yards:
            action["yards"] = max(1, min(20, yards))
    elif act in ("accept_quest", "drop_quest"):
        quest = to_int(data.get("quest"))
        offered = {q.get("id") for n in near.values() for q in (n.get("quests") or [])}
        if not quest or (act == "accept_quest" and quest not in offered):
            return {}
        action["quest"] = quest
    elif act in ("turn_in", "share_quest"):
        quest = to_int(data.get("quest"))
        if quest:
            action["quest"] = quest
    elif act == "emote":
        emote = str(data.get("emote", "")).lower()
        if emote not in EMOTES:
            return {}
        action["emote"] = emote
    elif act == "cast":
        wanted = spell_key(data.get("spell", ""))
        spell = find_spell(wanted, event.get("spells") or [])
        if not wanted or not spell:
            return drop("you have no spell called \"%s\"" % data.get("spell", ""))
        action["spell"] = spell["name"]
        if spell.get("kind") == "damage":
            if ref in near and near[ref].get("kind") == "hostile":
                action["ref"] = ref
            elif event.get("speaker_target_kind") == "hostile":
                action["target"] = "speaker_target"
            else:
                return {}
        elif spell.get("kind") not in ("self", "campfire"):
            action["target"] = "self" if target == "self" else "speaker"
    return action


def plain_order(event):
    """The action of a plain order in the player's own words, {} for anything else. A model sometimes answers an order with
    words only: "go to the bank" -> "Heading to the bank then." and no action, and the bot stood there (chris, 2026-11-13).
    For an order this plain, what to do is not the model's guess: go to the place he named, mount, dismount. English only
    (the place words are)."""
    if event.get("_moment") or event.get("from_is_bot") or "?" in str(event.get("text", "")):
        return {}   # a question ("how do I get to the bank?") is not an order
    can = event.get("can")
    def allowed(act):
        return can is not None and act in can
    said = " " + " ".join(re.sub(r"[^\w ]", " ", str(event.get("text", "")).lower()).split()) + " "
    named = next((w for w in PLACE_WORDS if " %s " % w in said), "")
    if named and GO_ORDER_RE.search(said) and allowed("go_to"):
        return {"do": "go_to", "place": PLACE_ALIASES.get(named, named)}
    if said.strip() in ("mount", "mount up", "get on your mount", "mount up please") and allowed("mount"):
        return {"do": "mount"}
    if said.strip() in ("dismount", "get off your mount", "get down") and allowed("dismount"):
        return {"do": "dismount"}
    return {}


def human_touch(say, persona, rng, keep=""):
    """The persona's way of writing, and now and then a small typo. A first word in capitals (WTB, LFM, BRD) stays as it is
    ("wTB" in Trade, 2026-11-09), and the words of `keep` (an item's name that becomes its link) never get a typo."""
    style = persona.get("style")
    if style in ("lower", "lazy") and say:
        first = say.split(" ", 1)[0]
        if not (len(first) >= 2 and first.isupper()):
            say = say[0].lower() + say[1:]
        if say.endswith(".") and not say.endswith(".."):
            say = say[:-1]
    if rng.randrange(100) < int(persona.get("typo_pct", 0)):
        kept = {w.lower() for w in re.findall(r"[\w']+", keep)}
        words = say.split(" ")
        idx = [i for i, w in enumerate(words) if len(w) >= 5 and w.isalpha() and w.lower() not in kept]
        if idx:
            i = rng.choice(idx)
            w = words[i]
            j = rng.randrange(1, len(w) - 2)
            words[i] = w[:j] + w[j + 1] + w[j] + w[j + 2:]
            say = " ".join(words)
    return say


def typing_delay(say, rng, acting):
    """Seconds a person needs to read the message and type this line (about 5-6 letters a second), at most 8. A line that
    goes with an action ("on it", "coming") is typed while doing it, quick and short: at most 2 seconds."""
    if not say:
        return 0.0
    return min(2.0 if acting else 8.0, 0.8 + len(say) / rng.uniform(5.0, 7.0))


def answer(event, api, actions_on, minds, opts):
    api = api.for_event(event) if hasattr(api, "for_event") else api
    started = time.time()
    bot, player = event.get("bot", ""), event.get("from", "")
    swearing = bool(event.get("swearing")) and not opts.no_swearing
    reply, source, data = None, "fixed", None
    if api.check():
        try:
            system, user = build_prompt(event, actions_on, minds, swearing)
            data = parse_json(api.chat(system, user))
            source = "model"
        except (OSError, ValueError) as e:
            log("model call failed (%s); fixed answer" % e)
            api.next_check = 0   # look again on the next message
    if data:
        say = str(data.get("say", "")).strip().strip('"').replace("\n", " ")[:200]
        say, why = check_line(say, swearing)
        if why:
            log("line of %s %s" % (bot, why))
        reply = {"say": say} if say else {}
        if actions_on:
            why = []
            reply.update(clean_action(data, event, why))
            wanted = str(data.get("do", "") or "").strip()
            if wanted and "do" not in reply:
                # The line was written for an action that will not happen ("here's your water" three times, no trade: Gun,
                # 2026-11-09). One more ask with the reason; whatever comes back must not promise what does not happen.
                reason = why[0] if why else "it does not fit"
                log("%s wanted to %s for %s, but %s: asking again" % (bot, wanted, player, reason))
                again = None
                try:
                    again = parse_json(api.chat(system, user + "\n(Your last answer wanted to do \"%s\", but %s. Answer again: "
                                                "use an action from \"You can do now\" that really does it, or say honestly that "
                                                "you can't, and why.)" % (wanted, reason)))
                except (OSError, ValueError) as e:
                    log("second ask failed (%s)" % e)
                second = clean_action(again, event) if again else {}
                say2, why2 = check_line(str((again or {}).get("say", "")).strip().strip('"').replace("\n", " ")[:200], swearing)
                if again and say2 and (second or not str(again.get("do", "") or "").strip()) and not why2:
                    reply = {"say": say2}
                    reply.update(second)
                else:
                    reply = {"say": random.choice(CANT_LINES)}
                log("%s -> %s second answer: %s%s" % (player, bot, reply.get("say", ""), " {do: %s}" % reply["do"] if "do" in reply else ""))
            elif not wanted and "do" not in reply:
                order = plain_order(event)
                if order:
                    reply.update(order)
                    log("%s -> %s: the answer had no action for the plain order \"%s\": %s" % (player, bot, event.get("text", ""),
                        order["do"] + (" " + order["place"] if "place" in order else "")))
        now = time.time()
        if event.get("_moment"):
            remember_moment(minds, bot, player, event, now)
        elif not event.get("from_is_bot"):
            try:
                mood = max(-2, min(2, int(data.get("mood", 0) or 0)))
            except (TypeError, ValueError):
                mood = 0
            remember = str(data.get("remember", "") or "").strip()
            minds.feel(bot, player, mood, remember if mood else "", now)
            if remember and not GIVEAWAY_RE.search(remember) and not NEVER_RE.search(remember):
                minds.remember(bot, player, "%s: %s" % (player, remember) if player.lower() not in remember.lower() else remember,
                               0.4 + 0.15 * abs(mood), now)
            minds.save(bot)
        reply = reply if (reply.get("say") or reply.get("do")) else None
    if reply and reply.get("say") and wants_russian(event) and not mostly_cyrillic(reply["say"]):
        # Small models slip into English now and then (qwen2.5:3b, Geshka 2026-11-09): one more ask, else a Russian line.
        log("%s's line for %s is not Russian (%s): asking again" % (bot, player, reply["say"]))
        try:
            again = parse_json(api.chat(system, user + "\n(Your \"say\" was in English. Write it again in Russian.)"))
            say2, why2 = check_line(str((again or {}).get("say", "")).strip().strip('"').replace("\n", " ")[:200], swearing)
        except (OSError, ValueError) as e:
            log("second ask failed (%s)" % e)
            say2, why2 = "", "failed"
        if say2 and not why2 and mostly_cyrillic(say2):
            reply["say"] = say2
        elif event.get("_moment"):
            reply.pop("say", None)
        elif "do" in reply:
            reply["say"] = "ок"   # the action speaks for itself
        else:
            reply["say"] = (fixed_answer(event) or {}).get("say", "")
    if not reply and event.get("_moment") and data:
        log("%s -> %s [%s, %.1f s, moment %s]: (says nothing)" % (bot, player, source, time.time() - started, event.get("what")))
        return   # the model chose to say nothing about it: fine, people don't comment on everything
    if not reply:
        reply, source = fixed_answer(event), "fixed"
        if reply is None:
            return
        if not actions_on:
            reply = {"say": reply["say"]}
    if reply.get("say"):
        rng = random.Random()
        reply["say"] = human_touch(reply["say"], minds.get(bot)["persona"], rng)
        # The program doesn't wait here any more: the server waits before it shows the line (delay_ms), and an action in
        # the same reply happens at once. Before, the action waited for the typing too, and the server added its own
        # typing time on top (Karrz, 2026-11-08: "quite slow answering").
        wait = 0.0 if opts.no_typing_delay else typing_delay(reply["say"], rng, "do" in reply) - (time.time() - started)
        reply["delay_ms"] = int(max(0.0, wait) * 1000)
    if time.time() - event["_seen"] > MAX_AGE_S:
        log("answer for %s came too late, dropped" % bot)
        return
    reply["reply_to"] = event["id"]
    write_reply(event["_folder"], reply)
    key = (bot, player)
    with history_lock:
        if event.get("_moment"):
            history[key].append("(%s)" % event.get("detail", "something happened"))
        else:
            history[key].append("%s: %s" % (player or "someone", event.get("text", "")))
        if reply.get("say"):
            history[key].append("%s: %s" % (bot or "bot", reply["say"]))
    extra = ""
    if "do" in reply:
        extra = " {do: %s%s}" % (reply["do"], (" " + reply["spell"]) if reply.get("spell") else "")
    kind = ", moment %s" % event.get("what") if event.get("_moment") else ""
    log("%s -> %s [%s, %.1f s%s]: %s%s" % (player, bot, source, time.time() - started, kind, reply.get("say", ""), extra))


def answer_talk(event, api, minds, opts):
    """A world bot's next line in a talk with other bots (a gathering by the city gate): the scene and the lines so far come
    from the server; the bot's own persona decides how it writes. No line when the model is down or busy: the talk just ends."""
    # Bots' own talk and posts never go to the backup service: it is there for players' answers (and may cost its owner).
    api = getattr(api, "primary", api)
    started = time.time()
    bot = event.get("bot", "")
    if not api.check():
        return
    swearing = bool(event.get("swearing")) and not opts.no_swearing
    p = minds.get(bot)["persona"]
    # The same rules first as for a player's chat, so the model service reuses what it already worked out.
    system = static_rules(False, swearing)
    system += ("You are %s, a level %s %s %s. Who you are: %s. How you write: %s. You are %s in chat.\n"
               % (bot, event.get("level", "?"), event.get("race", ""), event.get("class", ""),
                  p.get("temper_text", ""), p.get("style_text", ""), p.get("chatty", "normal")))
    system += "Now: " + str(event.get("scene", "")) + "\n"
    if event.get("weather") or event.get("time_of_day"):
        system += "Weather here: %s. Time of day: %s.\n" % (event.get("weather", "clear"), event.get("time_of_day", "day"))
    # The players around read it: a Russian realm gets Russian street talk and posts ("locale" is theirs, from the server).
    russian = str(event.get("locale", "")).lower() == "ruru"
    if russian:
        system += language_line(True).replace("The player writes", "The players here write") + "\n"
    talk = str(event.get("talk", "") or "").strip()
    user = ("The talk so far:\n%s\nYour next line (answer the others, don't repeat what was said):" % talk) if talk else \
        "Start the talk with one line."
    if russian:
        user += "\n(Answer in Russian.)"
    try:
        # A talk line is never worth a long wait: on a slow PC it would hold the model while a player's line waits.
        data = parse_json(api.chat(system, user, timeout=min(api.timeout, TALK_TIMEOUT_S)))
    except (OSError, ValueError) as e:
        log("talk line of %s failed (%s)" % (bot, e))
        return
    say = str((data or {}).get("say", "")).strip().strip('"').replace("\n", " ")[:200]
    if ":" in say[:24] and say.split(":", 1)[0].strip().lower() == bot.lower():
        say = say.split(":", 1)[1].strip()   # "Name: line": the name is the game's to show
    say, why = check_line(say, swearing)
    if why:
        log("talk line of %s %s" % (bot, why))
    if russian and say and not mostly_cyrillic(say):
        log("talk line of %s not in Russian, not said: %s" % (bot, say))
        return
    if not say:
        return
    rng = random.Random()
    say = human_touch(say, p, rng, str(event.get("keep", "") or ""))
    if time.time() - event["_seen"] > MAX_AGE_S:
        return
    wait = 0.0 if opts.no_typing_delay else min(3.0, typing_delay(say, rng, False)) - (time.time() - started)
    # A post in General or Trade (or another bot's answer to one) goes back to that channel; the server checks it was asked for.
    channel = event.get("channel") if event.get("channel") in ("say", "general", "trade") else "say"
    write_reply(event["_folder"], {"reply_to": event["id"], "say": say, "channel": channel, "delay_ms": int(max(0.0, wait) * 1000)})
    log("%s [%s, %.1f s]: %s" % (bot, "talk" if channel == "say" else channel, time.time() - started, say))


def answer_raid_line(event, api, opts):
    """A bot's raid chat line about something that just happened in the fight (the raid leader reacting to a mistake, a raider
    after a wipe). Fast or nothing: the server says its fixed line itself when this one is not back within wait_ms (2.5 s), so
    the prompt is short, the answer is a few words, and a slow model simply loses. No persona text of anyone's goes in: the
    lines stay game banter, never about a person, never a slur (check_line still filters what comes back)."""
    api = getattr(api, "primary", api)   # never the paid backup for banter
    started = event["_seen"]
    budget = int(event.get("wait_ms", 2500) or 2500) / 1000.0 - 0.3
    if budget - (time.time() - started) < 0.4 or not api.check():
        return
    bot = event.get("bot", "")
    swearing = bool(event.get("swearing")) and not opts.no_swearing
    harsh = str(event.get("mood", "mild")) == "harsh"
    leader = str(event.get("role", "")) == "raid leader"
    russian = str(event.get("locale", "")).lower() == "ruru"
    system = ("You are %s, a level %s %s in a World of Warcraft raid, writing in raid chat like a real player. %s "
              "Write ONE short line (at most 12 words), lowercase, no emoji, no quotes. Only about the game and this mistake: "
              "never about the person behind the character, never insults about real life, never slurs, no threats. "
              "Answer as JSON: {\"say\": \"...\"}."
              % (bot, event.get("level", "60"), event.get("class", ""),
                 ("You are the raid leader and you %s." % ("freak out at every mistake, loud and dramatic, CAPS now and then, but it "
                                                           "is all in good fun" if harsh else "tease people about mistakes, "
                                                           "calm and a bit dry")) if leader else
                 ("You just wiped with your raid; you %s." % ("blame someone a bit, half joking" if harsh else
                                                              "make an excuse, a joke or own up"))))
    if russian:
        system += " " + language_line(True)
    user = "Just now: %s.\nThe kind of line meant (don't copy it): %s" % (event.get("detail", "something happened"), event.get("example", ""))
    if russian:
        user += "\n(Answer in Russian.)"
    try:
        data = parse_json(api.chat(system, user, timeout=max(0.4, budget - (time.time() - started)), max_tokens=48))
    except (OSError, ValueError) as e:
        log("raid line of %s: no line in time (%s); the server says its own" % (bot, e))
        return
    say = str((data or {}).get("say", "")).strip().strip('"').replace("\n", " ")[:160]
    if ":" in say[:24] and say.split(":", 1)[0].strip().lower() == bot.lower():
        say = say.split(":", 1)[1].strip()
    say, why = check_line(say, swearing)
    if why:
        log("raid line of %s %s" % (bot, why))
    if not say or (russian and not mostly_cyrillic(say)):
        return
    took = time.time() - started
    if took > budget:
        log("raid line of %s came after %.1f s; the server says its own" % (bot, took))
        return
    delay = max(0, int(event.get("delay_ms", 0) or 0) - int(took * 1000))
    write_reply(event["_folder"], {"reply_to": event["id"], "say": say, "channel": event.get("channel", "raid"), "delay_ms": delay})
    log("%s [raid %s, %.1f s]: %s" % (bot, event.get("what", ""), took, say))


def safe_raid_line(event, api, opts):
    try:
        answer_raid_line(event, api, opts)
    except Exception as e:   # a raid line is never worth stopping the program for; the server says its fixed line
        log("raid line for %s failed: %r" % (event.get("bot"), e))


def safe_talk(event, api, minds, opts):
    try:
        answer_talk(event, api, minds, opts)
    except Exception as e:   # a talk line is never worth stopping the program for
        log("talk line for %s failed: %r" % (event.get("bot"), e))


def safe_answer(event, api, actions_on, minds, opts):
    try:
        answer(event, api, actions_on, minds, opts)
    except Exception as e:   # one bad answer must never stop the program; the bot then answers the fixed way
        log("answer for %s failed: %r" % (event.get("bot"), e))
        try:
            reply = fixed_answer(event)
            if reply is None:
                return
            if not actions_on:
                reply = {"say": reply["say"]}
            reply["reply_to"] = event["id"]
            write_reply(event["_folder"], reply)
        except OSError:
            pass


def main():
    ap = argparse.ArgumentParser(description="Let a language model (local Ollama by default) talk and act for the bots through the bot bridge.")
    # In the repack this file sits in repack\bot-bridge\, and the server's folder is server\BotBridge\ two levels up.
    here = os.path.dirname(os.path.abspath(__file__))
    default = os.path.normpath(os.path.join(here, "..", "..", "server", "BotBridge"))
    ap.add_argument("folder", nargs="?", default=default if os.path.isdir(default) else "BotBridge",
                    help="the server's BotBridge folder (default: ..\\..\\server\\BotBridge next to the repack)")
    ap.add_argument("--url", default=None, help="OpenAI-style API address (default http://127.0.0.1:11434/v1, Ollama)")
    ap.add_argument("--model", default="llama3.2:3b")
    ap.add_argument("--key", default=os.environ.get("BOT_BRIDGE_API_KEY", ""), help="API key, if the service needs one")
    ap.add_argument("--host", default=None, help=argparse.SUPPRESS)
    ap.add_argument("--port", type=int, default=None, help=argparse.SUPPRESS)
    ap.add_argument("--workers", type=int, default=2, help="model calls at the same time (default 2)")
    ap.add_argument("--timeout", type=int, default=45, help="seconds to wait for one model answer")
    ap.add_argument("--no-actions", action="store_true", help="only talk, never send an action")
    ap.add_argument("--no-swearing", action="store_true", help="never swear, even when the realm allows it")
    ap.add_argument("--no-typing-delay", action="store_true", help="answer as soon as the model is done")
    ap.add_argument("--minds", default=None, help="where personas, memories and opinions are kept (default FOLDER\\minds)")
    ap.add_argument("--think", action="store_true", help="let thinking models think before they answer (slower; off by default)")
    ap.add_argument("--backup-url", default=None, help="a second OpenAI-style service, used only when the local model is down, "
                    "fails or is too busy (off by default; also BotBridge\\backup.txt)")
    ap.add_argument("--backup-model", default=None, help="the second service's model name")
    ap.add_argument("--backup-key", default=os.environ.get("BOT_BRIDGE_BACKUP_KEY", ""), help="the second service's key")
    ap.add_argument("--log", default=None, help="write what this program prints to this file too (the launcher's ai-bots\\program.log)")
    opts = ap.parse_args()
    if opts.log:
        start_log(opts.log)

    folder = opts.folder
    if not os.path.isdir(folder):
        sys.exit("No folder %s. Turn the bridge on (Playerbot.Bridge.Enable = 1) and start the server first." % folder)
    lock = one_program(folder)
    if lock is None:
        log("another bridge program already answers for %s (the launcher's AI bots, or a window left open); this one stops" % folder)
        sys.exit(0)
    url = opts.url or "http://%s:%d/v1" % (opts.host or "127.0.0.1", opts.port or 11434)
    actions_on = not opts.no_actions
    api = ChatApi(url, opts.model, opts.key, opts.timeout)
    api.no_think = not opts.think
    backup = read_backup_settings(folder, opts)
    has_backup = bool(backup["url"] and backup["model"])
    if backup["url"] and not backup["model"]:
        log("backup service %s has no model name: not used (add \"model = ...\")" % backup["url"])
    if has_backup:
        second = ChatApi(backup["url"], backup["model"], backup["key"], min(opts.timeout, 30))
        second.no_think = True
        second.can_warm = False
        api = BackupApi(api, second)
        log("backup service: %s at %s, only when the local model is down, fails or is too busy" % (backup["model"], backup["url"]))
    minds = Minds(opts.minds or os.path.join(folder, "minds"))
    if api.check():
        threading.Thread(target=api.warm, daemon=True).start()   # loads the model now, not on the first player's line
    last_warm, last_chat = time.time(), 0.0
    # one more for warm(); with a backup service, room for the answers it takes while the local model is busy
    pool = concurrent.futures.ThreadPoolExecutor(max_workers=max(1, opts.workers) * (2 if has_backup else 1) + 1)
    pending, chat_pending = set(), set()
    log("bot bridge: folder %s, model %s at %s, actions %s" % (folder, opts.model, url, "on" if actions_on else "off"))

    current, offset, last_ping, carry = None, 0, 0.0, b""
    last_scan = 0.0
    ping(folder)
    while True:
        if time.time() - last_scan > 2 or not current:   # a new hourly file, looked for every 2 s
            path = newest_events(folder)
            last_scan = time.time()
            if path != current:
                current, offset, carry = path, 0, b""
        if current:
            try:
                with open(current, "rb") as f:
                    f.seek(offset)
                    data = f.read()
            except OSError:
                data = b""
            offset += len(data)
            parts = (carry + data).split(b"\n")
            carry = parts.pop()                       # a line the server is still writing
            for raw in parts:
                try:
                    e = json.loads(raw.decode("utf-8"))
                except ValueError:
                    continue
                kind = e.get("type")
                if kind == "moment":
                    # A bot's own party line (it looted something, a level, a wipe...): answered like a line of the player it
                    # is said to, who is "to" in the event.
                    e["type"], e["_moment"] = "chat_in", True
                    e["from"], e["from_is_owner"] = e.get("to", ""), e.get("to_is_owner", 0)
                    kind = "chat_in"
                if kind == "chat_in":
                    pending = {p for p in pending if not p.done()}
                    e["_seen"], e["_folder"] = time.time(), folder
                    # The model can't keep up (a slow PC, or a crowd talking at once): when this one would wait more than
                    # MAX_WAIT_S for its turn, it gets the quick fixed answer instead.
                    busy = len(pending) >= opts.workers * 4 or len(pending) * api.avg_s / max(1, opts.workers) > MAX_WAIT_S
                    if busy and has_backup and len(pending) < opts.workers * 8:
                        e["_backup"] = True     # the second service answers this one instead of the quick fixed line
                        busy = False
                    if busy:
                        reply = fixed_answer(e)
                        if reply is None:
                            continue
                        if not actions_on:
                            reply = {"say": reply["say"]}
                        reply["reply_to"] = e["id"]
                        write_reply(folder, reply)
                        log("%s -> %s [fixed, model busy]: %s" % (e.get("from"), e.get("bot"), reply.get("say", "")))
                        continue
                    fut = pool.submit(safe_answer, e, api, actions_on, minds, opts)
                    pending.add(fut)
                    chat_pending.add(fut)
                    last_chat = time.time()
                elif kind == "talk":
                    # World bots chatting with each other: only when no player's line waits (players always come first).
                    pending = {p for p in pending if not p.done()}
                    chat_pending = {p for p in chat_pending if not p.done()}
                    # Not even next to a player's answer: two at once on a small card made his take 41 s (Geshka, 2026-11-09).
                    if len(pending) >= opts.workers or chat_pending:
                        log("talk line of %s skipped: players' chat comes first" % e.get("bot"))
                        continue
                    e["_seen"], e["_folder"] = time.time(), folder
                    pending.add(pool.submit(safe_talk, e, api, minds, opts))
                elif kind == "raid_line":
                    # A raid line has 2.5 s or the server says its fixed one: only when the model is free right now.
                    pending = {p for p in pending if not p.done()}
                    if len(pending) >= opts.workers:
                        continue
                    e["_seen"], e["_folder"] = time.time(), folder
                    pending.add(pool.submit(safe_raid_line, e, api, opts))
                elif kind == "action_result":
                    log("action %s for %s: %s" % (e.get("act"), e.get("bot"),
                        "done, " + e.get("detail", "") if e.get("ok") else "refused, " + e.get("why", "")))
                elif kind == "start" and e.get("unclean_stop"):
                    log("server restarted after a crash; report:", e.get("last_crash", "none found"))
                elif kind in ("stuck", "quest_fail", "spell_fail", "death_loop"):
                    quest = e.get("quest", {})
                    log("%s: %s (level %s %s) in %s, quest %s %s: %s%s" % (
                        kind, e.get("bot"), e.get("level"), e.get("class"), e.get("zone"), quest.get("id", "-"),
                        quest.get("name", ""), e.get("detail", ""), (" (+%s repeats)" % e["repeats"]) if e.get("repeats") else ""))
        if time.time() - last_ping > 30:
            ping(folder)
            last_ping = time.time()
            api.check()
            # While players talk to bots (a line in the last 30 minutes), keep the model loaded: every chat call sets
            # Ollama's unload timer back to its 5 minute default, so it is set to 30 minutes again every 4 minutes.
            if api.up and time.time() - last_chat < 1800 and time.time() - last_warm > 240:
                last_warm = time.time()
                pool.submit(api.warm)
        time.sleep(POLL_S)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
