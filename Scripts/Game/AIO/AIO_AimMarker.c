// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). Licensed under the GNU GPL v3 or later (see LICENSE).

// Aim preview (issue #3): while the menu is open on a page with "look at" orders, a marker where
// the order would land. Colour and shape follow the page's kind of order (move, attack, defend,
// vehicle, supply, admin); Defense outlines the building and the vehicle pages the vehicle the
// order would act on. Grey with a red label when the order would be refused for want of a target.
// It reads the same look point and target rules the server uses (AIO_Menu.LookPoint,
// AIO_Maneuvers.BuildingOf, AIO_Vehicles.TargetVehicle). The marker has a fixed size in the world,
// so it shrinks with distance; the ring style lies flat on the surface hit (ground, slope, wall).
// Client only; hidden with the map open (the map cursor is the point there).
// More > Aim marker: crosshair / ring / off, distance on/off, size. Not saved across missions.
enum AIO_EAimStyle
{
	CROSSHAIR,
	RING,
	OFF
}

// What an aimed order acts on: the point itself, a building or a vehicle.
enum AIO_EAimNeed
{
	POINT,
	BUILDING,
	VEHICLE
}

enum AIO_EAimType
{
	MOVE,
	ATTACK,
	DEFEND,
	VEHICLE,
	SUPPLY,
	ADMIN
}

class AIO_AimMarker
{
	static const int RING_DOTS = 16;
	static const float BAR = 1.5;
	static const int ALPHA = 160; // see-through, so the marker doesn't hide what it marks

	protected static ref AIO_AimMarker s_Instance;
	protected static AIO_EAimStyle s_eStyle = AIO_EAimStyle.CROSSHAIR;
	protected static bool s_bNoDistance;
	protected static int s_iSize = 1; // 0 small, 1 medium, 2 large

	protected ref array<ImageWidget> m_aRing = {};
	protected ref array<ImageWidget> m_aArms = {};    // crosshair: up, down, left, right
	protected ref array<ImageWidget> m_aBox = {};     // square around the point: top, bottom, left, right
	protected ref array<ImageWidget> m_aOutline = {}; // building / vehicle: top, bottom, left, right
	protected ImageWidget m_wDot;
	protected TextWidget m_wLabel;
	protected float m_fHalf; // the marker's half-size on screen, for placing the label clear of it

	static AIO_AimMarker Get()
	{
		if (!s_Instance)
			s_Instance = new AIO_AimMarker();
		return s_Instance;
	}

	// More > Aim marker lines. Returns the feedback line.
	string Option(AIO_EOrder order)
	{
		switch (order)
		{
			case AIO_EOrder.AIM_CROSSHAIR: s_eStyle = AIO_EAimStyle.CROSSHAIR; return "Aim marker: crosshair";
			case AIO_EOrder.AIM_RING: s_eStyle = AIO_EAimStyle.RING; return "Aim marker: ring on the surface";
			case AIO_EOrder.AIM_OFF: s_eStyle = AIO_EAimStyle.OFF; Hide(); return "Aim marker off";
			case AIO_EOrder.AIM_DISTANCE:
				s_bNoDistance = !s_bNoDistance;
				if (s_bNoDistance)
					return "Aim marker distance hidden";
				return "Aim marker distance shown";
			case AIO_EOrder.AIM_SIZE:
				s_iSize = (s_iSize + 1) % 3;
				if (s_iSize == 0)
					return "Aim marker size: small";
				if (s_iSize == 1)
					return "Aim marker size: medium";
				return "Aim marker size: large";
		}
		return "";
	}

	static bool IsOption(AIO_EOrder order)
	{
		return order == AIO_EOrder.AIM_CROSSHAIR || order == AIO_EOrder.AIM_RING || order == AIO_EOrder.AIM_OFF
			|| order == AIO_EOrder.AIM_DISTANCE || order == AIO_EOrder.AIM_SIZE;
	}

