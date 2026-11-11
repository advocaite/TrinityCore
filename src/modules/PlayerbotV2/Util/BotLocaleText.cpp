#include "BotLocaleText.h"

#include <algorithm>
#include <utility>

namespace Playerbot::Locale {

namespace {

constexpr int kMaxDepth = 3;   // a capture translated inside a capture inside a line, no deeper

size_t Letters(std::vector<std::string> const& lits)
{
    size_t n = 0;
    for (std::string const& l : lits) n += l.size();
    return n;
}

// The captures a translation refers to: `{}` counts up from 1, `{N}` is capture N. Returns the highest one, or -1 when malformed.
int HighestSlot(std::string_view tr)
{
    int seq = 0, top = 0;
    for (size_t i = 0; i < tr.size(); ++i)
    {
        if (tr[i] != '{') continue;
        if (i + 1 < tr.size() && tr[i + 1] == '}') { top = std::max(top, ++seq); ++i; continue; }
        if (i + 2 < tr.size() && tr[i + 1] >= '1' && tr[i + 1] <= '9' && tr[i + 2] == '}') { top = std::max(top, tr[i + 1] - '0'); i += 2; continue; }
        return -1;
    }
    return top;
}

bool IsNumberChar(char c) { return (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+'; }

// The template's fixed texts and the kind of each capture between them: `{}` any non-empty text, `{?}` any text or none, `{#}` a
// number (digits . - +), which may sit right before another capture ("level {#}{?}": the number, then an optional tail).
void SplitTemplate(std::string_view en, std::vector<std::string>& lits, std::vector<char>& kinds)
{
    size_t start = 0, i = 0;
    while (i < en.size())
    {
        char kind = 0;
        size_t len = 0;
        if (en.substr(i, 2) == "{}") { kind = 'a'; len = 2; }
        else if (en.substr(i, 3) == "{?}") { kind = '?'; len = 3; }
        else if (en.substr(i, 3) == "{#}") { kind = '#'; len = 3; }
        else if (en.substr(i, 3) == "{$}") { kind = '$'; len = 3; }
        else if (en.substr(i, 3) == "{@}") { kind = '@'; len = 3; }
        else if (en.substr(i, 3) == "{!}") { kind = '!'; len = 3; }
        if (!kind) { ++i; continue; }
        lits.emplace_back(en.substr(start, i - start));
        kinds.push_back(kind);
        i += len;
        start = i;
    }
    lits.emplace_back(en.substr(start));
}

// "1g 20s 5c", any of the parts, as BotTrade::FormatMoney writes it.
bool IsMoney(std::string_view cap)
{
    if (cap.empty()) return false;
    size_t i = 0;
    while (i < cap.size())
    {
        size_t j = i;
        while (j < cap.size() && cap[j] >= '0' && cap[j] <= '9') ++j;
        if (j == i || j >= cap.size() || (cap[j] != 'g' && cap[j] != 's' && cap[j] != 'c')) return false;
        i = j + 1;
        if (i < cap.size())
        {
            if (cap[i] != ' ') return false;
            ++i;
            if (i == cap.size()) return false;
        }
    }
    return true;
}

bool CaptureFits(std::string_view cap, char kind)
{
    if (kind == '?') return true;
    if (cap.empty()) return false;
    if (kind == '#')
        return std::all_of(cap.begin(), cap.end(), IsNumberChar);
    if (kind == '$')
        return IsMoney(cap);
    if (kind == '@')
        return cap.find("|H") != std::string_view::npos;   // a chat link: |Hitem:...|h[Name]|h, |Hquest:...
    if (kind == '!')
        return cap.find(' ') == std::string_view::npos;    // a character's name: one word
    return true;
}

} // namespace

bool Catalog::Add(std::string_view en, std::string_view tr)
{
    auto reject = [&]() { rejected_.emplace_back(en); return false; };
    if (en.empty() || tr.empty()) return reject();
    std::vector<std::string> lits;
    std::vector<char> kinds;
    SplitTemplate(en, lits, kinds);
    int const slots = HighestSlot(tr);
    if (slots < 0 || slots > int(lits.size()) - 1) return reject();
    if (lits.size() == 1)
    {
        if (!exact_.emplace(std::string(en), std::string(tr)).second) return reject();
        return true;
    }
    for (size_t i = 1; i + 1 < lits.size(); ++i)
        if (lits[i].empty() && kinds[i - 1] != '#') return reject();   // two captures in a row can only be told apart after a number
    size_t const fixed = Letters(lits);
    if (fixed == 0) return reject();
    for (Pattern const& p : patterns_)
        if (p.lits == lits && p.kinds == kinds) return reject();
    // Kept most specific first: most fixed letters, then most captures of a narrow kind (so "needs {} for {$}?" is tried before
    // "needs {} for {}?"), then table order. A lookup never sorts, and readers on several threads share it.
    size_t const typed = size_t(std::count_if(kinds.begin(), kinds.end(), [](char k) { return k != 'a' && k != '?'; }));
    auto const at = std::upper_bound(patterns_.begin(), patterns_.end(), std::make_pair(fixed, typed),
        [](std::pair<size_t, size_t> const& f, Pattern const& p) { return f.first != p.fixed ? f.first > p.fixed : f.second > p.typed; });
    patterns_.insert(at, Pattern{ std::move(lits), std::move(kinds), std::string(tr), fixed, typed });
    return true;
}

namespace {

bool IsPlaceholderLetter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; }

// The length of the placeholder starting at text[i] ("%item_link", "<target>"), or 0 when there is none.
size_t PlaceholderAt(std::string_view text, size_t i)
{
    if (text[i] == '%')
    {
        size_t j = i + 1;
        while (j < text.size() && IsPlaceholderLetter(text[j])) ++j;
        return j > i + 1 ? j - i : 0;
    }
    if (text[i] == '<')
    {
        size_t j = i + 1;
        while (j < text.size() && text[j] >= 'a' && text[j] <= 'z') ++j;
        return (j > i + 1 && j < text.size() && text[j] == '>') ? j + 1 - i : 0;
    }
    return 0;
}

// The capture a chatter placeholder becomes: the ones BotTextMgr fills with a number, an amount of money, a chat link or a name
// match only that, so two lines that differ only there ("Anyone needs %my_role for %instance_name?", "Anyone needs
// %item_formatted_link for %cost_gold?") stay two templates, and a line that merely starts like a short chatter line ("hey ...",
// "got ...") is not taken for it.
std::string_view CaptureFor(std::string_view name)
{
    if (name == "%cost_gold") return "{$}";
    if (name == "%item_link" || name == "%item_formatted_link" || name == "%item_formatted_links" || name == "%formatted_item_links" ||
        name == "%quest_link" || name == "%quest_links" || name == "%category")
        return "{@}";   // filled with chat links: "got {}" must not take every line that starts with "got "
    if (name == "%s")
        return "{!}";   // the name of the player the bot answers ("hi %s"): "hey {}" must not take every line that starts "hey "
    if (name == "%my_level" || name == "%quest_obj_available" || name == "%quest_obj_required" || name == "%quest_obj_missing" ||
        name == "%amount")
        return "{#}";
    return "{}";
}

} // namespace

bool Catalog::AddChatter(std::string_view en, std::string_view tr)
{
    auto reject = [&]() { rejected_.emplace_back(en); return false; };
    if (en.find_first_of("{}") != std::string_view::npos || tr.find_first_of("{}") != std::string_view::npos) return reject();

    std::vector<std::string_view> names;   // the placeholders of `en`, in order: capture i + 1 is names[i]
    std::string en_tpl;
    for (size_t i = 0; i < en.size();)
    {
        if (size_t const len = PlaceholderAt(en, i))
        {
            std::string_view const name = en.substr(i, len);
            names.push_back(name);
            en_tpl += CaptureFor(name);
            i += len;
            continue;
        }
        en_tpl += en[i++];
    }
    if (names.size() > 9) return reject();

    std::string tr_tpl;
    for (size_t i = 0; i < tr.size();)
    {
        if (size_t const len = PlaceholderAt(tr, i))
        {
            auto const at = std::find(names.begin(), names.end(), tr.substr(i, len));
            if (at == names.end()) return reject();
            tr_tpl += '{';
            tr_tpl += char('1' + (at - names.begin()));
            tr_tpl += '}';
            i += len;
            continue;
        }
        tr_tpl += tr[i++];
    }
    if (!Add(en_tpl, tr_tpl))
    {
        rejected_.back() = std::string(en);   // name the database line, not its template
        return false;
    }
    return true;
}

namespace {

// Captures k-1 and the fixed texts from lits[k] on, starting at `pos` (just after lits[k-1]). The first place a fixed text fits
// is tried first, then the next ones; a number capture with no fixed text after it takes the whole run of digits.
bool MatchFrom(std::string_view text, std::vector<std::string> const& lits, std::vector<char> const& kinds, size_t k, size_t pos,
               std::vector<std::string_view>& caps)
{
    char const kind = kinds[k - 1];
    std::string const& last = lits.back();
    if (k + 1 == lits.size())
    {
        if (text.size() < pos + last.size()) return false;
        size_t const end = text.size() - last.size();
        if (text.substr(end) != last) return false;
        std::string_view const cap = text.substr(pos, end - pos);
        if (!CaptureFits(cap, kind)) return false;
        caps.push_back(cap);
        return true;
    }
    std::string const& lit = lits[k];
    size_t const limit = text.size() >= last.size() ? text.size() - last.size() : 0;
    if (lit.empty())   // after a number capture only
    {
        size_t q = pos;
        while (q < limit && IsNumberChar(text[q])) ++q;
        std::string_view const cap = text.substr(pos, q - pos);
        if (!CaptureFits(cap, kind)) return false;
        caps.push_back(cap);
        if (MatchFrom(text, lits, kinds, k + 1, q, caps)) return true;
        caps.pop_back();
        return false;
    }
    for (size_t q = text.find(lit, kind == '?' ? pos : pos + 1); q != std::string_view::npos && q + lit.size() <= limit;
         q = text.find(lit, q + 1))
    {
        std::string_view const cap = text.substr(pos, q - pos);
        if (!CaptureFits(cap, kind))
        {
            if (kind == '#') return false;   // a later place only makes the capture longer, with the same letters in it
            continue;
        }
        caps.push_back(cap);
        if (MatchFrom(text, lits, kinds, k + 1, q + lit.size(), caps)) return true;
        caps.pop_back();
    }
    return false;
}

} // namespace

bool Catalog::MatchPattern(Pattern const& p, std::string_view text, std::vector<std::string_view>& caps) const
{
    caps.clear();
    std::string const& first = p.lits.front();
    if (text.size() < p.fixed || text.substr(0, first.size()) != first) return false;
    return MatchFrom(text, p.lits, p.kinds, 1, first.size(), caps);
}

namespace {

// An amount of money in `units`; false when `cap` is not one.
bool MoneyInto(std::string_view cap, std::string const (&units)[3], std::string& out)
{
    if (units[0].empty() || !IsMoney(cap)) return false;
    out.clear();
    for (char c : cap)
    {
        if (c == 'g') out += units[0];
        else if (c == 's') out += units[1];
        else if (c == 'c') out += units[2];
        else out += c;
    }
    return true;
}

} // namespace

std::string Catalog::Fill(Pattern const& p, std::vector<std::string_view> const& caps, int depth) const
{
    std::string out;
    out.reserve(p.tr.size() + 32);
    int seq = 0;
    auto put = [&](int slot) {
        if (slot < 1 || slot > int(caps.size())) return;
        std::string_view const cap = caps[slot - 1];
        std::string sub;
        if (MoneyInto(cap, money_, sub)) out += sub;
        else if (depth < kMaxDepth && TranslateInto(cap, sub, depth + 1)) out += sub;
        else if (depth < kMaxDepth && TranslateList(cap, sub, depth + 1)) out += sub;
        else out.append(cap);
    };
    std::string_view const tr = p.tr;
    for (size_t i = 0; i < tr.size(); ++i)
    {
        if (tr[i] == '{' && i + 1 < tr.size() && tr[i + 1] == '}') { put(++seq); ++i; continue; }
        if (tr[i] == '{' && i + 2 < tr.size() && tr[i + 1] >= '1' && tr[i + 1] <= '9' && tr[i + 2] == '}') { put(tr[i + 1] - '0'); i += 2; continue; }
        out += tr[i];
    }
    return out;
}

bool Catalog::TranslateInto(std::string_view text, std::string& out, int depth) const
{
    if (text.empty()) return false;
    if (auto it = exact_.find(std::string(text)); it != exact_.end()) { out = it->second; return true; }

    std::vector<std::string_view> caps;
    for (Pattern const& p : patterns_)
        if (MatchPattern(p, text, caps)) { out = Fill(p, caps, depth); return true; }

    if (depth >= kMaxDepth) return false;

    // "<Name>: <rest>": a bot's own confirmation. The name stays; a known word before the colon is translated too.
    if (size_t const colon = text.find(": "); colon != std::string_view::npos && colon > 0 && colon + 2 < text.size())
    {
        std::string_view const head = text.substr(0, colon);
        if (head.find(' ') == std::string_view::npos)
        {
            std::string rest;
            if (TranslateInto(text.substr(colon + 2), rest, depth + 1))
            {
                std::string name;
                if (!TranslateInto(head, name, kMaxDepth)) name.assign(head);
                out = name + ": " + rest;
                return true;
            }
        }
    }

    // "a; b; c": a list line, each part on its own (the parts nobody translated stay as they are).
    if (text.find("; ") != std::string_view::npos)
    {
        std::string joined;
        bool any = false;
        size_t start = 0;
        while (true)
        {
            size_t const at = text.find("; ", start);
            std::string_view const part = text.substr(start, at == std::string_view::npos ? std::string_view::npos : at - start);
            std::string sub;
            if (TranslateInto(part, sub, depth + 1)) { joined += sub; any = true; }
            else joined.append(part);
            if (at == std::string_view::npos) break;
            joined += "; ";
            start = at + 2;
        }
        if (any) { out = std::move(joined); return true; }
    }
    return false;
}

bool Catalog::TranslateList(std::string_view text, std::string& out, int depth) const
{
    // A capture that is a comma list of known words ("Blessing of Might, pet off"): every part must be known, so a sentence that
    // merely has a comma in it is left alone.
    if (text.find(", ") == std::string_view::npos) return false;
    std::string joined;
    size_t start = 0;
    while (true)
    {
        size_t const at = text.find(", ", start);
        std::string_view const part = text.substr(start, at == std::string_view::npos ? std::string_view::npos : at - start);
        std::string sub;
        if (!TranslateInto(part, sub, depth)) return false;
        joined += sub;
        if (at == std::string_view::npos) break;
        joined += ", ";
        start = at + 2;
    }
    out = std::move(joined);
    return true;
}

std::string Catalog::Translate(std::string_view text) const
{
    std::string out;
    if (TranslateInto(text, out, 0)) return out;
    return std::string(text);
}

bool Catalog::Knows(std::string_view text) const
{
    std::string out;
    return TranslateInto(text, out, 0);
}

Catalog const& Russian()
{
    // Built once (thread-safe static), read-only afterwards.
    static Catalog const catalog = []
    {
        Catalog c(RussianEntries());
        for (Catalog::Entry const& e : RussianChatterEntries())
            c.AddChatter(e.en, e.tr);
        c.SetMoneyUnits("з", "с", "м");   // золото, серебро, медь: the Russian client's own money letters
        return c;
    }();
    return catalog;
}

bool NeverTranslate(std::string_view text)
{
    return text.substr(0, 7) == "ASSIGN|";
}

std::string Translate(Lang lang, std::string_view text)
{
    if (lang == Lang::En || NeverTranslate(text)) return std::string(text);
    return Russian().Translate(text);
}

} // namespace Playerbot::Locale
