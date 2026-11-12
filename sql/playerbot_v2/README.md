# Playerbot V2 — SQL migrations

Per `v2/SCHEMA.md`. Migration files in this directory are numbered, append-only, and idempotent. Run by `Persistence/PlayerbotMigrationMgr` at module init.

For full setup instructions (schema creation, config, world fixes) see
`v2/INSTALL.md`.

## Order
Applied in ascending numeric order; each file records itself in
`playerbot_v2_schema_version`. Current set:

- `0001_init.sql` — schema_version + core tables (account, character, personality, preferences, population_target)
- `0002`–`0014` — owner/squad control, talent builds, population columns, guild metadata, world metadata, leveling zone, fleet vitals, archetype, craft orders, stuck-objective ledger
- `0015_name_pool.sql` — the curated ~107k-row random bot **name pool** (`playerbots_names`), shipped with use-state reset so a fresh install starts with the whole pool available
- `0016`–`0017` — talent builds for 12.1, shared-database split
- `0018_companion_roster.sql` — `companion_saved` on `playerbot_v2_character`: the owner's saved companion roster
- `0019_bot_texts.sql` — (shared DB) the bots' speech lines and per-key speak chances, carried over from the ike3 build's `ai_playerbot_texts`
- `0020_starter_pet_granted.sql` — `playerbot_v2_character.starter_pet_granted`: a hunter companion's starter pet is granted once, never again after its owner abandons it
- `0022_bot_text_weight.sql` — (shared DB) `playerbot_v2_bot_text.weight`: a line's relative odds within its key, restoring the ike3 lines that 0019's unique key had collapsed
- `0023_companion_gear_varied.sql` — `playerbot_v2_character.gear_varied`: a companion geared before per-bot gear variety gets one full gear re-roll, then never again
- `0024_companion_role.sql` — `playerbot_v2_character.companion_role`: a companion's declared role (tank/healer/dps), kept across relog, `.bot call` and restart
- `0025_owner_gear_mode.sql` — `playerbot_v2_owner_gear`: the gear mode (match / best / t1 / t2 / t3) an owner chose for their companions, per owner account
- `0038_owner_language.sql` — `playerbot_v2_owner_language`: the language (en / ru) a player chose for the bots' replies with `.bot language`, per account
- `0040_companion_talent_code.sql` — `playerbot_v2_character.companion_talents`: a companion's own talent build, given as a Wowhead Forever talent calculator code (`change talents <link>`), kept across relog and restart

## Discipline
- Never edit a numbered migration after it ships. Add a new one to amend.
- Each migration is self-contained: no cross-file dependencies on application order beyond numbering.
- Reversibility documented in the file header.
