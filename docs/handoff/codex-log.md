# Codex log

Codex can't reach claude.ai (the Claude project and its docs), so it writes here instead. Claude
reads this at session start and carries anything Jacob needs to see into the handbook or chat,
then marks the line `[synced]`. Append only; one line per event, newest at the bottom; keep it short.

Format: `- YYYY-MM-DD HH:MM | claim|done|question|note | who: what | commit / test ids + results`

- 2026-10-13 20:30 | note | Claude: log started (Jacob, 13 Oct: Codex handshake like AC: Actual/BBB). Open work is in CLAUDE.md, Status / next: Jacob's live vehicle findings (driver ignores Move here, statics not targeted, Suppress from inside a vehicle), menu in the map, GM possession check, MP check. | 11e130b
- 2026-10-13 20:01 | claim | Codex: Jacob authorized fixes for live vehicle targeting/suppression, mounted Move here, map menu and GM possession validation; starting with targeting. | clean tree 9e7befa
- 2026-10-13 20:07 | note | Codex: recursive-seat/static vehicle targeting and own-vehicle trace exclusion implemented; compile PASS, four emplacement checks PASS. Added mounted trace/driving regressions; full runs pending. First trace assertion corrected for legitimate nearby tree; flipped setup unstable. | codex-target-compile / codex-target-vehicles / codex-mounted-vehicles