	// Whether a page gets a marker: the root (every aimed order starts there), pages with at least
	// one "look at" order, and the options page (to see the change). The page's kind of order and
	// what its orders act on.
	static bool TypeOf(AIO_MenuItem page, bool onRoot, out AIO_EAimType type, out AIO_EAimNeed need)
	{
		type = AIO_EAimType.MOVE;
		need = AIO_EAimNeed.POINT;
		bool aimed = onRoot;
		foreach (AIO_MenuItem item : page.m_aChildren)
		{
			if (item.m_eOrder == AIO_EOrder.AIM_CROSSHAIR)
				aimed = true;
			if (item.m_eTarget != AIO_ETarget.POINT)
				continue;
			aimed = true;
			AIO_EOrder order = item.m_eOrder;
			if (order == AIO_EOrder.DEF_GARRISON)
				need = AIO_EAimNeed.BUILDING;
			else if (order == AIO_EOrder.VEH_GET_IN_ANY || order == AIO_EOrder.VEH_UNFLIP)
				need = AIO_EAimNeed.VEHICLE;
			if (type == AIO_EAimType.MOVE)
				type = OrderType(order);
		}
		return aimed;
	}

	// The kind of a single order (colour and shape).
	static AIO_EAimType OrderType(AIO_EOrder order)
	{
		switch (order)
		{
			case AIO_EOrder.ATK_ADVANCE: case AIO_EOrder.ATK_FLANK_LEFT: case AIO_EOrder.ATK_FLANK_RIGHT:
			case AIO_EOrder.ASSAULT: case AIO_EOrder.SUPPRESS: case AIO_EOrder.BOUND:
			case AIO_EOrder.GREN_FRAG: case AIO_EOrder.GREN_SMOKE: case AIO_EOrder.GREN_SMOKE_SCREEN:
			case AIO_EOrder.FIRE_HE_3: case AIO_EOrder.FIRE_HE_6: case AIO_EOrder.FIRE_SMOKE: case AIO_EOrder.FIRE_ILLUM:
			case AIO_EOrder.ADMIN_KILL_TARGET:
				return AIO_EAimType.ATTACK;
			case AIO_EOrder.DEF_GARRISON: case AIO_EOrder.DEF_CLEAR: case AIO_EOrder.DEF_FORTIFY: case AIO_EOrder.WATCH_DIRECTION:
				return AIO_EAimType.DEFEND;
			case AIO_EOrder.VEH_GET_IN_ANY: case AIO_EOrder.VEH_GET_IN_DRIVER: case AIO_EOrder.VEH_GET_IN_GUNNER:
			case AIO_EOrder.VEH_GET_IN_CARGO: case AIO_EOrder.VEH_UNFLIP: case AIO_EOrder.VEH_REPAIR:
				return AIO_EAimType.VEHICLE;
			case AIO_EOrder.REARM_FROM: case AIO_EOrder.TAKE_WEAPON: case AIO_EOrder.TREAT_TARGET:
			case AIO_EOrder.EXP_PLACE_TIMED: case AIO_EOrder.EXP_PLACE_REMOTE: case AIO_EOrder.EXP_LAY_MINE: case AIO_EOrder.EXP_DISARM_MINES:
				return AIO_EAimType.SUPPLY;
			case AIO_EOrder.ADMIN_TELEPORT_ME: case AIO_EOrder.ADMIN_TELEPORT_GROUP: case AIO_EOrder.ADMIN_TELEPORT_VEHICLE:
				return AIO_EAimType.ADMIN;
		}
		return AIO_EAimType.MOVE;
	}

	// What a single order acts on.
	static AIO_EAimNeed OrderNeed(AIO_EOrder order)
	{
		if (order == AIO_EOrder.DEF_GARRISON || order == AIO_EOrder.DEF_CLEAR)
			return AIO_EAimNeed.BUILDING;
		if (OrderType(order) == AIO_EAimType.VEHICLE)
			return AIO_EAimNeed.VEHICLE;
		return AIO_EAimNeed.POINT;
	}

	static int TypeColor(AIO_EAimType type)
	{
		switch (type)
		{
			case AIO_EAimType.ATTACK: return ARGB(ALPHA, 255, 100, 60);
			case AIO_EAimType.DEFEND: return ARGB(ALPHA, 90, 170, 255);
			case AIO_EAimType.VEHICLE: return ARGB(ALPHA, 110, 230, 110);
			case AIO_EAimType.SUPPLY: return ARGB(ALPHA, 255, 215, 90);
			case AIO_EAimType.ADMIN: return ARGB(ALPHA, 200, 120, 255);
		}
		return ARGB(ALPHA, 240, 240, 240);
	}

