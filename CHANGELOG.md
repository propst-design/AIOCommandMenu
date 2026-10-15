# Changelog

## Unreleased

- Added Watch that direction and Watch: own choice under Defense, using the map cursor or camera target.
- Added Treat that soldier and Treat me under Supplies, using a nearby available squad member's real medical supplies and native treatment behavior.
- Added infantry route planning under More > Routes: up to 16 destinations, hold at the end or repeat as a patrol, with member selection fixed on the first draft point.
- Added element arrival and stalled-movement reports for routes and ordinary Move here. Reports appear with the menu closed and queue instead of overwriting one another; patrols stay quiet each lap.
- Added a Formation menu (Move > Formation), after Arma 3's, with the formations Reforger supports: Column, Staggered column, Wedge, Line. The squad keeps the formation for later moves.
- Moved Speed to the Stance page (now Stance & speed).

## 0.2.0 — prepared 2026-10-13

- Added a Vehicles page: board as driver, gunner or passenger; switch seats; get out, disembark passengers or eject.
- Added engine and light controls, automatic driver light control, and squad-assisted vehicle unflip.
- Added vehicle seat and boarding/dismounting status to the squad bar.
- Added command menu support while the map is open. Destination orders use the map cursor, and the cursor remains visible.
- Improved static weapon targeting so mortars and tripod machine guns can be selected instead of a nearby vehicle.
- Fixed camera targeting from inside vehicles to ignore the player's vehicle and its attached parts.
- Added squad command support when taking control of an AI soldier in Game Master; the controlled soldier is excluded from the commanded roster.
- Moved Take cover to the Defense page; Vehicles now occupies root menu slot 3.
- Fixed recovery from inaccessible building positions: garrison gives up on blocked posts, and clear routes skip inaccessible rooms with feedback.
- Fixed Return to formation crowding soldiers onto the leader; it now restores vanilla formation tasks and cancels pending building orders.
- Improved interior-position checks and avoided assigning multiple soldiers to the same garrison post when a building has too few positions.
- Stalled interior movement can open nearby closed doors through the game's normal door actions and retry the path.

## 0.1.0 — 2026-10-13

- Initial Workshop release: infantry movement, attack and defense orders, stance and engagement controls, supplies, member/team selection, squad indicators, and admin commands.
