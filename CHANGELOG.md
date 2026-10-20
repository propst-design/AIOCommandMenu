# Changelog

## 0.3.2 — unreleased

- Added Arma 3's behaviour modes under Stance & speed > Behaviour, for the whole squad or selected men:
  Safe (weapons down while quiet, raised as soon as there is danger), Aware (normal), Combat (low, running,
  weapons up) and Stealth (low, walking, weapons up, return fire only). The squad bar shows each man's mode.

## 0.3.1 — released

- Updated Workshop guidance for route recovery, persistent status and squad/vehicle teleport options.
- Added Admin > Teleport squad (where I look) to teleport the player and whole squad together to a target point/map cursor, maintaining relative formation spacing and non-colliding slots so units do not overlap.
- Added Admin > Teleport with vehicle to teleport the player's vehicle and all occupants inside it to the target point/map cursor.
- Added Admin > Teleport to my map cursor. Drivers bring their vehicle and its occupants; passengers and gunners teleport out alone. Requires the map open.
- Stalled routes now pause with the remaining plan intact, identify blocked soldiers and waypoint, and offer Resume or Skip waypoint under More > Routes. Recovery follows selected route members; skipping never counts as arrival.
- Added compact persistent feedback for current recipients, route/patrol progress, paused bound/peel reasons and commanded treatment. The full squad bar remains menu-only.

## 0.3.0 — released

Published on the Arma Reforger Workshop; release status updated 2026-10-15.
The changes below have automated server coverage; live checks of the new menu/map/contact
behavior and a second-player multiplayer check remain outstanding.

- Report issues at the [GitHub repository](https://github.com/propst-design/AIOCommandMenu/issues) (New issue > Bug report).
- Retain individual destinations and posts during contact, allow native firing from posts and temporary danger/self-aid reactions, and release posts on Engage at will.
- Replaced timed Bound forward with arrival-driven Bound to position. Two elements alternate until the ray/map destination is reached; cover readiness, pinning, casualties and blocked movement can pause or stop it, with status reports and a Resume bound / peel command.
- Marked ordinary Move here and route travel for relaxed marching in compatible Better Battle Buddies (BBB) builds. AiO itself does not lower weapons and does not require BBB; tactical orders retain their readiness.
- Added a radius selector to 360 defense: Auto (original squad-size-based radius), 5, 10, 20, 30 and 50 m. Feedback includes the chosen radius.
- Added Watch that direction and Watch: own choice under Defense, using the map cursor or camera target.
- Added Treat that soldier and Treat me under Supplies, using a nearby available squad member's real medical supplies and native treatment behavior.
- Added infantry route planning under More > Routes: up to 16 destinations, hold at the end or repeat as a patrol, with member selection fixed on the first draft point.
- Added element arrival and stalled-movement reports for routes and ordinary Move here. Reports appear with the menu closed and queue instead of overwriting one another; patrols stay quiet each lap. A stalled route stops and holds; a stalled ordinary Move here report leaves the game's movement order running.
- Added a Formation menu (Move > Formation), after Arma 3's, with Reforger's native Column, Staggered column, Wedge and Line. Always affects the whole squad and sets its formation for later formation movement; scenario or waypoint formation settings can override it.
- Moved Speed to the Stance page (now Stance & speed).
- Expanded the Reforger guide and aligned Workshop copy, credits and bug reporting with actual selection, targeting, admin access and AI limitations.

## 0.2.1 — 2026-10-14

- Made the Workshop item public; same content as 0.2.0.

## 0.2.0 — 2026-10-14

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
