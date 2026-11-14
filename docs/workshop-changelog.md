0.4.0

An order marker, recruiting and dismissing soldiers, and a lights-off order.

NEW

- Order marker: a faint chevron with the order and distance marks where your squad is heading, whoever gave the order. It fades up close and while aiming, and hides with the map or a hidden HUD. More > Aim & order markers turns it off or sets its strength.
- Recruit / dismiss (More): recruit the AI soldier you aim at, his whole squad, or every free soldier of your side within 30 m. Dismissed men wait as their own AI squad for another player to recruit, so you can trade men; they leave without their colour team or a Lights off order. Destroy removes your own selected men; destroying soldiers outside your squad is for admins unless the host starts the server with -aioDestroy all. Squads a player leads or belongs to are never touched, and the server's limit on AI per player squad still applies.
- Lights (Stance & speed): Lights off keeps the squad's flashlights and vehicle lights off, day and night, until you choose Lights: own choice. The squad bar shows Lights off.

FIXED

- Your orders end a few seconds after nobody commands the squad (a Game Master lets go, a leader steps down or leaves), so the game, the Game Master or another mod can order the men again.
- The colour teams in the menu match the teams your men use after a reconnect or a takeover.
- Lighter on servers (opening the menu asks for the roster once), and a smaller download: the automated tests are no longer in the package.
- Halt, a whole-squad Move and the command wheel end Engage at will, so men stop chasing out of formation; men possessed when your orders ended are released too.
- Recruit and Destroy on a whole squad check every man's range and side, recruits start clean, and a dismissed man no longer keeps boarding your vehicle.
- With mouse control, holding a mouse button no longer keeps fire blocked after a keyboard order; a stance given after Take cover lasts through Move here.
- Smaller fixes: held speed on a new body, the order marker while possessing, head numbers over the map, the Defense page for keyboard users, orders from a leader without a body.
