// BotLocaleText - the bots' replies in the player's language (pure: no game objects, unit tested).
//
// A bot builds every reply in English, as it always has. The reply is translated where it leaves the server for one player
// (Api/BotLanguage.cpp: a whisper, a party / raid / say line, a channel or guild line, a `.bot` command reply), so no call site
// changes and a line nobody translated yet still arrives, in English.
//
// A catalog entry is an English template and its translation. In the English template `{}` stands for any non-empty text (a
// name, a number, an item), `{?}` for any text or none (an optional tail such as ", mana 40/90"), `{#}` for a number (digits,
// . - +), `{$}` for an amount of money ("1g 20s 5c"), `{@}` for text holding a chat link (|Hitem:...) and `{!}` for one word
// (a character's name). The translation places the captured texts with `{}` (in the English order) or `{1}`..`{9}` (any
// order). A captured text is translated too when it is itself in the catalog ("pulling Mob to {}" with "the rally point" as the
// capture), or is a comma list of catalog words; money gets the language's own letters.
//
// Lookup, in order: the exact text; the templates, most specific (most fixed letters) first; "<Name>: <rest>" with the rest
// translated (the per-bot confirmations "Rento: following you"); the "; " separated parts of a list line each on their own.
// Anything else comes back unchanged.

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Playerbot::Locale {

// The languages a reply can be given in. English is the source text; the others have a catalog.
enum class Lang : uint8_t
{
    En = 0,
    Ru = 1,
};

class Catalog
{
public:
    struct Entry
    {
        char const* en;
        char const* tr;
    };

    Catalog() = default;
    explicit Catalog(std::vector<Entry> const& entries) { for (Entry const& e : entries) Add(e.en, e.tr); }

    // False when the entry is malformed: an empty side, a template with two `{}` next to each other (no text between them to tell
    // the captures apart), a translation that uses a capture the template does not have, or a duplicate English text.
    bool Add(std::string_view en, std::string_view tr);

    // A line of the bots' stock chatter (the database texts of migration 0019) with its placeholders written the database's way:
    // `%name` (letters, digits, _) and `<name>` (lower-case letters). Each placeholder of `en` becomes a capture (`{}`, or `{#}` /
    // `{$}` / `{@}` / `{!}` for the ones BotTextMgr fills with a number / money / chat links / a name); each one in `tr` becomes the
    // capture of the same placeholder, so the translation may put them in any order. False (and refused, as Add) when `tr`
    // names a placeholder `en` has not got, either side has a `{` or `}` of its own, or there are more than 9.
    bool AddChatter(std::string_view en, std::string_view tr);

    // `text` in the catalog's language, or `text` unchanged when nothing matches.
    std::string Translate(std::string_view text) const;

    // True when Translate would change `text` (for the coverage counter and the tests).
    bool Knows(std::string_view text) const;

    size_t Size() const { return exact_.size() + patterns_.size(); }

    // A captured amount of money ("1g 20s 5c", BotTrade::FormatMoney) is written with these units instead of g / s / c.
    void SetMoneyUnits(std::string gold, std::string silver, std::string copper)
    {
        money_[0] = std::move(gold); money_[1] = std::move(silver); money_[2] = std::move(copper);
    }

    // The entries Add refused, as "english" lines (shown by the unit test, so a typo in the table is caught before a build).
    std::vector<std::string> const& Rejected() const { return rejected_; }

private:
    struct Pattern
    {
        std::vector<std::string> lits;   // fixed texts around the captures: captures = lits.size() - 1
        std::vector<char>        kinds;  // per capture: 'a' any text, '?' any or none, '#' a number, '$' money, '@' a chat link, '!' one word
        std::string              tr;
        size_t                   fixed = 0;   // letters in lits, for the most-specific-first order
        size_t                   typed = 0;   // captures of a narrow kind (# $ @ !), the tie-break of that order
    };

    bool TranslateInto(std::string_view text, std::string& out, int depth) const;
    bool TranslateList(std::string_view text, std::string& out, int depth) const;
    bool MatchPattern(Pattern const& p, std::string_view text, std::vector<std::string_view>& caps) const;
    std::string Fill(Pattern const& p, std::vector<std::string_view> const& caps, int depth) const;

    std::unordered_map<std::string, std::string> exact_;
    std::vector<Pattern>                         patterns_;   // most fixed letters first
    std::vector<std::string>                     rejected_;
    std::string                                  money_[3];   // empty: money stays as it is
};

// The Russian catalog (Util/BotLocaleRu.cpp): the code's lines, then the stock chatter. Built on first use.
Catalog const& Russian();
std::vector<Catalog::Entry> const& RussianEntries();
// The bots' stock chatter in Russian (Util/BotLocaleRu.cpp), added to Russian() with Catalog::AddChatter: the English side is the
// database line of migration 0019 exactly as stored (playerbot_v2_bot_text.text).
std::vector<Catalog::Entry> const& RussianChatterEntries();

// `text` for a player reading `lang`.
std::string Translate(Lang lang, std::string_view text);

// Lines that must reach the client exactly as written whatever the reader's language: the add-on's structured whispers.
bool NeverTranslate(std::string_view text);

} // namespace Playerbot::Locale
