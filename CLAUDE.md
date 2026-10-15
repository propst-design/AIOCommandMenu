# All-in-One Command Menu (Reforger) - handbook for Claude

A GPLv3 port of Leopard20's All-In-One Command Menu (Arma 3) to Arma Reforger: a numbered
command menu for the player's AI squad plus a cheats/admin page. Owner: Jacob (propst-design).
Claude writes the code and docs; Jacob designs and play-tests. Keep this file current.
Claude project: "Command Enhancements" (its name predates the switch from DCE to AIO).

## Ground rules
- Standalone mod. No dependency on Arma Commander Actual (ACA), BBB or anything else; it must
  work in Conflict, Game Master and ACA. Not used with Enhanced AI Squad Commands.
- Licence: GPLv3 (LICENSE.md, COPYING). Never copy code from ACA (APL-SA, incompatible) or DCE.
  Talk to other mods only at runtime (by-name calls, the game's own group setup).
- Original source for behaviour reference: https://github.com/leopard20/All-In-One-Command-Menu
  (README has the full manual).
- Concise replies. Ask before big scope changes. Jacob iterates on the menu layout.

## Two agents (Claude and Codex, Jacob 13 Oct)
Codex works on this mod too under Jacob's direction and reads this same handbook (AGENTS.md points here).
- One handbook (this file), no parallel task lists. Codex can't reach claude.ai: it claims work and reports
  results in `docs/handoff/codex-log.md`. Claude: at session start read that log, carry what matters into
  this file (Status / next) and mark lines `[synced]`; append `- <date time> | note | Claude: ... | <sha>`
  lines for what Codex should know (Jacob's decisions, live findings, releases).
- Claim before starting (a `claim` line); Jacob's reassignments win. One testable tree: commit or stash
  before handing over, check `git status` and the `.aio-requests` queue first.
- Small commits, one change each, each agent's own attribution lines; note test runs and results.
- AGENTS.md, docs/ and tools/ never ship (make_release.sh copies only Scripts, Configs, rdb, README,
  licence files).

## Where things are
- Windows: `Documents\my games\ArmaReforgerWorkbench\addons\AIOCommandMenu`
  (Cowork VM: `$HOME/mnt/ArmaReforgerWorkbench--addons/AIOCommandMenu`). Mod GUID 629405AEFC950C12,
  class prefix `AIO_`.
- Vanilla source lookup: index the game paks (`E:\SteamLibrary\steamapps\common\Arma Reforger\addons`)
  with ACA's `tools/pakread.py` into `$HOME/pak.json` / `$HOME/pakcore.json` (rebuild each session).

## Code map (Scripts/Game/AIO)
- `AIO_Orders.c`: `AIO_EOrder` (RPC numbers: only append), `AIO_MenuItem`, `AIO_MenuTree.Build()` =
  the menu layout. Rearranging the menu = editing Build().
- `AIO_Menu.c`: client menu. Modes ORDERS / SELECT / TEAMS. Keys 1-9 lines, 0 = Select members
  (root) or Back, Backspace = back (on Select: clear selection), menu key closes. Selection is kept
  8 s after an order. Teams are client-side for now (Red/Green/Blue/Yellow, White = none).
  Look point = camera ray (1.5 km, excluding own vehicle) or the map cursor's terrain position when
  the map is open. Widgets built in script from vanilla Text_BodySmall layouts;
  removed in `OnBeforeWorldCleanup` (`AIO_Game.c`).
- `AIO_PlayerController.c`: modded SCR_PlayerController. OnUpdate drives the menu on the local
  controller. RPCs: roster (server -> owner every 1 s while open), orders, feedback.
- `AIO_Tasks.c`: orders to single soldiers (selected men/teams). Per-agent behaviours on the man's
  SCR_AIUtilityComponent: AIO_MoveBehavior (vanilla MoveIndividually, prio 58, combat 70+ still wins) +
  AIO_HoldBehavior (Wait.bt, prio 45 > formation 30). Release = fail both -> back to formation.
  Watchdog (1 s) walks holders back if >6 m off their spot. Whole-squad Halt/Follow/Move release all.
  Anchored holds (m_Anchor + m_vOffset): the spot moves with the player (Move menu).
  Interior holds use a 0.7 m move radius, a 1.5 m arrival tolerance, floor-height and building-bounds
  checks. Garrison abandons a post after 45 s without movement or 180 s travel; combat pauses the
  budget. Clear skips a stalled room after 20 s, reports reached/skipped positions and holds at the
  last reachable room; if no room is reachable it rejoins. Release marks our actions for removal.
  Arrival also needs floor support and a clear torso trace to the post (no arrival through a wall).
  Interior sampling requires standing clearance and building floor support, or an enclosed room
  for bare-ground/separate floors. Stalled men use vanilla nearby door actions, then replan; locked
  or nonresponsive doors do not reset the timeout indefinitely.
- Move menu (`AIO_Server.MoveMenu`): Advance / Stay back / Flank left / Flank right = keep station 50 m
  ahead/behind/left/right of the player's facing at order time (world-fixed bearing, moves with him),
  men in a line 5 m apart across that direction; Fall back = fixed spot 40 m behind, hold. Selection or
  whole squad. If the player dies the men rejoin formation.
- Infantry (`AIO_Server.Infantry`): Take cover = per man nearest free cover point within 25 m facing a
  threat 150 m ahead of the player (ChimeraCoverManagerComponent.GetBestCover, reserved via
  SetOccupiedCover, freed on release), crouch + hold; none found = crouch in place. Stance = character
  setting (origin COMMANDING, cause COMBAT) + immediate SetStanceChange; watchdog re-applies it (idle
  men never re-read settings); Auto removes it (prone men get up to crouch). Copy my stance = watchdog
  copies the player's stance each second. Heal up = vanilla SCR_AIHealBehavior on self for hurt men
  with a healing item (EUnitRole.MEDIC).
- `AIO_Supplies.c`: Rearm (nearest useful source within 50 m: dead bodies, loose items, vehicle/crate
  storages, arsenals), Rearm from that, Take that weapon. Needs per man: magazines per muzzle up to 6
  (launcher/handgun 2), 2 frag grenades if none, 2 bandages if none. Items move with
  TryMoveItemToStorage/TryInsertItem; arsenals spawn the prefab and charge supplies like vanilla
  SCR_AITakeItemFromArsenal. Magazine wells compared by type NAME (typename != unreliable).
  Characters count as loose items (they have an InventoryItemComponent): excluded. Fallen soldiers
  DROP their weapon: Take weapon looks within 3 m of a body. Errands (AIO_Fetch) walk him there,
  take, then restore his previous hold or Rejoin; he reports back to the player (AIO_Notify).
- `AIO_Maneuvers.c`: modded SCR_AICombatComponent.GetCombatMode (per-man ROE override; vanilla reads
  the group's mode) and SCR_AIUtilityComponent.ShouldKeepFormation (Engage at will / Disengage).
  Attack: Advance/Assault = line abreast at the point (whole-squad Assault = vanilla
  waypointSearchAndDestroy), Flank L/R = route (hold legs) wide out then beside the target,
  Suppress = per-man SCR_AISuppressBehavior on a 4 m sphere (whole squad: vanilla waypointSuppress),
  Fire on my lead = per-man hold fire until the player's loaded rounds drop, Bound/Peel = two elements
  (by colour team if the men are in 2+ teams, else alternating) moving 30/25 m in turns.
  Defense: 360 = ring around the player, crouched, looking out; Fortify = cover within 20 m of the point
  facing away from the player; Garrison/Clear = interior points (navmesh grid 2 m x 2.5 m floors inside
  the bounds, under a roof; else open points high up, e.g. a tower deck), garrison spreads over them
  looking out, clear walks the rooms nearest-first. BuildingOf = topmost ancestor with a model that is
  >= 4x4x2.5 m. Navmesh far from any AI is sparse (the ATC gave 57 hits from 16 m, 123 once the men were
  inside), so when no inside is found the men walk up to the building (AIO_Approach, retried every 5 s
  and on arrival, gives up after 90 s) and the order is re-issued; "[AIO] Interior of ..." logs counts.
  Teams are sent to the server (AIO_RpcTeam). Speed = character movement-speed setting (GROUP_GOAL).
- Hold behaviour extras: m_aLegs (routes), m_vLook (keeps looking, priority 15). `ActiveHold()` /
  `ActiveMove()` skip ended/removed tasks that linger in the queue. `Rejoin()` restores the game's
  MoveInFormation behavior and resets our keep-formation override. It replaces formation tasks tied
  to an old group activity so a late waypoint cancel cannot remove the new formation task. It no
  longer sends every man to the same leader position with a higher-priority individual move.
- `AIO_Vehicles.c` (Vehicles page, 13 Oct): per man through the game's own SCR_AIGetInVehicle /
  SCR_AIGetOutVehicle behaviours, so driving and formation stay vanilla (a seated man's vehicle joins the
  group's vehicles on entering). Target vehicle = the one looked at, else the player's own, else the
  nearest within 15 m of the look point; men > 400 m away refused. Seats are collected recursively
  (GetCompartments skips turrets on child entities). Get in as Any (driver, gunners, passengers) /
  Driver / Gunner / Passenger, seat reserved for him. Switch seat: same section = direct
  GetInVehicle switch, else get out + get in. Get out / Disembark passengers (cargo seats only) /
  EJECT (teleport out at once). After Get out/Eject the vehicle is dropped from the group's vehicle
  manager once no squad member is aboard or boarding (else the next move order re-boards them).
  Controls: engine on/off (needs a driver), lights on/off add a COMMANDING light-interaction setting on
  the AI driver (his idle behaviour switches Head lights by daylight otherwise); "driver decides"
  removes it. Unflip: up to 4 nearest men on foot walk there; with 2 (or all) at their spots around it (clear of its upright footprint, which
  otherwise hit them) for 2.5 s it is
  set upright (`SetPose`: Teleport + SetWorldTransform + Update + OnTransformReset; Teleport alone was
  undone by physics). Squad bar shows Driver/Gunner/Passenger/Boarding/Dismounting/Unflipping.
- `AIO_SquadBar.c`: bottom-centre strip, only while the menu is open (Jacob, 13 Oct; roster every 1 s then):
  key number, name (team colour), health, magazines, task label (`AIO_Tasks.TaskLabel`), plus the key
  number over each man's head in his team colour (within 120 m; every frame, render camera via
  BaseWorld.GetCurrentCamera; logs `[AIO] head markers:` skip reasons every 5 s when some man has none). More > Squad bar on/off.
- `AIO_Formations.c` (Formations, 14 Oct): Move > 9 Formation = Arma 3's formation menu (its order) limited to
  what Reforger handles natively: Column / Staggered column / Wedge / Line. Jacob, 14 Oct: only formations
  the game itself can do, no scripted shapes. Speed lives under Stance & speed > 6 Speed. Whole squad always (selection ignored; handled before ExecuteMen in Execute). Adds a
  COMMANDING `SCR_AIGroupFormationSetting` to the group's SCR_AIGroupSettingsComponent (the game's
  ActivityMove.bt node SCR_AISetGroupFormation re-reads it on every move; EDITOR/SCENARIO/WAYPOINT origin
  settings still outrank it) and applies it at once like that node (AIFormationComponent.SetFormation +
  every AIGroupMovementComponent handler). Only the game's four exist (SCR_AIWorld.et `Formations`:
  Wedge, Line, Column, StaggeredColumn; 3 m spacing). Arma 3's echelons, vee, diamond and file need new
  AIFormationDefinition entries in that prefab: a delta override `Prefabs/AI/SCR_AIWorld.et` (meta GUID
  E0A05C76552E7F58) + `modded enum SCR_EAIGroupFormation` was tried and NOT loaded by the dedicated
  server (prefab still 4 formations, with or without resourceDatabase.rdb; the world's AI world entity
  reports no prefab name). Untested: whether Workbench registering the override (rdb) changes that.
  Scripted keep-station shapes were declined (Jacob, 14 Oct). Add more entries only if the game's
  formation list grows.
- `AIO_Test.c`: server tests (`-aioTaskTest 1`: spawns a US fire team on GM_Arland, moves/holds/releases men).
- `AIO_Server.c`: squad = the player's group's slave AI group (`SCR_AIGroup.GetSlave()`), the same
  setup the vanilla commanding wheel and ACA's possession use. Whole-squad orders go through the
  game's own commands (`SCR_CommandingManagerComponent.RequestCommandExecution`: halt, follow,
  waypointMove, waypointDefend, waypointSuppress, holdFire/fireAtWill/returnFire) for voice replies;
  fallbacks without a commanding manager. Game Master "take control" of an AI soldier (Jacob, 13 Oct:
  "no units under your command"): `PossessedGroup` = the controlled body's own AI group (never a
  playable or slave group); whole-squad orders then go to every other man through ExecuteMen (no vanilla
  commanding for that group), and the possessed body is left out of the roster and admin squad lists.
  Admin page: SP, listen-server host, logged-in admins;
  `-aioCheats all|admins|off`.
