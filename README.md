# AiO Command Menu (Reforger)

[![Game: Arma Reforger](https://img.shields.io/badge/Arma-Reforger-556b2f?style=flat-square)](https://reforger.armaplatform.com/)
[![Workshop release: 0.3.0](https://img.shields.io/badge/Workshop-0.3.0-blue?style=flat-square)](https://reforger.armaplatform.com/workshop/629405AEFC950C12)
[![Standalone addon](https://img.shields.io/badge/Addon-Standalone-teal?style=flat-square)](#version-and-installation)
[![License: GPL v3 or later](https://img.shields.io/badge/License-GPL--3.0--or--later-orange?style=flat-square)](LICENSE.md)

A numbered command menu for your AI squad, based on the design of
[Leopard20's All-In-One Command Menu for Arma 3](https://github.com/leopard20/All-In-One-Command-Menu).
Move the squad, give individual soldiers orders, assign colour teams, control
vehicles and manage supplies through the keyboard.

AiO is standalone: it requires Arma Reforger, with no other mod dependencies.
It uses the game's player-led AI squad setup, including Conflict and compatible
scenarios such as Arma Commander Actual (ACA). In Game Master, take control of a
soldier to command the other AI in that soldier's group. It does not recruit AI,
create a squad, or command arbitrary groups from the Game Master camera.

## Version and installation

The current public Workshop version is **0.3.0** (release status updated
15 October 2026). This README describes that version; formations, watch orders,
casualty treatment, routes, arrival popups, selectable defense radii and revised bounding are
new in that version. See [CHANGELOG.md](CHANGELOG.md) for the version history.

Get the mod from the [Arma Reforger Workshop](https://reforger.armaplatform.com/workshop/629405AEFC950C12)
and enable it for your scenario. Multiplayer requires the mod on the server and
joining clients; it is not a client-only menu. A second-player multiplayer
acceptance check is still outstanding.

## Quick start

1. Lead a squad with linked AI, or take control of a soldier in a Game Master AI group.
2. Press **T**, then choose an order with the number keys. No selected members means the whole squad.
3. For destination orders, aim at the location or put the map cursor there before choosing the order.
4. Use **Move > Follow me / Return to formation** to regroup.

The sections below are the full command reference. Start with [Controls and selection](#controls-and-selection)
and [Menu reference](#menu-reference), then consult movement, defense, vehicles/supplies or routes as needed.

## Controls and selection

- **T** opens or closes the menu. Rebind it under Settings > Controls >
  All-in-One Command Menu.
- **1–9** choose a line. **0** opens Select members on the top page and goes
  back on subpages. **Backspace** goes back; on Select members it clears the
  selection instead.
- Choosing an order normally closes the menu. Adding or clearing draft route
  points keeps it open.
- While the menu is open, number keys operate the menu instead of weapon and
  quick-slot selection.
- No selected members means **whole squad**. On Select members, **1–7** toggle
  soldiers, **8** cycles pages, **9** opens Teams and **0** returns to orders.
- Selection lasts for about **eight seconds with the menu closed**. Reopen it
  promptly for another order to the same men, and check the recipients shown in
  the menu heading. Clear selection with Backspace on Select members to order
  the whole squad again.
- Teams are Red, Green, Blue and Yellow; White means unassigned. On Teams,
  **1–4** select a team, **5–8** assign the selected men and **9** unassigns them.
  Assignments last for the current mission; they are not saved presets.
- Formation orders always affect the whole squad. Admin squad commands also
  ignore individual selection. Treat orders use selection to choose a helper.

Orders that need a destination use the spot under your camera aim. With the map
open, they use the **current map cursor position when you choose the order**;
there is no extra confirmation click. Map destinations use terrain height.
For building, vehicle, casualty or supply targets, aim at the object itself;
from the map, a trace from above identifies the object under the cursor.

## Menu reference

| Key | Page | Commands |
| --- | --- | --- |
| 1 | Move | Move here; Follow me / Return to formation; Advance; Stay back; Flank left; Flank right; Fall back; Hold here; Formation |
| 2 | Attack | Advance; Flank left; Flank right; Assault; Suppress; Fire on my lead; Bound to position; Peel back; Resume bound / peel |
| 3 | Vehicles | Get in (Any seat, Driver, Gunner, Passenger); Switch seat; Get out; Disembark passengers; EJECT; Vehicle controls; Unflip that vehicle |
| 4 | Defense | Take cover; Garrison building; Clear building; 360 defense (radius); Fortify here; Watch that direction; Watch: own choice |
| 5 | R.O.E. | Fire at will; Return fire only; Hold fire; Engage at will; Disengage |
| 6 | Stance & speed | Stand; Crouch; Prone; Auto; Copy my stance; Speed (Walk, Run, Sprint, Auto) |
| 7 | Supplies | Heal up!; Rearm; Rearm from that; Take that weapon; Treat that soldier; Treat me |
| 8 | More | Squad bar on/off; Admin (when allowed); Routes |
| 9 | HALT | Stop the current movement assignment |
| 0 | Select members | Select individuals or colour teams |

The Admin page is hidden without permission, so the Routes line number under
More changes with admin access. Follow labels when navigating that page.

## Movement and combat

**Move > Advance / Stay back / Flank left / Flank right** keep station about
50 m from you. Their bearing is set by your facing when ordered: the posts move
with your position but do not rotate when you turn. **Fall back** moves to fixed
posts about 40 m behind your order-time position. **Attack > Advance / Flank**
instead use the camera/map target. Attack flanks take a wide route to its side.

**Follow me / Return to formation** releases individual assignments and restores
the game's formation movement. **HALT** cancels current movement; selected men
hold where they are. **Hold here** with the whole normal player-led squad uses
the game's defend command at your position; selected men and GM-controlled
groups hold at their own positions.

**Move > 9 Formation** offers Column, Staggered column, Wedge and Line. It sets
the whole squad's native formation for subsequent formation movement. Individual
posts and tactical routes keep their own spacing. Scenario or waypoint formation
settings can override the choice; a custom scenario may not provide all four.

Individual infantry movement and posts take precedence over ordinary combat repositioning.
Men can fire from posts and temporarily react to immediate danger or available
self-treatment, then resume the retained order. **Engage at will** releases a
commanded post and permits pursuit; **Disengage** asks men to keep formation.
These are separate from fire permission: Disengage does not mean Hold fire.
**Fire on my lead** holds fire until your loaded ammunition count drops, then
switches the recipients to Fire at will.

**Assault** lets recipients pursue enemies near the target. For a normal whole
squad it uses the game's search-and-destroy command when available; selected
men and GM-controlled groups advance to spread posts with pursuit permitted.
**Suppress** uses the game's squad suppression command for a normal whole squad;
selected men and GM-controlled groups receive a 20-second suppression task.

**Stance & speed** sets recipients' stance or requested movement pace. Stance
Auto removes the forced stance; Copy my stance follows your stance as it changes.
Speed Auto removes the requested pace. These settings use the game's AI and do
not guarantee a particular pose or speed during every combat or vehicle action.

**Bound to position** needs at least two soldiers on foot. Two elements
alternate moves of up to 20 m toward spread posts at the camera/map destination.
**Peel back** uses up to 25 m legs to move each man 50 m opposite your order-time
facing. If the assigned men include a colour team and men outside it, the first
colour present (Red, then Green, Blue, Yellow) forms one element and the rest
form the other; otherwise soldiers alternate between elements.

Covering soldiers must be conscious, in position and armed with ammunition.
During known contact, cover also needs a visible target and permission to fire.
Pinning, aid needs or unavailable cover pause movement automatically. Blocked
movement or losses above 40% require **Attack > 9 Resume bound / peel**;
resuming still requires ready cover. Follow, Halt or new movement removes the
affected men from the maneuver. Losing an entire element, the leader or command
authority stops it. Arrival requires the remaining assigned men to reach their posts.

## Defense and buildings

**Take cover** searches within 25 m of each man, facing a notional threat ahead
of your facing. If no suitable cover is found, he crouches in place.
**Fortify here** finds cover within 20 m of the camera/map point, facing away
from you; it does not build fortifications.

**Defense > 4 360 defense** offers Auto or a 5, 10, 20, 30 or 50 m **radius**.
Auto scales with the number of recipients, between 5 and 12 m. Men crouch and
face outward around your position when ordered; the ring stays there when you
move. Selection limits the recipients.

**Garrison building** assigns distinct detected interior posts; extra men stay
in formation if there are too few posts. **Clear building** visits up to six
sampled interior positions and holds at the last reachable one. It does not
guarantee every room is visited or that the building is free of enemies.
Both depend on the building's geometry and the game's navigation. Men may first
approach a distant building so its interior can be detected. They try normal
door actions, abandon unreachable garrison posts, and skip stalled clearing
positions with feedback. Locked doors and unsupported buildings can prevent
completion. Use Follow to cancel and regroup.

**Watch that direction** points men toward the camera/map target. Existing
holders keep their positions; others hold where they are. Combat and danger can
still draw their attention. **Watch: own choice** removes the facing override
while leaving the hold in place; Follow returns them to formation.

## Vehicles and supplies

**Get in** targets the vehicle or static weapon you aim at, then your own
vehicle, then a seat-bearing object within 15 m of the target point. Soldiers
more than 400 m from it are refused. Any seat fills available driver, gunner and
passenger seats; specific roles require a free seat of that type.

Switch seat works in the soldier's current vehicle. **Disembark passengers**
only empties cargo seats; **Get out** includes crew; **EJECT** puts recipients
outside immediately. Vehicle controls act on vehicles occupied by the recipients,
not an empty vehicle you aim at. Engine controls require a driver. Lights on/off
sets headlights; **Lights: driver decides** restores the AI driver's choice.
**Unflip** sends up to four available men on foot to right the target vehicle.
Driving and boarding use the game's AI and remain subject to its pathfinding.

**Heal up!** tells wounded recipients to use their own medical supplies.
**Rearm** searches within 50 m of each man for useful bodies, loose gear,
vehicle/crate storage or arsenals. **Rearm from that** uses the aimed-at source.
Men walk to collect compatible ammunition and basic grenades/bandages, subject
to stock and inventory space; arsenal items follow the arsenal's supply costs. **Take that
weapon** sends one nearest eligible recipient to the aimed-at weapon or a
dropped weapon near the body you target.

**Supplies > 5 Treat that soldier / 6 Treat me** sends the nearest available
helper from the selected men, or the squad when nobody is selected. The patient
must be a wounded squadmate or you. Helper and patient must be on foot; the
helper must be conscious, have medical supplies and be within 200 m **of the
patient**. Treatment uses normal items and animations and may fail or stop;
it is not instant healing. On completion or failure, the helper returns to a
previous static post or rejoins formation. Follow or new movement for the helper
cancels the assignment.

## Routes, patrols and feedback

1. Select men or a colour team first; no selection means the whole squad.
2. Open the map, then T > More > Routes. Put the cursor on a destination and
   choose **Add waypoint here**. The menu stays open; repeat for up to 16 points.
   You can also add points from camera aim outside the map.
3. Choose **Start route** to hold at the last point, or **Start patrol** to loop
   through at least two points, returning from the last to the first.

The first point fixes the draft's members. Clear the draft before planning for
a different element. **Clear draft route** leaves running orders alone, and
Start retains the draft for reuse. Routes require soldiers on foot. Follow,
Halt or new movement cancels the affected men's route assignments; remaining
members can continue. Leader death or loss of command authority cancels the route.

Arrival popups identify the element and waypoint and wait for all remaining
assigned men to reach their spread posts. Ordinary **Move here** also reports
arrival, with a wider arrival area for native whole-squad movement. Dead,
reassigned or departed members no longer count toward completion. Notifications
queue and remain visible with the menu closed. Patrols do not announce each lap.

Routes stop and hold if there is no progress for 60 seconds or a leg exceeds a
distance-based travel allowance of at least five minutes; combat pauses these
budgets. Ordinary Move here uses the same reporting budgets, but a stalled
report does not cancel the game's movement order. Ordinary Move here retains
its existing vehicle movement path; multi-point routes are infantry only.

The squad bar shows names/team colours, health, magazines and current tasks
while the menu is open. Number markers appear over nearby men (within 120 m).
**More > Squad bar on/off** toggles these indicators.

## Admin page (More > Admin)

Commands are Heal me, Heal my squad (also heals you), Teleport me to the target
point, Teleport squad to me (skips mounted soldiers), and Kill what I look at.
Squad admin commands affect the whole squad regardless of selection.

The host/server startup option controls access:

| Option | Access |
| --- | --- |
| `-aioCheats admins` (default) | Single player, the listen-server host and logged-in admins |
| `-aioCheats all` | Every player |
| `-aioCheats off` | Nobody, including the host |

## Compatibility and current limits

Automated server checks have passed standalone and alongside ACA and Better
Battle Buddies (BBB); earlier infantry and vehicle features also have live
single-player testing. The 0.3.0 changes still need live menu/map/contact
checks and a second-player multiplayer check. Automated checks do not establish
compatibility with every scenario or mod combination.

- Vehicle Move here has worked in live map and camera tests, but an automated
  selected-driver movement regression remains unresolved. Multiplayer vehicle
  ownership/unflip and the full-squad church garrison retry still need live checks.
- Voice replies come from supported whole-squad commands routed through the
  game's commanding system. Individual and GM-possession orders use AiO text
  feedback instead.
- AiO does not itself lower weapons for relaxed marching. Compatible BBB builds
  can use ordinary Move here and route travel to do that when conditions allow;
  tactical orders keep their readiness. BBB is optional and its version matters.
- Input bindings extend the game's configuration. Rebind conflicting keys if
  needed. Combining AiO with another squad-command menu mod, including Enhanced
  AI Squad Commands, is unsupported.
- No soldiers listed: make sure you lead a player squad with linked AI, or are
  controlling a soldier in a GM AI group. Allow about a second for the roster to
  update. Nearby friendly AI are not automatically under your command.

## Reporting bugs

Use [GitHub Issues](https://github.com/propst-design/AIOCommandMenu/issues)
(New issue > Bug report). Include the mod and game versions, scenario, session
type, other mods, exact menu path, selected recipients and camera/map targeting.
Describe what happened and what you expected.

Attach `console.log` and `script.log` from the affected session. With the
default profile they are under
`Documents\My Games\ArmaReforger\logs` (use the newest affected session folder).
Custom profiles and dedicated servers may use another location. For multiplayer, include the
server logs and the affected client's logs when available.

## Credits and license

Original Arma 3 mod and design reference:
[Leopard20's All-In-One Command Menu](https://github.com/leopard20/All-In-One-Command-Menu).
This is an unofficial Reforger rebuild by Jacob (propst-design), developed with
Claude (Anthropic) and Codex (OpenAI). The original manual describes the Arma 3
mod; the controls and supported features here are specific to Reforger.

GNU GPL version 3 or later. See [LICENSE.md](LICENSE.md) for credits and notices,
and [COPYING](COPYING) for the full license. Script source is included with the
mod and available in [this repository](https://github.com/propst-design/AIOCommandMenu).
