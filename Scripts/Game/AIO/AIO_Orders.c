// All-in-One Command Menu for Arma Reforger
// Based on Leopard20's All-In-One Command Menu for Arma 3 (GPLv3); rebuilt in Enforce Script.
// Copyright (C) 2026 Jacob (propst-design). Licensed under the GNU GPL v3 or later (see LICENSE).

// Every order the menu can give. The numbers travel in RPCs: only ever add at the end.
enum AIO_EOrder
{
	NONE,
	HALT,
	FOLLOW,
	MOVE_HERE,
	TAKE_COVER,
	HOLD_HERE,
	ADVANCE,
	STAY_BACK,
	FLANK_LEFT,
	FLANK_RIGHT,
	FALL_BACK,
	ATK_ADVANCE,
	ATK_FLANK_LEFT,
	ATK_FLANK_RIGHT,
	ASSAULT,
	SUPPRESS,
	FIRE_ON_MY_LEAD,
	BOUND,
	PEEL,
	ENGAGE_AT_WILL,
	DISENGAGE,
	HOLD_FIRE,
	RETURN_FIRE,
	FIRE_AT_WILL,
	STANCE_UP,
	STANCE_CROUCH,
	STANCE_PRONE,
	STANCE_AUTO,
	COPY_STANCE,
	HEAL_UP,
	DEF_GARRISON,
	DEF_CLEAR,
	DEF_360,
	DEF_FORTIFY,
	ADMIN_HEAL_ME,
	ADMIN_HEAL_SQUAD,
	ADMIN_TELEPORT_ME,
	ADMIN_TELEPORT_SQUAD,
	ADMIN_KILL_TARGET,
	TOGGLE_SQUAD_BAR, // client only, never sent
	REARM,
	REARM_FROM,
	TAKE_WEAPON,
	SPEED_WALK,
	SPEED_NORMAL,
	SPEED_FAST,
	SPEED_AUTO,
	VEH_GET_IN_ANY,
	VEH_GET_IN_DRIVER,
	VEH_GET_IN_GUNNER,
	VEH_GET_IN_CARGO,
	VEH_TO_DRIVER,
	VEH_TO_GUNNER,
	VEH_TO_CARGO,
	VEH_GET_OUT,
	VEH_DISEMBARK,
	VEH_EJECT,
	VEH_ENGINE_ON,
	VEH_ENGINE_OFF,
	VEH_LIGHTS_ON,
	VEH_LIGHTS_OFF,
	VEH_UNFLIP,
	VEH_LIGHTS_AUTO,
	FORM_COLUMN,
	FORM_STAGGERED_COLUMN,
	FORM_WEDGE,
	FORM_LINE,
	WATCH_DIRECTION,
	WATCH_AUTO,
	TREAT_TARGET,
	TREAT_ME,
	ROUTE_ADD, // client draft controls; never executed as ordinary server orders
	ROUTE_START,
	ROUTE_PATROL,
	ROUTE_CLEAR,
	DEF_360_5,
	DEF_360_10,
	DEF_360_20,
	DEF_360_30,
	DEF_360_50,
	RESUME_MANEUVER,
	ROUTE_RESUME,
	ROUTE_SKIP,
	ADMIN_TELEPORT_MAP,
	ADMIN_TELEPORT_GROUP,
	ADMIN_TELEPORT_VEHICLE
}

// What an order needs from the player's side.
enum AIO_ETarget
{
	NONE,  // just the order
	POINT, // where the player looks
	MAP_POINT // map cursor only; requires an open map
}

// One line of the menu: an order, or a page of further lines.
class AIO_MenuItem
{
	string m_sLabel;
	AIO_EOrder m_eOrder;
	AIO_ETarget m_eTarget;
	bool m_bAdmin;
	ref array<ref AIO_MenuItem> m_aChildren;

	void AIO_MenuItem(string label, AIO_EOrder order = AIO_EOrder.NONE, AIO_ETarget target = AIO_ETarget.NONE)
	{
		m_sLabel = label;
		m_eOrder = order;
		m_eTarget = target;
	}

	bool IsPage()
	{
		return m_aChildren != null;
	}

	// Adds a sub-page and returns it.
	AIO_MenuItem Page(string label)
	{
		AIO_MenuItem page = new AIO_MenuItem(label);
		page.m_aChildren = {};
		Add(page);
		return page;
	}

	AIO_MenuItem Order(string label, AIO_EOrder order, AIO_ETarget target = AIO_ETarget.NONE)
	{
		AIO_MenuItem item = new AIO_MenuItem(label, order, target);
		Add(item);
		return item;
	}

	protected void Add(AIO_MenuItem item)
	{
		if (!m_aChildren)
			m_aChildren = {};
		m_aChildren.Insert(item);
	}
}

