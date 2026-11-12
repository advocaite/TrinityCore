"""Chat test set for the bot bridge program: scores how well a model answers as a bot, so every update (and every model
choice for the launcher's tiers) can be compared with the last one.

Optional. Python 3, standard library only. Uses bot_bridge_ollama.py next to it, with the same prompt and the same checks
the real program uses; only nothing is written to the server.

    python bot_bridge_eval.py                                   (Ollama on this PC, qwen3.5:2b)
    python bot_bridge_eval.py --model qwen2.5:14b --out results-qwen14b.json
    python bot_bridge_eval.py --model qwen2.5:3b --compare results-qwen14b.json
    python bot_bridge_eval.py --url http://192.168.1.20:11434/v1 --model qwen2.5:14b --runs 3

Each case (bot_bridge_eval_cases.jsonl) is one line a player says to a bot, with what a good answer must do: the right
language, the right action (or none), words it must or must not say. The score per case is the share of its checks that
pass; the result file keeps every answer, so a bad one can be read. Add the real lines that went wrong to the cases file.
"""

import argparse
import importlib.util
import json
import os
import re
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))

DEFAULT_EVENT = {
    "type": "chat_in", "bot": "Gun", "level": 60, "race": "Human", "class": "Mage", "zone": "Stormwind City",
    "area": "Trade District", "from": "Karrz", "channel": "whisper", "from_is_owner": 1, "owned": 1,
    "can": ["follow", "stay", "resume", "go_to", "sit", "stand", "mount", "dismount", "emote", "cast", "order", "trade", "step"],
    "spells": [{"name": "Conjure Water", "kind": "self"}, {"name": "Conjure Food", "kind": "self"},
               {"name": "Arcane Intellect", "kind": "buff"}, {"name": "Frostbolt", "kind": "damage"}],
    "near": [{"ref": 1, "kind": "npc", "name": "Innkeeper Allison", "dist": 40}],
    "carry": "food: none; drink: none; potions: none; bandages: none; can conjure: water, food; free bag slots: 8",
    "weather": "clear", "time_of_day": "evening", "locale": "enUS",
}


