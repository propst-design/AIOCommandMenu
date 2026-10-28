// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). Licensed under the GNU GPL v3 or later (see LICENSE).

// Aim preview (issue #3): while the menu is open on a page with "look at" orders, a ring on the
// ground where the order would land, or an outline of the building / vehicle the page's orders
// would act on, with the distance. Red when the order would be refused for want of a target.
// It reads the same look point and target rules the server uses (AIO_Menu.LookPoint,
// AIO_Maneuvers.BuildingOf, AIO_Vehicles.TargetVehicle). Client only; hidden with the map open
// (the map cursor is the point there). More > "Aim marker on/off".
enum AIO_EAimKind
{
	GROUND,
	BUILDING,
	VEHICLE
}

class AIO_AimMarker
{
	static const int DOTS = 12;
	static const int DOT = 5;
	static const int BAR = 2;

	protected static ref AIO_AimMarker s_Instance;
	protected static bool s_bOff;

	protected ref array<ImageWidget> m_aDots = {};
	protected ref array<ImageWidget> m_aBars = {}; // outline: top, bottom, left, right
	protected ImageWidget m_wCentre;
	protected TextWidget m_wLabel;

	static AIO_AimMarker Get()
	{
		if (!s_Instance)
			s_Instance = new AIO_AimMarker();
		return s_Instance;
	}

	bool Toggle()
	{
		s_bOff = !s_bOff;
		if (s_bOff)
			Hide();
		return !s_bOff;
	}

	// Whether a page gets a marker, and which kind: the root (every aimed order starts there)
	// and pages with at least one "look at" order.
	static bool KindOf(AIO_MenuItem page, bool onRoot, out AIO_EAimKind kind)
	{
		kind = AIO_EAimKind.GROUND;
		bool aimed = onRoot;
		foreach (AIO_MenuItem item : page.m_aChildren)
		{
			if (item.m_eTarget != AIO_ETarget.POINT)
				continue;
			aimed = true;
			if (item.m_eOrder == AIO_EOrder.DEF_GARRISON)
				kind = AIO_EAimKind.BUILDING;
			if (item.m_eOrder == AIO_EOrder.VEH_GET_IN_ANY || item.m_eOrder == AIO_EOrder.VEH_UNFLIP)
				kind = AIO_EAimKind.VEHICLE;
		}
		return aimed;
	}

	void Update(SCR_PlayerController pc, AIO_MenuItem page, bool onRoot)
	{
		AIO_EAimKind kind;
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (s_bOff || !page || (mapEntity && mapEntity.IsOpen()) || !KindOf(page, onRoot, kind))
		{
			Hide();
			return;
		}
		if (!EnsureWidgets())
			return;
		BaseWorld world = GetGame().GetWorld();
		IEntity player = pc.GetControlledEntity();
		vector point;
		IEntity hit;
		if (!AIO_Menu.LookPoint(pc, point, hit))
		{
			HideShapes();
			ShowLabelAtCentre("Nothing in view", false);
			return;
		}
		// The order carries the nearest replicated parent of what the ray hit (AIO_Menu.Give).
		while (hit && !hit.FindComponent(RplComponent))
			hit = hit.GetParent();
		float distance = vector.Distance(player.GetOrigin(), point);
		string dist = string.Format("%1 m", Math.Round(distance));

		if (kind == AIO_EAimKind.BUILDING)
		{
			IEntity building = AIO_Maneuvers.BuildingOf(hit);
			Target(building, point, distance);
			if (building)
				Label(point, "Building  " + dist, true);
			else
				Label(point, "No building (Garrison, Clear)  " + dist, false);
			return;
		}
		if (kind == AIO_EAimKind.VEHICLE)
		{
			IEntity vehicle = AIO_Vehicles.TargetVehicle(hit, player, point);
			Target(vehicle, point, distance);
			if (!vehicle)
				Label(point, "No vehicle  " + dist, false);
			else if (vehicle == AIO_Vehicles.VehicleOf(player))
				Label(point, "Your vehicle", true);
			else
				Label(point, "Vehicle  " + dist, true);
			return;
		}
		HideBars();
		bool dry = point[1] >= world.GetOceanBaseHeight() + 0.3;
		Ring(point, distance, dry);
		if (dry)
			Label(point, dist, true);
		else
			Label(point, "Water  " + dist, false);
	}

