# Optional server-owner options

Every option below is OFF in the shipped `worldserver.conf.dist`, which gives the Blizzlike (1.12) behaviour. Turn one on by setting it to `1` in your `worldserver.conf`. No SQL is involved.

| Option | What it does when on | Default |
|---|---|---|
| `Instance.MoltenCoreSkipRunes` | Molten Core without the runes: the seven runes count as doused (no Aqual Quintessence), Majordomo Executus stands in his hall, Ragnaros is in his lair and attackable. A boss already killed in the lockout stays dead. | 0 |
| `Instance.IgnoreAttunement` | No attunement item or quest is needed to enter an instance (Molten Core, Onyxia, Blackwing Lair, Naxxramas and so on). Level limits are separate: `Instance.IgnoreLevel`. | 0 |
| `Instance.AhnQirajGatesOpen` | The Gate of Ahn'Qiraj stands open, so the war effort and the Scarab Gong are not needed to reach Ruins and Temple of Ahn'Qiraj. | 0 |
| `Instance.PersistRaidTrash` | Raid trash that was killed stays dead until the lockout resets, including after you leave the instance, the instance unloads or the server restarts. Needs `Instance.BindOnEntry = 1` for players without a lockout yet. | 0 |
| `Instance.MoltenCoreHoundsLinked` | Core Hounds as in 1.12 (feign death and rise again unless the whole pack dies together). Off (standard): hound packs are unlinked and can be killed one by one. | 0 |

Note: the first four switch off Blizzlike rules; `MoltenCoreHoundsLinked` is the reverse, its off state is the shared default for this project (packs unlinked).
