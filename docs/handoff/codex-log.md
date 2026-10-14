# Codex log

Codex can't reach claude.ai (the Claude project and its docs), so it writes here instead. Claude
reads this at session start and carries anything Jacob needs to see into the handbook or chat,
then marks the line `[synced]`. Append only; one line per event, newest at the bottom; keep it short.

Format: `- YYYY-MM-DD HH:MM | claim|done|question|note | who: what | commit / test ids + results`

- 2026-10-13 20:30 | note | Claude: log started (Jacob, 13 Oct: Codex handshake like AC: Actual/BBB). Open work is in CLAUDE.md, Status / next: Jacob's live vehicle findings (driver ignores Move here, statics not targeted, Suppress from inside a vehicle), menu in the map, GM possession check, MP check. | 11e130b
- 2026-10-13 20:01 | claim | Codex: Jacob authorized fixes for live vehicle targeting/suppression, mounted Move here, map menu and GM possession validation; starting with targeting. | clean tree 9e7befa
- 2026-10-13 20:07 | note | Codex: recursive-seat/static vehicle targeting and own-vehicle trace exclusion implemented; compile PASS, four emplacement checks PASS. Added mounted trace/driving regressions; full runs pending. First trace assertion corrected for legitimate nearby tree; flipped setup unstable. | codex-target-compile / codex-target-vehicles / codex-mounted-vehicles
- 2026-10-13 20:12 | done | Codex: map menu enabled; cursor terrain destination plus vertical entity trace, opener above vanilla MapContext and separate cursor-visible menu context. README corrected. Compile PASS; live map input/cursor/orders still required. | codex-map-compile; targeting commit 52daf98
- 2026-10-13 20:19 | done | Codex: selected roster-ID resolve and ExecuteMen task coverage PASS 7/7; compile with ACA PASS. Mounted vehicle run 17/19: statics, own-vehicle ray and original vehicle checks passed; mounted Move here and flipped setup failed. Group-waypoint experiment dismounted crew and was removed; driver issue stays open. Handbook updated with live checks. | codex-roster-tasks / codex-compat-compile / codex-mounted-vehicles; map commit 576f21f
- 2026-10-13 20:30 | claim | Codex: Jacob requested update changelog, version recommendation and refresh of the existing Workshop release copy; preparing 0.2.0 notes and preserving the release thumbnail. | baseline published 0.1.0 at 5b356a1
- 2026-10-13 20:32 | done | Codex: prepared 0.2.0 changelog since published 0.1.0, updated Workshop description, added safe Windows release builder and preserved thumbnail in both builders. Release copy refreshed for 0.2.0; NOT published. Verified 21 source/release files, matching GUID/title, unchanged thumbnail, no development files, matching Workshop texts; PowerShell syntax valid. | tools/workshop/WORKSHOP-CHANGELOG.txt / CHANGELOG.md