	static int Tint(bool valid)
	{
		if (valid)
			return ARGB(230, 255, 210, 127);
		return ARGB(230, 255, 80, 70);
	}

	// Outline of the target; a ring instead when there is none (red) or it can't be outlined
	// because the camera is inside it (the player's own building or vehicle).
	protected void Target(IEntity target, vector point, float distance)
	{
		if (target && Outline(target))
		{
			HideDots();
			return;
		}
		HideBars();
		Ring(point, distance, target != null);
	}

	// A circle on the ground around the point, drawn as projected dots so it lies flat in
	// perspective. Its radius grows with distance so it stays readable far out.
	protected void Ring(vector point, float distance, bool valid)
	{
		WorkspaceWidget ws = GetGame().GetWorkspace();
		BaseWorld world = GetGame().GetWorld();
		float radius = Math.Clamp(distance * 0.025, 0.6, 12);
		int color = Tint(valid);
		for (int i = 0; i < DOTS; i++)
		{
			float angle = Math.PI2 * i / DOTS;
			vector at = point + Vector(Math.Cos(angle) * radius, 0.05, Math.Sin(angle) * radius);
			vector screen;
			if (!Project(ws, world, at, screen))
			{
				m_aDots[i].SetVisible(false);
				continue;
			}
			FrameSlot.SetPos(m_aDots[i], screen[0] - DOT / 2, screen[1] - DOT / 2);
			m_aDots[i].SetColorInt(color);
			m_aDots[i].SetVisible(true);
		}
		vector centre;
		if (Project(ws, world, point, centre))
		{
			FrameSlot.SetPos(m_wCentre, centre[0] - DOT / 2, centre[1] - DOT / 2);
			m_wCentre.SetColorInt(color);
			m_wCentre.SetVisible(true);
		}
		else
		{
			m_wCentre.SetVisible(false);
		}
	}

