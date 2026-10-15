# All-in-One Command Menu for Arma Reforger

A numbered command menu for your AI squad, after Leopard20's All-In-One Command
Menu for Arma 3. Standalone: works in any scenario where you lead AI (Conflict,
Game Master, Arma Commander Actual...).

## Using it

- **T** opens the menu (rebind under Settings > Controls > All-in-One Command Menu).
- **1-9** pick a line, **0** = Select members (top page) or Back (sub-pages),
  **Backspace** = back, **T** again closes.
- While the menu is open, the number keys only drive the menu (no weapon or
  quick-slot switching).
- Orders go to the whole squad unless you pick men on the Select members page
  (1-7 toggle men, 8 next page, 9 Teams, 0 done, Backspace clears).
- Teams: Red, Green, Blue, Yellow (White = none). Select a team, or assign the
  selected men to one.
- Orders that need a place use where you are looking, or the map cursor while the map is open.

Top page: 1 Move, 2 Attack, 3 Vehicles, 4 Defense (including Take cover), 5 R.O.E., 6 Stance & speed, 7 Supplies,
8 More (squad bar on/off, Routes, Admin), 9 HALT, 0 Select members.

Formations: Move > 9 Formation: Arma 3's formation menu with the formations Reforger has itself:
Column, Staggered column, Wedge, Line (always the whole squad, also with men selected).
Speed: Stance & speed > 6 Speed.

Defense > 4 360 defense offers radii of 5, 10, 20, 30 or 50 m, plus Auto
(the original squad-size-based 5-12 m radius). The ring is centred on the player's
position when ordered. Men crouch and face outward; selection limits which men
receive the order. Distances are radii, not diameters.

Defense > 6 Watch that direction makes selected men watch the map cursor or the
spot you look at. Men already holding keep their positions; others hold where
they are. Enemies and danger can still draw their attention. Defense > 7 Watch:
own choice removes the facing order; Follow returns them to formation.

Attack > Bound to position uses the map cursor or the spot you look at. Two
elements alternate 20 m moves until the surviving assigned men reach that
destination. Peel back uses alternating 25 m moves over 50 m. A covering
element must be conscious, armed and ready; during known contact it also needs
a visible native target and permission to fire. Pinning, aid needs or unavailable
cover pause the maneuver. Blocked movement and heavy losses require Attack > 9
Resume bound / peel. Follow, Halt or another movement order cancels it.
Status popups report pauses, stops and arrival; timers never count as arrival.

Infantry movement and posts take precedence over ordinary combat repositioning.
Men can fire from their posts and react to immediate danger or self-treatment,
then resume the retained order. Engage at will releases a commanded post.
With BBB installed, quiet Move here and route travel can lower held weapons;
tactical movement and defense keep their combat readiness. AiO remains standalone.

Supplies > 5 Treat that soldier / 6 Treat me sends the nearest available selected
soldier with medical supplies to treat a squadmate or the player. With nobody
selected, it chooses from the squad. Both must be on foot; the helper must be
conscious and within 200 m. This uses normal medical items and treatment, not
admin healing. Follow or a new movement order cancels the helper's assignment.

More > Routes builds a route for infantry:

1. Select the men or colour team first (no selection means the whole squad).
2. Open the map, then T > More > Routes. Put the cursor on a destination and
   choose Add waypoint here. The menu stays open; repeat for up to 16 points.
3. Choose Start route to hold at the last point, or Start patrol to repeat a
   route with at least two points.

The first added point fixes the draft's members. Clear draft route before
planning for a different element; clearing a draft does not cancel a running
route. The draft is retained after Start for retrying. Follow, Halt or a new
movement assignment cancels the affected men's running route. Routes require
soldiers on foot; ordinary Move here keeps its existing vehicle support.

Arrival popups identify the element and waypoint, appear with the menu closed,
and wait for all surviving assigned men. Ordinary Move here also reports arrival.
Routes stop and report if stalled for 60 seconds or exceeding a travel allowance
that grows with waypoint distance (at least five minutes); combat pauses these
budgets. Patrols do not repeat arrival
popups every lap. Notifications queue so simultaneous reports remain readable.

## Admin page (More > Admin)

Single player and the host of your own game always; on dedicated servers only
logged-in admins. Host option: `-aioCheats all|admins|off`.

## Status

Infantry orders, vehicle orders, the admin page, and destinations from the map cursor
are implemented. Formations (column, staggered column, wedge, line) are in; multiplayer and recent fixes need live testing.
Version 0.3.0 is prepared for Workshop publication, including watch direction,
casualty treatment, infantry routes/patrols and arrival-driven bounding.
Live menu/map and multiplayer checks remain outstanding.

## Reporting bugs

Please report bugs on GitHub: https://github.com/propst-design/AIOCommandMenu/issues
(New issue > Bug report). Include your mod version, game mode, what you did, and
console.log / script.log from `Documents\My Games\ArmaReforger\profile\logs`.

## Credits

Original Arma 3 mod, design and manual: Leopard20 (All-in-One Command Menu). This
Reforger version is a new implementation by propst-design, written with Claude
(Anthropic), using the original as the behaviour reference.

## License

GPLv3. See LICENSE.md and COPYING.
