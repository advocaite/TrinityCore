# Auction house bot

The core already ships TrinityCore's AuctionHouseBot (seller lists items in the neutral/Alliance/Horde
houses, buyer buys player listings, `.ahbot` commands). This branch adds a vanilla-era item filter in
its own files (`AuctionHouseBotVanilla.{h,cpp}`) and a conf preset (`ahbot-vanilla.conf.snippet`).
Seller.cpp has a 3-line hook. Everything stays off unless `AuctionHouseBot.Seller.Enabled = 1`.

Filter (`AuctionHouseBot.Vanilla.Only = 1`): no gems/glyphs, no later-expansion items, quality up to epic,
required level <= `Vanilla.MaxReqLevel` (60), item level <= `Vanilla.MaxItemLevel` (100). Force-include list
(`AuctionHouseBot.forceIncludeItems`) bypasses it. Prices come from vendor buy/sell price x the
`AuctionHouseBot.*.Price.Ratio` keys. `AuctionHouseBot.Items.Crafted = 1` (presets) also lists items a profession recipe makes, which are in no loot or vendor
list and so were dropped by `Items.Misc = 0`; `Items.Crafted.MinStock = 1` (presets) keeps each of them listed at least once,
also while the house is full (the random fill only adds items as listings run out). `AuctionHouseBot.Class.WeightByPoolSize = 1` (presets) shares each quality's amount by class priority times
the number of different items in that class, so greens and blues are mostly gear instead of the same few bags, arrows and trade goods.
`AuctionHouseBot.Vanilla.ValueCurve = 1` (presets) prices by item level squared x quality (1/2/15/50/150 copper for gray to epic by default; the presets use 1/2/28/110/900 through `AuctionHouseBot.Vanilla.ValueCurve.Gray`..`.Epic`)
x class (gear, bags, trade goods 100%, recipes and misc 50%, consumables 25%, ammo and reagents vendor price only), never below
vendor price; the buyer's fair price uses the same value. The 70532 data gives loot-only goods such as essences a 1 copper vendor price. Per-bot posting/buying lives separately in
`Bot/States/Rules/AuctionRules.cpp`.

Not built or run in the cloud (no full toolchain): first test is on the PC. Check the startup log for
"AuctionHouseBot seller will use N items" (log channel `ahbot` at debug) and `.ahbot status`.

## Seller names
`AuctionHouseBot.Account` = an account id and `AuctionHouseBot.Sellers.Count` = N: at startup
`AuctionHouseBotSellers.cpp` tops that account up to N named level-60 characters (bare `characters` rows, mixed
Alliance/Horde races, never logged in) so listings show a real seller name. make-configs.ps1 uses account
2000000001 (no real account has it; must stay below 2^31) and 12 characters. Listings made before this change stay nameless.

## Buyer rules and commands
The core buyer (bids/buyouts on player listings, random chance by price) now also follows
`AuctionHouseBotBuyerPolicy.cpp`. Keys (worldserver.conf; all optional):
- `AuctionHouseBot.Buyer.<Alliance|Horde|Neutral>.Enabled` per-house on/off (stock, default 0).
- `AuctionHouseBot.Buyer.Commodities` (0): also buy stackable listings (herbs, ore, cloth), always the whole listing.
- `AuctionHouseBot.Buyer.MaxPriceMultiplier` (1.5): never pay more than this times the fair price per unit
  (item vendor buy price, or 4x vendor sell price). 0 = no cap.
- `AuctionHouseBot.Buyer.MinDelay` / `MaxDelay` (30 / 600 seconds): a listing is left alone for a delay in that
  range (stable per listing) so purchases don't look instant.
- `AuctionHouseBot.Buyer.DailyBudgetGold` (0 = unlimited) and `AuctionHouseBot.Buyer.MaxBuysPerHour` (60).
- Existing stock keys still apply: `Buyer.ChanceFactor`, `Buyer.ChanceMultiplier.<quality>`, `Buyer.Recheck.Interval`.
The bot never buys its own listings. Each purchase logs `[ahbot] buy <item> xN for <gold> in <house>` (channel `ahbot`, info).

GM commands (console or in game): `.ahbot status [all]` counts per house and quality, `.ahbot stats` buyer spend today and
limits, `.ahbot reload` re-read config, `.ahbot refresh` expire the bot's unbid listings now and restock,
`.ahbot add [n]` raise every active quality target by n per house (default 500) until the next reload,
`.ahbot rebuild [all]` stock command (`all` also expires bid-on bot listings), `.ahbot items ...` / `.ahbot ratio ...` stock.

## Fair-price buying
`AuctionHouseBot.Buyer.Enabled = 1` is the master switch; the per-house keys do nothing without it.
`AuctionHouseBot.Buyer.FairPricing = 1` replaces the stock chance roll: a listing at or under the fair price
(vendor buy price, or 4x vendor sell) is bought with certainty (once past its delay), above that the chance falls
linearly to `Buyer.FairMinChance` (35) percent at `Buyer.MaxPriceMultiplier` times fair, and nothing above the cap sells.
Unsold listings are rechecked every `Buyer.Recheck.Interval` minutes, so a fairly priced item sells in about the listing
delay (30 to 600 s) plus a few rechecks. `AllowTwoSide.Interaction.Auction = 1` (the dist default) puts every auctioneer on
the Neutral house, so stock and buy only there.
