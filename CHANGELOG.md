# Changelog

## 0.3.4 — unreleased

- Aim marker: while the command menu is open, a crosshair marks where an aimed order will land, with the distance. Its colour and shape show the kind of order (move, attack, defend, vehicle, supply), and it shrinks with distance like a real object. On Defense it outlines the building Garrison and Clear would use, and on the vehicle pages the vehicle your men would get into, unflip or repair. It turns grey with a red note when there is nothing to act on (no building, no vehicle, water). More > Aim marker switches between crosshair, a ring lying flat on the surface you aim at, or off, and sets the distance display and size.
- Mouse control, old Arma style (More > Mouse control on/off): with the menu open, the mouse wheel moves a highlight over the lines, left click gives the highlighted order and right click goes back or closes the menu. The aim marker then previews the highlighted order itself. Shooting and aiming down sights are blocked only while the menu is open; the number keys keep working. Not with the map open, where the mouse works the map.
- Your menu options (aim marker style, distance, size, mouse control and the squad bar) are now remembered across missions and restarts.
- Fixed: men told to Fire on my lead held their fire for good if you gave them any other order (Follow, Halt, Move here...) before you fired. A new order now hands their fire back.
- Fixed: a soldier you sent off elsewhere while he was pushing a vehicle upright was pulled back to formation when the push ended, cancelling your new order.
- Fixed: Disarm mines now walks the mines nearest-first instead of zig-zagging to the far end of the line after the first one.
- Fixed: admin Teleport with group could put a soldier at the corner of the map when there was no free spot near you; he now stays where he is.
- Fixed: men keeping station on a player (Advance, Flank) stood at his last position for good if he disconnected; they now rejoin the squad, as they already did when he died.
- Fixed: a soldier killed while in cover (Take cover, Fortify) left that spot reserved for the rest of the mission, so no other AI, friend or foe, would use it.
- Fixed: a soldier given a new order while disarming mines, or who couldn't reach the next mine, stayed crouched until you gave him a stance order.
- Fixed: after a mission restart on the same server (admin #restart), orders and jobs from the previous mission could carry over, and holds might not be watched. Everything AiO tracks now starts fresh with each mission.
- Fixed: men in Combat or Stealth mode stood up for good after a repair, a disarm or a crouched hold ended; they now go back to the mode's low stance.
- Fixed: ordering Unflip a second time on the same vehicle with other men left the first helpers standing at their push spots forever.
- Fixed: a seat a soldier was sent to stayed reserved for him if he never got there (killed, given another order), so nobody else could take it.
- Fixed: Lights and Engine orders switched vehicles driven by someone outside your squad (another player, another group's driver) when one of your men rode along as a passenger.
- Fixed: Disarm mines could send a man anywhere on the map and told you whether mines lay there; it now has the same 400 m limit as placing charges and mines.
- Fixed: Garrison and Clear on a distant building lost their reports if you died or respawned while the men walked up, and still posted the men if you stepped down or left the squad meanwhile.
- Fixed: a Bound or Peel in progress left the men holding posts nobody owned when you stepped down or left the squad; they now go back to the squad. Men moved to another group drop out of the bound.
- Fixed: Copy my stance and Fire on my lead kept following you after you stepped down or changed squads.
- Fixed: Resume bound / peel could stop with a script error if a soldier in it had just been removed.
- The command menu closes while you are unconscious (as the game's own commanding does) and orders are refused then.
- Mouse control: right click on the Select members page goes back instead of clearing your selection (Backspace still clears it).
- A blank line in the saved menu options file no longer drops the options after it.
- The game's own command wheel now overrides AiO: a wheel order to move, follow, stop or get in releases men you'd told to hold, and a wheel fire order ends per-man fire orders (Hold fire on selected men, Fire on my lead). Before, those men ignored the wheel.

## 0.3.3 — released

- More > Become leader / Step down as leader changes leadership in your own squad, including taking over from an AI leader while possessing a squad member. Another player cannot be displaced.
- Defense > Grenades & smoke: Frag there, Smoke there and Smoke screen there (up to three men) use the grenades your soldiers carry. Men throw from their posts or move up when the target is beyond throwing range, and frags are refused when a friendly is within 15 m of the target.
- Garrison building gets your men inside properly. Each man takes a window post facing out, with the nearest man on each post. Men no longer pick spots on top of beds and tables, they give up on unreachable posts in seconds rather than standing still for most of a minute, and they move to another free post. The squad remembers floors and rooms it found no way into. You get one report once everyone is in, saying how many men are in position and how many found no way in. Distant buildings load their navigation data as soon as you give the order.
- Clear building shares the rooms between the men, so they no longer queue in single file through narrow houses. A man whose rooms are all unreachable ends inside at a room he can reach.
- Fall back, Peel back and Attack > Flank now throw a smoke screen between the squad and the enemy when the squad is in contact, using men who carry smoke.

## 0.3.2 — released

Put your specialists to work: set the squad's behaviour, send a demolition man forward, repair a vehicle or bring a mortar into the fight.

### Set the pace

- Stance & speed > Behaviour adds Safe, Aware, Combat and Stealth for the whole squad or selected soldiers. Safe lowers weapons while quiet; Aware restores normal behaviour; Combat keeps weapons up with a low stance and running pace; Stealth uses a low stance, walking and return fire only. The squad bar shows each man's mode.

### Demolition and field support

- Supplies > Explosives & mines sends an equipped soldier to plant a demolition charge on the aimed vehicle, wall or ground. Choose a 30-second timer or a detonator-controlled charge, then use DETONATE when ready. The soldier moves clear after planting; charges attached to vehicles move with them. Remote detonation needs a living placer still carrying his detonator within range.
- Order soldiers to lay mines they carry or disarm armed mines around a target point. These orders use real equipment; they do not spawn explosives.
- Vehicles > Repair that vehicle sends a soldier with a repair kit to work on the target. Repairs follow the kit's normal limits; wrecks remain beyond field repair.
- More > Fire mission uses a squad soldier and a nearby mortar to fire 3 or 6 HE rounds, 3 smoke rounds or 1 illumination round at your aim or map cursor. It needs suitable shells nearby; Cease fire stops the mission.
- Supplies > Give me ammo and Give me bandages sends the nearest selected soldier with spares to bring them to you, keeping a reserve for himself.

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
