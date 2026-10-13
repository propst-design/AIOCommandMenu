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

## Testing
- Jacob runs `Start-AIO-Test-Launcher.cmd` (tools/AIO-Watcher.ps1). Claude: `bash tools/runtest.sh
  <new-id> <preset> <polls>`; results in `.aio-requests/<id>.result.txt`.
- Extra server tests come from `tools/test-presets.txt` (re-read per request, no launcher restart):
  `tasks` (~100 s), `moves` (~160 s).
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
- Next: (done: Move-menu shifts) (Advance/Stay back/Flank = offset from the player's facing, ~50 m), Take cover,
  stance, Heal up, squad bar HUD, team colours on nameplates, then Attack maneuvers and Defense.