	static int Grey()
	{
		return ARGB(ALPHA, 150, 150, 150);
	}

	static int Warn()
	{
		return ARGB(200, 255, 80, 70);
	}

	// focus: the order under the mouse highlight, if any; the marker then shows that order alone
	// (hidden for an order that doesn't use the look point).
	void Update(SCR_PlayerController pc, AIO_MenuItem page, bool onRoot, AIO_MenuItem focus = null)
	{
		AIO_EAimType type;
		AIO_EAimNeed need;
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		bool aimed = page && TypeOf(page, onRoot, type, need);
		string name = "";
		if (page && !onRoot)
			name = page.m_sLabel;
		if (focus && !AIO_AimMarker.IsOption(focus.m_eOrder))
		{
			aimed = focus.m_eTarget == AIO_ETarget.POINT;
			type = OrderType(focus.m_eOrder);
			need = OrderNeed(focus.m_eOrder);
			name = focus.m_sLabel;
		}
		if (s_eStyle == AIO_EAimStyle.OFF || !aimed || (mapEntity && mapEntity.IsOpen()))
		{
			Hide();
			return;
		}
		if (!EnsureWidgets())
			return;
		IEntity player = pc.GetControlledEntity();
		vector point, normal;
		IEntity hit;
		if (!AIO_Menu.LookSurface(pc, point, hit, normal))
		{
			Hide();
			WorkspaceWidget ws = GetGame().GetWorkspace();
			PlaceLabel(ws.DPIUnscale(ws.GetWidth()) / 2 + 12, ws.DPIUnscale(ws.GetHeight()) / 2 + 8, "Nothing in view", Warn());
			return;
		}
		// The order carries the nearest replicated parent of what the ray hit (AIO_Menu.Give).
		while (hit && !hit.FindComponent(RplComponent))
			hit = hit.GetParent();
		string dist = "";
		if (!s_bNoDistance)
			dist = string.Format("  %1 m", Math.Round(vector.Distance(player.GetOrigin(), point)));
		string text = name + dist;
		int color = TypeColor(type);
		bool valid = true;
		IEntity target;

		if (need == AIO_EAimNeed.BUILDING)
		{
			target = AIO_Maneuvers.BuildingOf(hit);
			if (!target)
			{
				valid = false;
				text = "No building here" + dist;
			}
		}
		else if (need == AIO_EAimNeed.VEHICLE)
		{
			target = AIO_Vehicles.TargetVehicle(hit, player, point);
			if (!target)
			{
				valid = false;
				text = "No vehicle here" + dist;
			}
			else if (target == AIO_Vehicles.VehicleOf(player))
			{
				text = "Your vehicle";
			}
		}
		else if (point[1] < GetGame().GetWorld().GetOceanBaseHeight() + 0.3)
		{
			valid = false;
			text = "Water" + dist;
		}
		if (!valid)
			color = Grey();

		// The building or vehicle is outlined; inside it (no outline possible) the marker alone shows.
		if (!target || !Outline(target, color))
			HideAll(m_aOutline);
		if (!Marker(point, normal, type, color))
		{
			m_wLabel.SetVisible(false);
			return;
		}
		int labelColor = color;
		if (!valid)
			labelColor = Warn();
		vector screen;
		Project(point, screen);
		PlaceLabel(screen[0] + m_fHalf + 8, screen[1] - 12, text.Trim(), labelColor); // past the right arm / ring
	}

