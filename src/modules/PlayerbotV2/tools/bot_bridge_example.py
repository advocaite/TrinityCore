"""Example program for the bot bridge (Playerbot.Bridge.*, see BOT_BRIDGE.md).

Reads the server's newest events file, prints bot problems, answers chat_in lines and keeps the bridge marked as running.
Python 3, standard library only. Run it next to the server:

    python bot_bridge_example.py "C:\\WoW Forever\\server\\BotBridge"

Replace answer() with a call to your own chat model; everything else can stay.

With Playerbot.Bridge.Actions.Enable = 1 a reply may also carry one action ("do"). answer() shows the idea with a few words:
"follow me", "wait here", "attack" (what the player has selected), "hi" (a wave). A model would pick the action itself, using
the chat_in's "near" list (things around the bot, each with a "ref") and "speaker_target". The server checks every action and
answers with an action_result event, printed here.
"""

import glob
import json
import os
import sys
import time


def answer(event):
    """The reply to a chat_in event: what the bot says, plus (optional) one action. Here: fixed lines and a few key words."""
    text = event.get("text", "").lower()
    who = event.get("from", "there")
    if "follow" in text:
        return {"say": "Sure, right behind you.", "do": "follow"}
    if "wait" in text or "stay" in text:
        return {"say": "Okay, I'll wait here.", "do": "stay"}
    if "attack" in text and event.get("speaker_target_kind") == "hostile":
        return {"say": "On it!", "do": "attack", "target": "speaker_target"}
    if text.startswith("hi") or "hello" in text:
        return {"say": "Hi %s!" % who, "do": "emote", "emote": "wave"}
    return {"say": "Hi %s, I'm in %s right now." % (who, event.get("zone", "the world"))}


def write_reply(folder, reply):
    with open(os.path.join(folder, "replies.jsonl"), "a", encoding="utf-8") as f:
        f.write(json.dumps(reply, ensure_ascii=False) + "\n")


def ping(folder):
    """Marks the program as running: touches replies.jsonl (makes it when it is missing), so pings don't make the file grow."""
    path = os.path.join(folder, "replies.jsonl")
    if os.path.exists(path):
        os.utime(path, None)
    else:
        open(path, "a", encoding="utf-8").close()


def newest_events(folder):
    files = glob.glob(os.path.join(folder, "events-*.jsonl"))
    return max(files, key=os.path.getmtime) if files else None


def main(folder):
    current, offset, last_ping = None, 0, 0.0
    while True:
        path = newest_events(folder)
        if path != current:                       # a new hour (or the first run): read the new file from its start
            current, offset = path, 0
        if current:
            with open(current, "r", encoding="utf-8") as f:
                f.seek(offset)
                while True:
                    line = f.readline()
                    if not line.endswith("\n"):   # nothing more, or a line the server is still writing
                        break
                    offset = f.tell()
                    try:
                        e = json.loads(line)
                    except ValueError:
                        continue
                    kind = e.get("type")
                    if kind == "chat_in":
                        reply = answer(e)
                        reply["reply_to"] = e["id"]
                        write_reply(folder, reply)
                    elif kind == "action_result":
                        print("action %s for %s: %s" % (e.get("act"), e.get("bot"),
                              "done, " + e.get("detail", "") if e.get("ok") else "refused, " + e.get("why", "")))
                    elif kind == "start" and e.get("unclean_stop"):
                        print("server restarted after a crash; report:", e.get("last_crash", "none found"))
                    elif kind in ("stuck", "quest_fail", "spell_fail", "death_loop"):
                        quest = e.get("quest", {})
                        print("%s: %s (level %s %s) in %s, quest %s %s: %s" % (
                            kind, e.get("bot"), e.get("level"), e.get("class"), e.get("zone"),
                            quest.get("id", "-"), quest.get("name", ""), e.get("detail", "")))
        if time.time() - last_ping > 30:          # the server sends chat here only while this file keeps changing
            ping(folder)
            last_ping = time.time()
        time.sleep(1)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "BotBridge")