// The menu layout. Keys 1-9 pick the lines in order; 0 is always "Select members" on the
// top page and "Back" on sub-pages. Rearranging the menu = editing this list.
class AIO_MenuTree
{
	static AIO_MenuItem Build()
	{
		AIO_MenuItem root = new AIO_MenuItem("Orders");
		root.m_aChildren = {};

		AIO_MenuItem move = root.Page("Move");
		move.Order("Move here", AIO_EOrder.MOVE_HERE, AIO_ETarget.POINT);
		move.Order("Follow me / Return to formation", AIO_EOrder.FOLLOW);
		move.Order("Advance", AIO_EOrder.ADVANCE);
		move.Order("Stay back", AIO_EOrder.STAY_BACK);
		move.Order("Flank left", AIO_EOrder.FLANK_LEFT);
		move.Order("Flank right", AIO_EOrder.FLANK_RIGHT);
		move.Order("Fall back", AIO_EOrder.FALL_BACK);
		move.Order("Hold here", AIO_EOrder.HOLD_HERE);
		// Arma 3's formation menu, in its order, with the formations Reforger itself has
		// (no echelons, vee, file or diamond in the game's formation list).
		AIO_MenuItem formation = move.Page("Formation");
		formation.Order("Column", AIO_EOrder.FORM_COLUMN);
		formation.Order("Staggered column", AIO_EOrder.FORM_STAGGERED_COLUMN);
		formation.Order("Wedge", AIO_EOrder.FORM_WEDGE);
		formation.Order("Line", AIO_EOrder.FORM_LINE);

		AIO_MenuItem attack = root.Page("Attack");
		attack.Order("Advance", AIO_EOrder.ATK_ADVANCE, AIO_ETarget.POINT);
		attack.Order("Flank left", AIO_EOrder.ATK_FLANK_LEFT, AIO_ETarget.POINT);
		attack.Order("Flank right", AIO_EOrder.ATK_FLANK_RIGHT, AIO_ETarget.POINT);
		attack.Order("Assault", AIO_EOrder.ASSAULT, AIO_ETarget.POINT);
		attack.Order("Suppress", AIO_EOrder.SUPPRESS, AIO_ETarget.POINT);
		attack.Order("Fire on my lead", AIO_EOrder.FIRE_ON_MY_LEAD);
		attack.Order("Bound to position", AIO_EOrder.BOUND, AIO_ETarget.POINT);
		attack.Order("Peel back", AIO_EOrder.PEEL);
		attack.Order("Resume bound / peel", AIO_EOrder.RESUME_MANEUVER);

		AIO_MenuItem vehicles = root.Page("Vehicles");
		AIO_MenuItem getIn = vehicles.Page("Get in (that vehicle or mine)");
		getIn.Order("Any seat", AIO_EOrder.VEH_GET_IN_ANY, AIO_ETarget.POINT);
		getIn.Order("Driver", AIO_EOrder.VEH_GET_IN_DRIVER, AIO_ETarget.POINT);
		getIn.Order("Gunner", AIO_EOrder.VEH_GET_IN_GUNNER, AIO_ETarget.POINT);
		getIn.Order("Passenger", AIO_EOrder.VEH_GET_IN_CARGO, AIO_ETarget.POINT);
		AIO_MenuItem seats = vehicles.Page("Switch seat");
		seats.Order("To driver", AIO_EOrder.VEH_TO_DRIVER);
		seats.Order("To gunner", AIO_EOrder.VEH_TO_GUNNER);
		seats.Order("To passenger", AIO_EOrder.VEH_TO_CARGO);
		vehicles.Order("Get out", AIO_EOrder.VEH_GET_OUT);
		vehicles.Order("Disembark passengers", AIO_EOrder.VEH_DISEMBARK);
		vehicles.Order("EJECT (out at once)", AIO_EOrder.VEH_EJECT);
		AIO_MenuItem controls = vehicles.Page("Vehicle controls");
		controls.Order("Engine on", AIO_EOrder.VEH_ENGINE_ON);
		controls.Order("Engine off", AIO_EOrder.VEH_ENGINE_OFF);
		controls.Order("Lights on", AIO_EOrder.VEH_LIGHTS_ON);
		controls.Order("Lights off", AIO_EOrder.VEH_LIGHTS_OFF);
		controls.Order("Lights: driver decides", AIO_EOrder.VEH_LIGHTS_AUTO);
		vehicles.Order("Unflip that vehicle", AIO_EOrder.VEH_UNFLIP, AIO_ETarget.POINT);

		AIO_MenuItem defense = root.Page("Defense");
		defense.Order("Take cover", AIO_EOrder.TAKE_COVER);
		defense.Order("Garrison building", AIO_EOrder.DEF_GARRISON, AIO_ETarget.POINT);
		defense.Order("Clear building", AIO_EOrder.DEF_CLEAR, AIO_ETarget.POINT);
		AIO_MenuItem perimeter = defense.Page("360 defense (radius)");
		perimeter.Order("Auto (squad size)", AIO_EOrder.DEF_360);
		perimeter.Order("5 m", AIO_EOrder.DEF_360_5);
		perimeter.Order("10 m", AIO_EOrder.DEF_360_10);
		perimeter.Order("20 m", AIO_EOrder.DEF_360_20);
		perimeter.Order("30 m", AIO_EOrder.DEF_360_30);
		perimeter.Order("50 m", AIO_EOrder.DEF_360_50);
		defense.Order("Fortify here", AIO_EOrder.DEF_FORTIFY, AIO_ETarget.POINT);
		defense.Order("Watch that direction", AIO_EOrder.WATCH_DIRECTION, AIO_ETarget.POINT);
		defense.Order("Watch: own choice", AIO_EOrder.WATCH_AUTO);

		AIO_MenuItem roe = root.Page("R.O.E.");
		roe.Order("Fire at will", AIO_EOrder.FIRE_AT_WILL);
		roe.Order("Return fire only", AIO_EOrder.RETURN_FIRE);
		roe.Order("Hold fire", AIO_EOrder.HOLD_FIRE);
		roe.Order("Engage at will", AIO_EOrder.ENGAGE_AT_WILL);
		roe.Order("Disengage", AIO_EOrder.DISENGAGE);

		AIO_MenuItem stance = root.Page("Stance & speed");
		stance.Order("Stand", AIO_EOrder.STANCE_UP);
		stance.Order("Crouch", AIO_EOrder.STANCE_CROUCH);
		stance.Order("Prone", AIO_EOrder.STANCE_PRONE);
		stance.Order("Auto", AIO_EOrder.STANCE_AUTO);
		stance.Order("Copy my stance", AIO_EOrder.COPY_STANCE);
		AIO_MenuItem speed = stance.Page("Speed");
		speed.Order("Walk", AIO_EOrder.SPEED_WALK);
		speed.Order("Run", AIO_EOrder.SPEED_NORMAL);
		speed.Order("Sprint", AIO_EOrder.SPEED_FAST);
		speed.Order("Auto", AIO_EOrder.SPEED_AUTO);

		AIO_MenuItem supplies = root.Page("Supplies");
		supplies.Order("Heal up!", AIO_EOrder.HEAL_UP);
		supplies.Order("Rearm (nearest bodies, crates, vehicles)", AIO_EOrder.REARM);
		supplies.Order("Rearm from that", AIO_EOrder.REARM_FROM, AIO_ETarget.POINT);
		supplies.Order("Take that weapon", AIO_EOrder.TAKE_WEAPON, AIO_ETarget.POINT);
		supplies.Order("Treat that soldier", AIO_EOrder.TREAT_TARGET, AIO_ETarget.POINT);
		supplies.Order("Treat me", AIO_EOrder.TREAT_ME);

		AIO_MenuItem more = root.Page("More");
		more.Order("Squad bar on/off", AIO_EOrder.TOGGLE_SQUAD_BAR);
		AIO_MenuItem admin = more.Page("Admin");
		admin.m_bAdmin = true;
		admin.Order("Heal me", AIO_EOrder.ADMIN_HEAL_ME);
		admin.Order("Heal my squad", AIO_EOrder.ADMIN_HEAL_SQUAD);
		admin.Order("Teleport me (where I look)", AIO_EOrder.ADMIN_TELEPORT_ME, AIO_ETarget.POINT);
		admin.Order("Teleport squad to me", AIO_EOrder.ADMIN_TELEPORT_SQUAD);
		admin.Order("Kill what I look at", AIO_EOrder.ADMIN_KILL_TARGET, AIO_ETarget.POINT);
		admin.Order("Teleport to my map cursor", AIO_EOrder.ADMIN_TELEPORT_MAP, AIO_ETarget.MAP_POINT);
		admin.Order("Teleport squad (where I look)", AIO_EOrder.ADMIN_TELEPORT_GROUP, AIO_ETarget.POINT);
		admin.Order("Teleport with vehicle", AIO_EOrder.ADMIN_TELEPORT_VEHICLE, AIO_ETarget.POINT);
		AIO_MenuItem routes = more.Page("Routes (select men, then add points)");
		routes.Order("Add waypoint here", AIO_EOrder.ROUTE_ADD, AIO_ETarget.POINT);
		routes.Order("Start route (hold at end)", AIO_EOrder.ROUTE_START);
		routes.Order("Start patrol (repeat route)", AIO_EOrder.ROUTE_PATROL);
		routes.Order("Clear draft route", AIO_EOrder.ROUTE_CLEAR);
		routes.Order("Resume paused routes (selected men)", AIO_EOrder.ROUTE_RESUME);
		routes.Order("Skip blocked waypoint (selected routes)", AIO_EOrder.ROUTE_SKIP);

		root.Order("HALT", AIO_EOrder.HALT);
		return root;
	}
}