	// Screen rectangle around the entity's world bounds. False when part of it is behind the camera.
	protected bool Outline(IEntity entity)
	{
		WorkspaceWidget ws = GetGame().GetWorkspace();
		BaseWorld world = GetGame().GetWorld();
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
			if (!Project(ws, world, corner, screen))
				return false;
			left = Math.Min(left, screen[0]);
			right = Math.Max(right, screen[0]);
			top = Math.Min(top, screen[1]);
			bottom = Math.Max(bottom, screen[1]);
		}
		float width = ws.DPIUnscale(ws.GetWidth());
		float height = ws.DPIUnscale(ws.GetHeight());
		left = Math.Max(left, 0);
		top = Math.Max(top, 0);
		right = Math.Min(right, width);
		bottom = Math.Min(bottom, height);
		if (right - left < 4 || bottom - top < 4)
			return false;
		int color = Tint(true);
		SetBar(m_aBars[0], left, top, right - left, BAR, color);
		SetBar(m_aBars[1], left, bottom - BAR, right - left, BAR, color);
		SetBar(m_aBars[2], left, top, BAR, bottom - top, color);
		SetBar(m_aBars[3], right - BAR, top, BAR, bottom - top, color);
		return true;
	}

	protected void SetBar(ImageWidget bar, float x, float y, float w, float h, int color)
	{
		FrameSlot.SetPos(bar, x, y);
		FrameSlot.SetSize(bar, w, h);
		bar.SetColorInt(color);
		bar.SetVisible(true);
	}

	protected static bool Project(WorkspaceWidget ws, BaseWorld world, vector at, out vector screen)
	{
		vector camera[4];
		world.GetCurrentCamera(camera);
		if (vector.Dot(at - camera[3], camera[2]) <= 0.1)
			return false;
		screen = ws.ProjWorldToScreen(at, world);
		return screen[2] > 0;
	}

	protected void Label(vector point, string text, bool valid)
	{
		WorkspaceWidget ws = GetGame().GetWorkspace();
		vector screen;
		if (!Project(ws, GetGame().GetWorld(), point, screen))
		{
			m_wLabel.SetVisible(false);
			return;
		}
		FrameSlot.SetPos(m_wLabel, screen[0] + 12, screen[1] - 12); // right of the point
		m_wLabel.SetText(text);
		m_wLabel.SetColorInt(Tint(valid));
		m_wLabel.SetVisible(true);
	}

	protected void ShowLabelAtCentre(string text, bool valid)
	{
		WorkspaceWidget ws = GetGame().GetWorkspace();
		FrameSlot.SetPos(m_wLabel, ws.DPIUnscale(ws.GetWidth()) / 2 + 12, ws.DPIUnscale(ws.GetHeight()) / 2 + 16);
		m_wLabel.SetText(text);
		m_wLabel.SetColorInt(Tint(valid));
		m_wLabel.SetVisible(true);
	}

	protected bool EnsureWidgets()
	{
		if (m_wLabel)
			return true;
		WorkspaceWidget ws = GetGame().GetWorkspace();
		if (!ws)
			return false;
		for (int i = 0; i < DOTS + 1; i++)
		{
			ImageWidget dot = NewBox(ws, DOT, DOT);
			if (!dot)
				return false;
			if (i < DOTS)
				m_aDots.Insert(dot);
			else
				m_wCentre = dot;
		}
		for (int b = 0; b < 4; b++)
		{
			ImageWidget bar = NewBox(ws, BAR, BAR);
			if (!bar)
				return false;
			m_aBars.Insert(bar);
		}
		m_wLabel = TextWidget.Cast(ws.CreateWidgets("{B537936D5B7E2BA1}UI/layouts/WidgetLibrary/TextWidgets/Text_BodySmall.layout", ws));
		if (!m_wLabel)
			return false;
		m_wLabel.SetFlags(WidgetFlags.IGNORE_CURSOR);
		m_wLabel.SetZOrder(861);
		FrameSlot.SetSize(m_wLabel, 260, 24);
		return true;
	}

	protected ImageWidget NewBox(WorkspaceWidget ws, int w, int h)
	{
		ImageWidget box = ImageWidget.Cast(ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.IGNORE_CURSOR | WidgetFlags.BLEND, Color.FromRGBA(255, 210, 127, 230), 0, ws));
		if (!box)
			return null;
		FrameSlot.SetSize(box, w, h);
		box.SetZOrder(860);
		box.SetVisible(false);
		return box;
	}

	protected void HideDots()
	{
		foreach (ImageWidget dot : m_aDots)
			dot.SetVisible(false);
		if (m_wCentre)
			m_wCentre.SetVisible(false);
	}

	protected void HideBars()
	{
		foreach (ImageWidget bar : m_aBars)
			bar.SetVisible(false);
	}

	protected void HideShapes()
	{
		HideDots();
		HideBars();
	}

	void Hide()
	{
		HideShapes();
		if (m_wLabel)
			m_wLabel.SetVisible(false);
	}

	static void Cleanup()
	{
		if (!s_Instance)
			return;
		foreach (ImageWidget dot : s_Instance.m_aDots)
		{
			if (dot)
				dot.RemoveFromHierarchy();
		}
		foreach (ImageWidget bar : s_Instance.m_aBars)
		{
			if (bar)
				bar.RemoveFromHierarchy();
		}
		if (s_Instance.m_wCentre)
			s_Instance.m_wCentre.RemoveFromHierarchy();
		if (s_Instance.m_wLabel)
			s_Instance.m_wLabel.RemoveFromHierarchy();
		s_Instance = null;
	}
}