	// The marker at the point, sized in metres so it shrinks with distance. False when off screen.
	protected bool Marker(vector point, vector normal, AIO_EAimType type, int color)
	{
		vector centre;
		if (!Project(point, centre))
		{
			HideAll(m_aRing);
			HideAll(m_aArms);
			HideAll(m_aBox);
			m_wDot.SetVisible(false);
			return false;
		}
		float metres = 0.3;
		if (s_iSize == 1)
			metres = 0.6;
		else if (s_iSize == 2)
			metres = 1.2;
		// Pixels per metre at the point, from a sideways step along the camera's right axis.
		vector camera[4];
		GetGame().GetWorld().GetCurrentCamera(camera);
		vector side;
		float perMetre = 0;
		if (Project(point + camera[0], side))
			perMetre = Math.AbsFloat(side[0] - centre[0]);
		float half = Math.Clamp(perMetre * metres, 5, 45);
		m_fHalf = half;
		float dot = Math.Clamp(half * 0.12, 2, 4);

		bool arms = type != AIO_EAimType.DEFEND;
		bool box = type == AIO_EAimType.ATTACK || type == AIO_EAimType.DEFEND;
		bool centreDot = type != AIO_EAimType.MOVE;
		if (s_eStyle == AIO_EAimStyle.RING)
		{
			Ring(point, normal, metres * 1.5, dot, color);
			m_fHalf = half * 1.5;
			arms = false;
			box = false;
			centreDot = true;
		}
		else
		{
			HideAll(m_aRing);
		}

		if (arms)
		{
			float gap = half * 0.35;
			float length = half - gap;
			Bar(m_aArms[0], centre[0] - BAR / 2, centre[1] - half, BAR, length, color);
			Bar(m_aArms[1], centre[0] - BAR / 2, centre[1] + gap, BAR, length, color);
			Bar(m_aArms[2], centre[0] - half, centre[1] - BAR / 2, length, BAR, color);
			Bar(m_aArms[3], centre[0] + gap, centre[1] - BAR / 2, length, BAR, color);
		}
		else
		{
			HideAll(m_aArms);
		}
		if (box)
		{
			float b = half * 0.7;
			Bar(m_aBox[0], centre[0] - b, centre[1] - b, b * 2, BAR, color);
			Bar(m_aBox[1], centre[0] - b, centre[1] + b - BAR, b * 2, BAR, color);
			Bar(m_aBox[2], centre[0] - b, centre[1] - b, BAR, b * 2, color);
			Bar(m_aBox[3], centre[0] + b - BAR, centre[1] - b, BAR, b * 2, color);
		}
		else
		{
			HideAll(m_aBox);
		}
		if (centreDot)
			Bar(m_wDot, centre[0] - dot / 2, centre[1] - dot / 2, dot, dot, color);
		else
			m_wDot.SetVisible(false);
		return true;
	}

	// A circle lying in the plane of the surface hit, drawn as projected dots.
	protected void Ring(vector point, vector normal, float radius, float dot, int color)
	{
		if (normal.Length() < 0.5)
			normal = Vector(0, 1, 0);
		normal.Normalize();
		vector axis = Vector(0, 1, 0);
		if (Math.AbsFloat(normal[1]) > 0.9)
			axis = Vector(1, 0, 0);
		vector u = Cross(normal, axis);
		u.Normalize();
		vector v = Cross(normal, u);
		vector lifted = point + normal * 0.05;
		for (int i = 0; i < RING_DOTS; i++)
		{
			float angle = Math.PI2 * i / RING_DOTS;
			vector screen;
			if (!Project(lifted + u * (Math.Cos(angle) * radius) + v * (Math.Sin(angle) * radius), screen))
			{
				m_aRing[i].SetVisible(false);
				continue;
			}
			Bar(m_aRing[i], screen[0] - dot / 2, screen[1] - dot / 2, dot, dot, color);
		}
	}