- Input: `Configs/System/chimeraInputCommon.conf` is an OVERRIDE (delta, same GUID as vanilla) like
  ACE and most Workshop mods, so it stacks with other mods (ACA's gproj input config inherits it).
  `AIO_OpenContext` (prio 60, above the map's 50) holds the menu key (default T; Y is Game Master); `AIO_MenuContext`
  (prio 150, flags 0xa like vanilla weapon selection: 0x4 would show the mouse cursor) takes 0-9/Backspace while the menu is open so the
  weapon/quick-slot keys (SwitchWeaponCategoryN, prio 10) don't fire; it stays on 0.5 s after closing so the closing key's release is swallowed too. `keyBindingMenu.conf` override
  adds the Controls category. `AIO_MapMenuContext` (prio 150, flags 0xe) captures the same menu keys
  while preserving the map cursor; also used to swallow the closing key while the map is open.

- `AIO_Support.c` (14 Oct, Codex): Defense > 6 Watch that direction / 7 Watch: own choice.
  Existing holders keep their positions; others hold in place. Uses existing hold look priority
  (combat/danger still win). Supplies > 5 Treat that soldier / 6 Treat me selects the nearest available
  conscious helper on foot with MEDIC role (medical supplies), within 200 m. Targets own squad/player,
  on foot and wounded. Own subtype of vanilla SCR_AIMedicHealBehavior uses real items/animations;
  Release cancels it. Watchdog reports completion/failure, bounds it to 180 s and restores a previous
  static hold (with interior guard) or rejoins. No admin healing or dependency on another mod.
- `AIO_Routes.c` (14 Oct, Codex): More > Routes: client draft up to 16 points and roster IDs fixed at
  first Add; Start route holds at end, Start patrol loops (2+ points), Clear draft leaves active orders
  alone. Add keeps the menu open for changing the map cursor. Separate owner/server RPC validates
  membership; infantry only. Server dispatches one spread set of individual holds per leg, waits for
  all surviving assigned men within 5 m horizontally / 2.5 m vertically of their posts; stale/replaced
  holds and reassigned men drop out. No progress for 60 s or exceeding a distance-scaled travel budget
  (at least 300 s; 2 s/m + 120 s for longer legs) stops the route and reports; combat pauses budgets.
  Owner loss/death cancels. Patrols stay quiet each lap.
  Ordinary Move here also gets arrival observation (native whole-squad movement stays native).
  Feedback uses AIO_Notify and a client FIFO visible with the menu closed, independent of ACA markers.

## Workshop release
- Windows: `pwsh -File tools/make_release.ps1` (or `powershell -File tools/make_release.ps1`).
  On Unix: `bash tools/make_release.sh`. These refresh `addons/_AIO-release/AIOCommandMenu` (same GUID,
  title "AiO Command Menu (Reforger)"; only Scripts, Configs, rdb, README, CHANGELOG, licence files) and put
  `tools/workshop/` (summary, description, placeholder preview image) beside it. Re-run before every
  publish. One level down so the game/Workbench never see two copies in addons/.
  Existing thumbnail and publisher metadata are preserved; Scripts and Configs are replaced to avoid
  stale assets. The Windows builder validates its target paths before replacing managed directories.
- Jacob publishes from Workbench under his Bohemia login: open the release gproj, Publish, Unlisted first.
  PUBLISHED 13 Oct as "AiO Command Menu (Reforger)" 0.1.0, Unlisted: https://reforger.armaplatform.com/workshop/629405AEFC950C12
  (custom licence = LICENSE.txt). 0.2.0 PUBLISHED 14 Oct by Claude (computer use, Jacob's request), still
  Unlisted, change notes = tools/workshop/WORKSHOP-CHANGELOG.txt, description refreshed. 0.2.1 = same content, PUBLIC
  (Jacob, 14 Oct: go public before the MP check). Next: MP check from the Workshop copy.
- 0.2.0 prepared 13 Oct: vehicles, map commands, targeting and GM possession improvements since
  published 0.1.0. Release refreshed; NOT yet published. Paste `tools/workshop/WORKSHOP-CHANGELOG.txt`
  into Change Notes and enter `0.2.0` as Version in Workbench's publish dialog. The gproj has no version
  field; keep its GUID unchanged to update the existing Workshop item. Recommended numbering:
  0.2.1 for subsequent fixes, 0.3.0 for the next feature update; 1.0.0 once the stable release is ready.

## Testing
- Jacob runs `Start-AIO-Test-Launcher.cmd` (tools/AIO-Watcher.ps1). Claude: `bash tools/runtest.sh
  <new-id> <preset> <polls>`; results in `.aio-requests/<id>.result.txt`.
- Extra server tests come from `tools/test-presets.txt` (re-read per request, no launcher restart):
  `tasks` (~100 s), `moves` (~160 s), `infantry` (~110 s; prone can fail next to walls/at cover spots),
  `supply` (~100 s), `attack` (~210 s), `defense` (~270 s; finds a house near the map centre),
  `tower` (~400 s; defense on Arland's ControlTowerMilitary_E_01 + walk-up fallback),
  `vehicles` (~235 s; M1025: board, mounted look trace and Move here, switch seats, lights/engine,
  disembark, get out, eject, unflip; mortar/tripod target lookups too; 19 checks).
  Mounted Move here is a known failing regression (Codex, 13 Oct). The flipped setup can also fail when
  physics rights the vehicle before helpers reach it; distinguish setup from actual unflip failure.
- `recovery` (~130 s): a nine-man rifle squad without a playable master/slave, leader AI disabled
  to simulate GM possession. Impossible interior posts must release; Follow cancels approach jobs,
  restores formation despite a late waypoint cancel and follows the moved leader without clustering.
- `support` (~200 s), `support-aca`: Watch selected man / free look; native commanded treatment of a
  bleeding leader with every other helper's AI disabled; invalid/healthy/duplicate casualty checks;
  two real route legs waiting for the entire element, patrol repetition, selected cancellation, stalled
  and inaccessible-height handling, route validation, ordinary arrival and native observer cancellation.
- `formations` (~330 s): rifle squad (slave of a playable master) walks a 180 m square, one leg per
  formation (Column, Staggered column, Line, Wedge), shape sampled on the move every 4 s, best sample counts.
  Columns are checked fully; Line and Wedge sideways only (vs Column): the AI leader sprints and after a
  turn the wings of a wide formation trail 10-40 m behind (vanilla movement; full-shape wedge passed
  in about half the runs). `formations-aca` too.
- `<test>-aca` presets (tasks, moves, infantry, supply, attack, defense, vehicles) run the same tests with ACA + BBB
  on ACR_TestRange (ACA's AI commanders and battle running alongside). The test squad is made the slave
  of a playable "master" group like the player's real squad (`AIO_Test.MakeMaster/LinkToMaster`):
  ACA registers and commands every non-slave AI group, which sent the test men off to its bases.
  A finished test's last lines can be missing from console.log (killed before flush): check script.log.
- Presets: `compile` (dedicated server, GM_Arland, this mod only; done at "[AIO] ready"),
  `compile-aca` (with ACA on its test range). The launcher waits for another dedicated server (your SP game is fine;
  ACA's test server shares port 2001).
- Manual checks are Jacob's (menu feel, number keys not switching weapons, orders, MP).

## Status / next
- Built (12-13 Oct): menu shell, input capture, select-members and teams pages, whole-squad orders
  through vanilla commands (Halt, Follow, Move here, Hold here, Suppress, ROE), admin page (heal me,
  heal squad, teleport me, bring squad, kill target).
- 13 Oct: orders to selected men/teams (Move here, Halt/Hold here, Follow) - `tasks` test 5/5 PASS.
  Fixed live: no mouse cursor, closing key no longer reaches the hotbar. Input works after Workbench
  built resourceDatabase.rdb (needed for the input overrides to register; re-open in Workbench when
  adding new .conf overrides).
- 13 Oct: Move menu (Advance/Stay back/Flank L/R/Fall back) - `moves` 5/5 PASS (men within 1.7 m of
  their slots, also while the leader walks 40 m).
- 13 Oct: Take cover, Stance (incl. Copy my stance), Heal up, squad bar - `infantry` 7/7, `tasks` 6/6,
  `moves` 5/5.
- 13 Oct: Supplies (rearm, take weapon, ammo on the bar), Attack maneuvers, Defense, per-man ROE,
  engage/disengage, fire on my lead, speed, team markers. Server tests all pass: tasks 6/6, moves 5/5,
  infantry 7/7, supply 5/5, attack 7/7, defense 8/8.
- 13 Oct: Garrison/Clear failed on the airfield ATC ("No way into that building"): finer interior grid,
  open-deck fallback, BuildingOf ignores model-less roots, walk-up fallback. `tower` 10/10, `defense` 8/8.
  (The two VM exceptions in Jacob's log at that time were vanilla Game Master attribute UI, not ours.)
- 13 Oct: Jacob's live play-test PASSED: arsenals/vehicle/crate rearm, real firefights (suppress, flank
  under fire, fire on my lead), garrison on other buildings, head markers and squad bar, number keys.
  First version (infantry orders + admin page) is feature-complete in SP.
- 13 Oct: compatibility with ACA + BBB: compiles clean, all six `-aca` tests pass (tasks 6/6, moves 5/5,
  infantry 7/7, supply 5/5, attack 7/7, defense 8/8); Arland tasks/moves still pass. No code clashes:
  ACA skips slave groups (the player's squad) and player-led squads; BBB overrides different methods
  on the same classes and steps aside for HOLD_FIRE and player-priority actions.
- 13 Oct: Vehicles page (root slot 3; Take cover moved to the top of Defense): get in as any/driver/
  gunner/passenger, switch seat, get out, disembark passengers, eject, engine/lights, unflip.
  `vehicles` 13/13 PASS, `tasks` 6/6. Untested live: player-driven vehicles, AI driving the player around,
  unflip of a vehicle a client still owns in MP, statics (mortar/MG tripods as "vehicles").
- 13 Oct evening, Jacob's live vehicle check (GM launcher; follow-up status below):
  - Works well: get in/out, specific seats (e.g. gunner).
  - AI driver ignores Move here (whole squad and/or selected; check which), but obeys Advance. Suspect: Move
    here for a seated man = per-man AIO_MoveBehavior/hold (a seated driver doesn't drive on that); the
    keep-station path somehow does. Read both paths for a mounted driver; maybe give a vehicle's crew a group
    waypoint / drive order instead.
  - Mortars and tripod MGs: looking straight at the emplacement, men got into the nearby HMMWV instead.
    Suspect TargetVehicle: the looked entity isn't accepted by VehicleRoot (root without
    BaseCompartmentManagerComponent? seats on a child?) and the 15 m fallback only takes Vehicle.Cast, so
    the HMMWV wins. Accept any root whose subtree has seats; include statics in the nearest search.
  - Suppress given from inside the HMMWV: the gunner did nothing; from outside he suppressed perfectly.
    Suspect LookPoint: the camera ray excludes only the controlled character, so from inside it hits the
    own vehicle (window/hull) and the point is on the HMMWV. Exclude the player's vehicle from the trace
    (and from the target entity). Also check any "player in vehicle" checks in the attack path.
  - GM "take control" of an AI leader: roster now shows his men (first fix worked); orders said "Those men
    are no longer in your squad". e2c13ac sends whole-squad orders straight to the men and logs
    `[AIO] order ...: none of N ids matched` if selected-men orders still fail. Unconfirmed live.
  - Jacob (13 Oct): the menu must work in the map: open it while the map is open, and orders that need a
    point use the map cursor's world position instead of the camera ray (covers the planned map-click
    orders). Check that the input contexts still fire with the map open.
- Next: MP check with a second player, the vehicle findings above, then
  map-click orders. Formations built 14 Oct (see Status). Workshop prep DONE 13 Oct (release copy, texts, placeholder image).
- 13 Oct, Codex: vehicle targeting now checks seats recursively and includes static entities in the
  nearest search; camera traces exclude the player's vehicle and its children as well as the character.
  Compile `codex-target-compile` PASS. New mortar/tripod direct and nearest targeting checks PASS.
  `codex-mounted-vehicles`: 17/19 PASS; failures are mounted selected-men Move here (vehicle stayed
  ~33 m from a 35 m destination) and flipped setup (physics righted it three times). All four emplacement
  checks, mounted ray exclusion, boarding, seat switches, controls, dismount/eject and unflip end-state
  checks passed. Actual gunner fire still needs a live check. The first trace test's 5 m clear-ray
  assumption hit a nearby tree; corrected to check the actual exclusion.
- 13 Oct, Codex: menu can now open with the map, point orders use GetMapCursorWorldPosition + terrain
  height, with a vertical trace for entity targets. A separate menu input context preserves the cursor;
  opener priority raised above MapContext. `codex-map-compile` PASS; input/cursor and map orders need
  Jacob's live check. README now reflects Vehicles in root slot 3 and map destinations.
- 13 Oct, Codex: mounted Move here remains open. Experimenting with a group move waypoint caused
  the crew to dismount and did not drive the vehicle (`codex-groupdrive-vehicles`); that experiment
  was removed. Do not substitute a vanilla group waypoint blindly for the possessed/selected path.
  The task preset now resolves selected roster IDs and issues through ExecuteMen instead of calling
  MoveTo directly, to cover the selection path in e2c13ac: `codex-roster-tasks` 7/7 PASS.
  `codex-compat-compile` (ACA + its dependencies) PASS. Actual GM possession and MP remain live checks.
- 13 Oct, Jacob's live follow-up: "Seems like things are working. Driver Move worked from the map."
  Jacob also confirmed the driver responded to the camera-ray Move order. Both map and ray Move here
  work in his live session; selected vs whole squad was not specified. The automated mounted
  selected-driver failure still needs reconciliation with this live
  success (destination/obstacles/test setup or order scope). Do not assume mounted movement itself is
  universally broken or replace it before isolating that difference. Other recent fixes were reported
  as generally working, but individual suppression/static/possession checks were not specified.
- Next live checks for Jacob: T with the map open (cursor remains visible; numbers only operate the
  menu), Move here/Suppress at the map cursor, gunner Suppress from inside the HMMWV, and mortar/tripod
  Get in as Gunner. Driver Move from both map and camera ray passed live; next code work is reconciling the automated
  mounted Move here failure with that result, then live possession/MP and formations.
- 13 Oct, Codex: Jacob proposed optional Windows voice orders and supplied Workshop archive
  2582780947 (Asaayu's Integrated AI Voice Control System). Static inspection found System.Speech
  recognition in Arma 3 extension DLLs, XML command grammars, push-to-talk, confidence/silence
  settings and ExtensionCallback delivery to SQF. Included licence is APL-ND: reference behavior
  only; do not copy its scripts, grammars or DLLs into GPLv3 AiO. Proposed independent Windows
  companion plus local command bridge into existing AiO orders; retail-client bridge feasibility
  and installed Windows recognizer availability are untested. No voice feature implemented yet.
- 13 Oct, Jacob live: whole rifle squad (eight AI plus him), GM take control in the test scenario:
  garrison stalled in several buildings, including a church; Follow left the men clustered on him.
  Codex fixed: bounded interior movement, precise arrival/floor checks, door opening/replanning,
  distinct garrison posts (extra men stay in formation), unreachable-room skipping with feedback,
  removal of stale tasks, and native formation restoration that survives a late waypoint cancel.
  Whole-squad garrison/clear and Follow also remove old GM waypoints; selected orders leave other
  men's group waypoints alone. No teleporting or changes to RPC order numbers.
  Tests: `codex-garrison-recovery3` 5/5 (GM-style nine-man squad; about 24 m formation span),
  `codex-garrison-tasks1` 7/7, `codex-garrison-defense4` 8/8, `codex-garrison-tower3` 10/10.
  Earlier defense/tower failures exposed false room completion on the floor below and blocked
  positions; current tests distinguish reached vs skipped positions rather than claiming all rooms
  were visited. `codex-garrison-final-compile` (ACA) PASS; final vanilla compile also checked after
  tightening the nonresponsive-door guard. Release refreshed with the fixes and new change notes;
  source/release hashes verified and thumbnail preserved. Live full-squad/church retry is still
  Jacob's check; 0.2.0 remains unpublished.
- 14 Oct, Claude: Formations (Jacob: "Build formations"). Move > Formation (Arma 3's menu, native formations
  only, Jacob's call): Column, Staggered column, Wedge, Line; Speed moved to Stance & speed. Custom
  Arma 3 shapes blocked by the AI world prefab (see code map). Tests: `form-test15` formations 10/10, `form-aca3` formations-aca 10/10, `form-tasks1`
  tasks 7/7, `form-moves1` moves 5/5. Live check for Jacob: pick each formation
  while walking/running and confirm the men take it and keep it after Move here / Follow.
- 14 Oct, Codex (Jacob: "Work on those 3"): Watch direction, Treat casualty and queued map routes
  implemented in source (unreleased). Menu paths and behavior are in the code map above and README.
  Notifications wait for the entire surviving assigned element, include team/count and waypoint progress,
  and queue with the menu closed. Patrols do not announce every lap. Native Move here movement is
  preserved and observed for arrival. Existing RPC enum values preserved; eight values appended.
  Tests: `codex-support-final` 21/21 (vanilla, includes distance-scaled long map leg budget),
  `codex-support-aca1` 20/20 (ACA + BBB, before the final long-leg check),
  `codex-support-tasks1` 7/7. Actual native bandaging verified with other helpers' AI disabled;
  two route legs and repeated 20 m patrol legs walked, cancellation and stall reporting verified.
  Live checks for Jacob: Watch facing and free look, Treat me / looked-at squadmate and cancel,
  map route drafting with selected teams / whole squad, arrival popups with the menu closed and
  simultaneous team arrivals; owner-only feedback and route/treatment RPCs with a second player.
  No Workshop publication or release-copy refresh in this task.