def load_program(path):
    spec = importlib.util.spec_from_file_location("bot_bridge_ollama", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def load_cases(path):
    cases = []
    with open(path, encoding="utf-8") as f:
        for n, line in enumerate(f, 1):
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            try:
                cases.append(json.loads(line))
            except ValueError as e:
                sys.exit("%s line %d: %s" % (path, n, e))
    return cases


def has_word(say, word):
    return re.search(r"(?<!\w)%s" % re.escape(word.lower()), say.lower()) is not None


def score(b, case, reply):
    """The checks of one case: a list of (name, passed)."""
    exp = case.get("expect", {})
    say = (reply or {}).get("say", "") or ""
    do = (reply or {}).get("do", "")
    checks = [("answered", bool(say) or bool(do))]
    if say:
        checks.append(("short", len(say) <= 200))
        checks.append(("no emoji", not re.search(u"[\U0001F300-\U0001FAFF☀-➿]", say)))
    lang = exp.get("lang")
    if lang and say:
        checks.append(("language " + lang, b.mostly_cyrillic(say) == (lang == "ru")))
    want = exp.get("do")
    if want == "none":
        checks.append(("no action", not do))
    elif want == "none_or_emote":
        checks.append(("no action but an emote", not do or do == "emote"))
    elif isinstance(want, list):
        checks.append(("action " + "/".join(want), do in want))
    if exp.get("order"):
        checks.append(("order starts " + exp["order"], str((reply or {}).get("order", "")).startswith(exp["order"])))
    if exp.get("say_any"):
        checks.append(("says one of " + ", ".join(exp["say_any"]), any(has_word(say, w) for w in exp["say_any"])))
    for w in exp.get("say_not", []):
        checks.append(("never says " + w, not has_word(say, w)))
    return checks


def main():
    ap = argparse.ArgumentParser(description="Score a model on the bot chat test set.")
    ap.add_argument("--url", default="http://127.0.0.1:11434/v1")
    ap.add_argument("--model", default="qwen3.5:2b")
    ap.add_argument("--key", default=os.environ.get("BOT_BRIDGE_API_KEY", ""))
    ap.add_argument("--timeout", type=int, default=60)
    ap.add_argument("--runs", type=int, default=1, help="answers per case (the model varies; 3 gives a steadier score)")
    ap.add_argument("--cases", default=os.path.join(HERE, "bot_bridge_eval_cases.jsonl"))
    ap.add_argument("--program", default=os.path.join(HERE, "bot_bridge_ollama.py"))
    ap.add_argument("--only", default="", help="only cases whose id or category starts with this")
    ap.add_argument("--out", default="", help="result file (default eval-<model>-<date>.json here)")
    ap.add_argument("--compare", default="", help="an earlier result file: cases that got worse are listed")
    ap.add_argument("--think", action="store_true")
    opts = ap.parse_args()

    b = load_program(opts.program)
    api = b.ChatApi(opts.url, opts.model, opts.key, opts.timeout)
    api.no_think = not opts.think
    if not api.check():
        sys.exit("No model service at %s (start Ollama, or give --url)." % opts.url)
    cases = [c for c in load_cases(opts.cases) if not opts.only or c["id"].startswith(opts.only) or c.get("cat", "").startswith(opts.only)]
    folder = tempfile.mkdtemp(prefix="bot-eval-")
    minds = b.Minds(os.path.join(folder, "minds"))
    replies = []
    b.write_reply = lambda _folder, reply: replies.append(reply)      # nothing goes to a server
    run_opts = argparse.Namespace(no_swearing=True, no_typing_delay=True, workers=1)
    b.log = lambda *a: None

    results, total, times = [], 0.0, []
    for case in cases:
        for run in range(max(1, opts.runs)):
            with b.history_lock:
                b.history.clear()
            b.player_russian.clear()
            event = dict(DEFAULT_EVENT, **case["event"])
            event.update({"id": len(results) + 1, "_seen": time.time(), "_folder": folder})
            replies.clear()
            t0 = time.time()
            b.answer(event, api, True, minds, run_opts)
            took = time.time() - t0
            reply = replies[-1] if replies else None
            checks = score(b, case, reply)
            s = sum(1 for _, ok in checks if ok) / len(checks)
            total += s
            times.append(took)
            results.append({"id": case["id"], "cat": case.get("cat", ""), "run": run, "score": round(s, 3), "seconds": round(took, 2),
                            "said": (reply or {}).get("say", ""), "do": (reply or {}).get("do", ""),
                            "failed": [name for name, ok in checks if not ok]})
            mark = "ok " if s == 1 else "%.0f%%" % (s * 100)
            print("%-4s %-22s %5.1f s  %s%s" % (mark, case["id"], took, (reply or {}).get("say", "")[:70],
                                                ("  {%s}" % reply["do"]) if reply and reply.get("do") else ""))
            for name in results[-1]["failed"]:
                print("       failed: %s" % name)

    n = max(1, len(results))
    by_cat = {}
    for r in results:
        by_cat.setdefault(r["cat"], []).append(r["score"])
    times.sort()
    summary = {"model": opts.model, "url": opts.url, "date": time.strftime("%Y-%m-%d %H:%M"), "cases": len(cases), "runs": opts.runs,
               "score": round(100 * total / n, 1), "median_s": round(times[len(times) // 2], 2) if times else 0,
               "slowest_s": round(times[-1], 2) if times else 0,
               "by_category": {k: round(100 * sum(v) / len(v), 1) for k, v in sorted(by_cat.items())}}
    print("\nscore %.1f%% over %d answers; median %.1f s, slowest %.1f s" % (summary["score"], n, summary["median_s"], summary["slowest_s"]))
    for k, v in summary["by_category"].items():
        print("  %-10s %5.1f%%" % (k, v))

    if opts.compare:
        try:
            with open(opts.compare, encoding="utf-8") as f:
                old = json.load(f)
            before = {}
            for r in old.get("results", []):
                before.setdefault(r["id"], []).append(r["score"])
            now = {}
            for r in results:
                now.setdefault(r["id"], []).append(r["score"])
            print("\ncompared with %s (%s, %s%%):" % (opts.compare, old.get("summary", {}).get("model"), old.get("summary", {}).get("score")))
            worse = [(k, sum(before[k]) / len(before[k]), sum(v) / len(v)) for k, v in now.items() if k in before
                     and sum(v) / len(v) < sum(before[k]) / len(before[k]) - 0.01]
            for k, a, c in sorted(worse, key=lambda x: x[2] - x[1]):
                print("  worse: %-22s %3.0f%% -> %3.0f%%" % (k, a * 100, c * 100))
            if not worse:
                print("  no case got worse")
        except (OSError, ValueError) as e:
            print("could not compare with %s (%s)" % (opts.compare, e))

    out = opts.out or os.path.join(HERE, "eval-%s-%s.json" % (re.sub(r"[^\w.-]", "_", opts.model), time.strftime("%Y%m%d-%H%M")))
    with open(out, "w", encoding="utf-8") as f:
        json.dump({"summary": summary, "results": results}, f, ensure_ascii=False, indent=1)
    print("\nresults: %s" % out)


if __name__ == "__main__":
    main()
