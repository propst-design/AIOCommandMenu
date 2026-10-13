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
  Look point = camera ray (1.5 km). Widgets built in script from vanilla Text_BodySmall layouts;
  removed in `OnBeforeWorldCleanup` (`AIO_Game.c`).
- `AIO_PlayerController.c`: modded SCR_PlayerController. OnUpdate drives the menu on the local
  controller. RPCs: roster (server -> owner every 1 s while open), orders, feedback.
- `AIO_Tasks.c`: orders to single soldiers (selected men/teams). Per-agent behaviours on the man's
  SCR_AIUtilityComponent: AIO_MoveBehavior (vanilla MoveIndividually, prio 58, combat 70+ still wins) +
  AIO_HoldBehavior (Wait.bt, prio 45 > formation 30). Release = fail both -> back to formation.
  Watchdog (2 s) walks holders back if >6 m off their spot. Whole-squad Halt/Follow/Move release all.
  Anchored holds (m_Anchor + m_vOffset): the spot moves with the player (Move menu).
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
- Hold behaviour extras: m_aLegs (routes), m_vLook (keeps looking, priority 15). `ActiveHold()` skips
  ended holds that linger in the queue. `Rejoin()` walks far-off men back to the leader (formation
  following alone was sometimes slow to pick them up).
- `AIO_SquadBar.c`: bottom-centre strip, only while the menu is open (Jacob, 13 Oct; roster every 1 s then):
  key number, name (team colour), health, magazines, task label (`AIO_Tasks.TaskLabel`), plus the key
  number over each man's head in his team colour (within 120 m; every frame, render camera via
  BaseWorld.GetCurrentCamera; logs `[AIO] head markers:` skip reasons every 5 s when some man has none). More > Squad bar on/off.
- `AIO_Test.c`: server tests (`-aioTaskTest 1`: spawns a US fire team on GM_Arland, moves/holds/releases men).
- `AIO_Server.c`: squad = the player's group's slave AI group (`SCR_AIGroup.GetSlave()`), the same
  setup the vanilla commanding wheel and ACA's possession use. Whole-squad orders go through the
  game's own commands (`SCR_CommandingManagerComponent.RequestCommandExecution`: halt, follow,
  waypointMove, waypointDefend, waypointSuppress, holdFire/fireAtWill/returnFire) for voice replies;
  fallbacks without a commanding manager. Admin page: SP, listen-server host, logged-in admins;
  `-aioCheats all|admins|off`.
- Input: `Configs/System/chimeraInputCommon.conf` is an OVERRIDE (delta, same GUID as vanilla) like
  ACE and most Workshop mods, so it stacks with other mods (ACA's gproj input config inherits it).
  `AIO_OpenContext` (prio 15) holds the menu key (default T; Y is Game Master); `AIO_MenuContext`
  (prio 150, flags 0xa like vanilla weapon selection: 0x4 would show the mouse cursor) takes 0-9/Backspace while the menu is open so the
  weapon/quick-slot keys (SwitchWeaponCategoryN, prio 10) don't fire; it stays on 0.5 s after closing so the closing key's release is swallowed too. `keyBindingMenu.conf` override
  adds the Controls category.

## Workshop release
- `bash tools/make_release.sh` builds the stripped copy `addons/_AIO-release/AIOCommandMenu` (same GUID,
  title "AiO Command Menu (Reforger)"; only Scripts, Configs, rdb, README, licence files) and puts
  `tools/workshop/` (summary, description, placeholder preview image) beside it. Re-run before every
  publish. One level down so the game/Workbench never see two copies in addons/.
- Jacob publishes from Workbench under his Bohemia login: open the release gproj, Publish, Unlisted first.
  PUBLISHED 13 Oct as "AiO Command Menu (Reforger)" 0.1.0, Unlisted: https://reforger.armaplatform.com/workshop/629405AEFC950C12
  (custom licence = LICENSE.txt). Next: MP check from the Workshop copy, then Public.

## Testing
- Jacob runs `Start-AIO-Test-Launcher.cmd` (tools/AIO-Watcher.ps1). Claude: `bash tools/runtest.sh
  <new-id> <preset> <polls>`; results in `.aio-requests/<id>.result.txt`.
- Extra server tests come from `tools/test-presets.txt` (re-read per request, no launcher restart):
  `tasks` (~100 s), `moves` (~160 s), `infantry` (~110 s; prone can fail next to walls/at cover spots),
  `supply` (~100 s), `attack` (~210 s), `defense` (~190 s; finds a house near the map centre),
  `tower` (~300 s; defense on Arland's ControlTowerMilitary_E_01 + walk-up fallback).
- `<test>-aca` presets (tasks, moves, infantry, supply, attack, defense) run the same tests with ACA + BBB
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
- Next: MP check with a second player, then vehicle orders (get in/out, seats, driver/gunner, unflip),
  formations, map-click orders. Workshop prep DONE 13 Oct (release copy, texts, placeholder image).