	protected static vector Cross(vector a, vector b)
	{
		return Vector(a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]);
	}

	// Screen rectangle around the entity's world bounds. False when part of it is behind the camera.
	protected bool Outline(IEntity entity, int color)
	{
		WorkspaceWidget ws = GetGame().GetWorkspace();
		vector mins, maxs;
		entity.GetWorldBounds(mins, maxs);
		float left = 100000, top = 100000, right = -100000, bottom = -100000;
		for (int c = 0; c < 8; c++)
		{
			vector corner = mins;
			if ((c & 1) != 0) corner[0] = maxs[0];
			if ((c & 2) != 0) corner[1] = maxs[1];
			if ((c & 4) != 0) corner[2] = maxs[2];
			vector screen;
			if (!Project(corner, screen))
				return false;
			left = Math.Min(left, screen[0]);
			right = Math.Max(right, screen[0]);
			top = Math.Min(top, screen[1]);
			bottom = Math.Max(bottom, screen[1]);
		}
		left = Math.Max(left, 0);
		top = Math.Max(top, 0);
		right = Math.Min(right, ws.DPIUnscale(ws.GetWidth()));
		bottom = Math.Min(bottom, ws.DPIUnscale(ws.GetHeight()));
		if (right - left < 4 || bottom - top < 4)
			return false;
		Bar(m_aOutline[0], left, top, right - left, BAR, color);
		Bar(m_aOutline[1], left, bottom - BAR, right - left, BAR, color);
		Bar(m_aOutline[2], left, top, BAR, bottom - top, color);
		Bar(m_aOutline[3], right - BAR, top, BAR, bottom - top, color);
		return true;
	}

	protected void Bar(ImageWidget bar, float x, float y, float w, float h, int color)
	{
		FrameSlot.SetPos(bar, x, y);
		FrameSlot.SetSize(bar, w, h);
		bar.SetColorInt(color);
		bar.SetVisible(true);
	}

	protected static bool Project(vector at, out vector screen)
	{
		BaseWorld world = GetGame().GetWorld();
		vector camera[4];
		world.GetCurrentCamera(camera);
		if (vector.Dot(at - camera[3], camera[2]) <= 0.1)
			return false;
		screen = GetGame().GetWorkspace().ProjWorldToScreen(at, world);
		return screen[2] > 0;
	}

	protected void PlaceLabel(float x, float y, string text, int color)
	{
		if (text.IsEmpty())
		{
			m_wLabel.SetVisible(false);
			return;
		}
		FrameSlot.SetPos(m_wLabel, x, y);
		m_wLabel.SetText(text);
		m_wLabel.SetColorInt(color);
		m_wLabel.SetVisible(true);
	}

	protected bool EnsureWidgets()
	{
		if (m_wLabel)
			return true;
		WorkspaceWidget ws = GetGame().GetWorkspace();
		if (!ws)
			return false;
		if (!AddBoxes(ws, m_aRing, RING_DOTS) || !AddBoxes(ws, m_aArms, 4) || !AddBoxes(ws, m_aBox, 4) || !AddBoxes(ws, m_aOutline, 4))
			return false;
		m_wDot = NewBox(ws);
		m_wLabel = TextWidget.Cast(ws.CreateWidgets("{B537936D5B7E2BA1}UI/layouts/WidgetLibrary/TextWidgets/Text_BodySmall.layout", ws));
		if (!m_wDot || !m_wLabel)
			return false;
		m_wLabel.SetFlags(WidgetFlags.IGNORE_CURSOR);
		m_wLabel.SetZOrder(861);
		FrameSlot.SetSize(m_wLabel, 320, 24);
		m_wLabel.SetVisible(false);
		return true;
	}

	protected bool AddBoxes(WorkspaceWidget ws, notnull array<ImageWidget> boxes, int count)
	{
		for (int i = 0; i < count; i++)
		{
			ImageWidget box = NewBox(ws);
			if (!box)
				return false;
			boxes.Insert(box);
		}
		return true;
	}

	protected ImageWidget NewBox(WorkspaceWidget ws)
	{
		ImageWidget box = ImageWidget.Cast(ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.IGNORE_CURSOR | WidgetFlags.BLEND, Color.FromRGBA(255, 255, 255, 230), 0, ws));
		if (!box)
			return null;
		box.SetZOrder(860);
		box.SetVisible(false);
		return box;
	}

	protected static void HideAll(array<ImageWidget> boxes)
	{
		foreach (ImageWidget box : boxes)
			box.SetVisible(false);
	}

	protected static void RemoveAll(array<ImageWidget> boxes)
	{
		foreach (ImageWidget box : boxes)
		{
			if (box)
				box.RemoveFromHierarchy();
		}
	}

	void Hide()
	{
		HideAll(m_aRing);
		HideAll(m_aArms);
		HideAll(m_aBox);
		HideAll(m_aOutline);
		if (m_wDot)
			m_wDot.SetVisible(false);
		if (m_wLabel)
			m_wLabel.SetVisible(false);
	}

	static void Cleanup()
	{
		if (!s_Instance)
			return;
		RemoveAll(s_Instance.m_aRing);
		RemoveAll(s_Instance.m_aArms);
		RemoveAll(s_Instance.m_aBox);
		RemoveAll(s_Instance.m_aOutline);
		if (s_Instance.m_wDot)
			s_Instance.m_wDot.RemoveFromHierarchy();
		if (s_Instance.m_wLabel)
			s_Instance.m_wLabel.RemoveFromHierarchy();
		s_Instance = null;
	}
}
