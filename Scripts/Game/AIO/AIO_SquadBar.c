// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). Licensed under the GNU GPL v3 or later (see LICENSE).

// The squad bar: a strip at the bottom of the screen while the menu is open and you lead AI. One box per man:
// his number (the Select members key), name, health, team colour, what he's doing.
// Selected men are highlighted. More > "Squad bar on/off" hides it (remembered per game).
class AIO_SquadBar
{
	static const int BOX_W = 128;
	static const int BOX_H = 44;
	static const int GAP = 4;
	static const int BOTTOM = 96; // distance from the bottom edge
	static const int REDRAW_MS = 250;

	protected static ref AIO_SquadBar s_Instance;
	protected static bool s_bOff;

	protected ref array<ImageWidget> m_aBacks = {};
	protected ref array<TextWidget> m_aNames = {};
	protected ref array<TextWidget> m_aTasks = {};
	protected ref array<TextWidget> m_aMarks = {}; // number over each man's head, in his team colour
	protected int m_iNextDraw;

	static AIO_SquadBar Get()
	{
		if (!s_Instance)
			s_Instance = new AIO_SquadBar();
		return s_Instance;
	}

	bool Toggle()
	{
		s_bOff = !s_bOff;
		if (s_bOff)
			Hide();
		return !s_bOff;
	}

	void Update(int now, array<ref AIO_Member> members, array<RplId> selected)
	{
		if (s_bOff || members.IsEmpty())
		{
			Hide();
			return;
		}
		int count = members.Count();
		// Head markers follow the camera every frame; the boxes redraw every REDRAW_MS.
		if (now < m_iNextDraw)
		{
			UpdateMarks(now, members, selected);
			return;
		}
		m_iNextDraw = now + REDRAW_MS;
		int total = count * BOX_W + (count - 1) * GAP;
		for (int i = 0; i < count; i++)
		{
			if (!EnsureBox(i))
				return;
			AIO_Member member = members[i];
			bool isSelected = selected.Contains(member.m_Id);
			int x = i * (BOX_W + GAP) - total / 2;
			Place(m_aBacks[i], x, -BOTTOM - BOX_H, BOX_W, BOX_H);
			Place(m_aNames[i], x + 6, -BOTTOM - BOX_H + 3, BOX_W - 8, 20);
			Place(m_aTasks[i], x + 6, -BOTTOM - BOX_H + 22, BOX_W - 8, 20);
			if (isSelected)
				m_aBacks[i].SetColor(Color.FromRGBA(70, 90, 60, 210));
			else
				m_aBacks[i].SetColor(Color.FromRGBA(16, 20, 24, 150));
			// Numbers match the Select members page (1-7 per page).
			int key = i % AIO_Menu.MEN_PER_PAGE + 1;
			m_aNames[i].SetText(string.Format("%1 %2", key, member.m_sName));
			m_aNames[i].SetColorInt(AIO_Menu.TeamColor(member.m_iTeam));
			string task = member.m_sTask;
			if (task.IsEmpty())
				task = "-";
			string ammo = "";
			if (member.m_iAmmo >= 0)
				ammo = string.Format("  %1 mag", member.m_iAmmo);
			m_aTasks[i].SetText(member.m_iHealth.ToString() + "%" + ammo + "  " + task);
			int color = HealthColor(member.m_iHealth);
			if (member.m_iAmmo >= 0 && member.m_iAmmo <= 1 && member.m_iHealth >= 40)
				color = ARGB(255, 255, 200, 80); // low on ammo
			m_aTasks[i].SetColorInt(color);
			m_aBacks[i].SetVisible(true);
			m_aNames[i].SetVisible(true);
			m_aTasks[i].SetVisible(true);
		}
		for (int j = count; j < m_aBacks.Count(); j++)
		{
			m_aBacks[j].SetVisible(false);
			m_aNames[j].SetVisible(false);
			m_aTasks[j].SetVisible(false);
			m_aMarks[j].SetVisible(false);
		}
		UpdateMarks(now, members, selected);
	}

	// Diagnostics: why men got no marker (printed at most every 5 s while the menu is open).
	protected int m_iNoEntity, m_iNoCamera, m_iTooFar, m_iBehind, m_iShown;
	protected int m_iNextReport;

	protected void UpdateMarks(int now, array<ref AIO_Member> members, array<RplId> selected)
	{
		m_iNoEntity = 0; m_iNoCamera = 0; m_iTooFar = 0; m_iBehind = 0; m_iShown = 0;
		int count = Math.Min(members.Count(), m_aMarks.Count());
		for (int i = 0; i < count; i++)
		{
			AIO_Member member = members[i];
			PlaceMark(i, member, i % AIO_Menu.MEN_PER_PAGE + 1, selected.Contains(member.m_Id));
		}
		if (now >= m_iNextReport && m_iShown < count)
		{
			m_iNextReport = now + 5000;
			Print(string.Format("[AIO] head markers: %1 men, shown %2, not found %3, no camera %4, too far %5, off screen %6",
				count, m_iShown, m_iNoEntity, m_iNoCamera, m_iTooFar, m_iBehind));
		}
	}

	static IEntity BodyOf(RplId id)
	{
		Managed item = Replication.FindItem(id);
		RplComponent rpl = RplComponent.Cast(item);
		if (rpl)
			return rpl.GetEntity();
		return IEntity.Cast(item);
	}

	static const float MARK_RANGE = 120;

	// The man's key number over his head (team colour; brackets when selected), if he is in view.
	protected void PlaceMark(int index, AIO_Member member, int key, bool isSelected)
	{
		TextWidget mark = m_aMarks[index];
		mark.SetVisible(false);
		IEntity body = BodyOf(member.m_Id);
		if (!body)
		{
			m_iNoEntity++;
			return; // not streamed in here
		}
		BaseWorld world = GetGame().GetWorld();
		WorkspaceWidget ws = GetGame().GetWorkspace();
		if (!world || !ws)
		{
			m_iNoCamera++;
			return;
		}
		// The camera the world is rendered from (the same one ProjWorldToScreen uses).
		vector camera[4];
		world.GetCurrentCamera(camera);
		vector head = body.GetOrigin() + Vector(0, 2.1, 0);
		vector toHead = head - camera[3];
		float distance = toHead.Length();
		if (distance > MARK_RANGE)
		{
			m_iTooFar++;
			return;
		}
		vector screen = ws.ProjWorldToScreen(head, world);
		float width = ws.DPIUnscale(ws.GetWidth());
		float height = ws.DPIUnscale(ws.GetHeight());
		if (screen[2] <= 0 || vector.Dot(toHead, camera[2]) <= 0 || screen[0] < 0 || screen[1] < 0 || screen[0] > width || screen[1] > height)
		{
			m_iBehind++;
			return;
		}
		FrameSlot.SetAnchorMin(mark, 0, 0);
		FrameSlot.SetAnchorMax(mark, 0, 0);
		FrameSlot.SetAlignment(mark, 0, 0);
		FrameSlot.SetPos(mark, screen[0] - 20, screen[1] - 12);
		FrameSlot.SetSize(mark, 40, 24);
		string text = key.ToString();
		if (isSelected)
			text = "[" + text + "]";
		mark.SetText(text);
		mark.SetColorInt(AIO_Menu.TeamColor(member.m_iTeam));
		mark.SetVisible(true);
		m_iShown++;
	}

	static int HealthColor(int health)
	{
		if (health >= 75)
			return ARGB(255, 210, 210, 210);
		if (health >= 40)
			return ARGB(255, 255, 200, 80);
		return ARGB(255, 255, 90, 80);
	}

	void Hide()
	{
		foreach (ImageWidget back : m_aBacks)
			back.SetVisible(false);
		foreach (TextWidget name : m_aNames)
			name.SetVisible(false);
		foreach (TextWidget task : m_aTasks)
			task.SetVisible(false);
		foreach (TextWidget mark : m_aMarks)
			mark.SetVisible(false);
	}

	// Bottom-centre anchored placement.
	protected void Place(Widget w, int x, int y, int width, int height)
	{
		FrameSlot.SetAnchorMin(w, 0.5, 1);
		FrameSlot.SetAnchorMax(w, 0.5, 1);
		FrameSlot.SetPos(w, x, y);
		FrameSlot.SetSize(w, width, height);
	}

	protected bool EnsureBox(int index)
	{
		WorkspaceWidget ws = GetGame().GetWorkspace();
		if (!ws)
			return false;
		while (m_aBacks.Count() <= index)
		{
			ImageWidget back = ImageWidget.Cast(ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.BLEND, Color.FromRGBA(16, 20, 24, 150), 0, ws));
			TextWidget name = TextWidget.Cast(ws.CreateWidgets("{B537936D5B7E2BA1}UI/layouts/WidgetLibrary/TextWidgets/Text_BodySmall.layout", ws));
			TextWidget task = TextWidget.Cast(ws.CreateWidgets("{B537936D5B7E2BA1}UI/layouts/WidgetLibrary/TextWidgets/Text_BodySmall.layout", ws));
			TextWidget mark = TextWidget.Cast(ws.CreateWidgets("{B537936D5B7E2BA1}UI/layouts/WidgetLibrary/TextWidgets/Text_BodySmall.layout", ws));
			if (!back || !name || !task || !mark)
				return false;
			mark.SetFlags(WidgetFlags.IGNORE_CURSOR);
			mark.SetZOrder(870);
			mark.SetVisible(false);
			m_aMarks.Insert(mark);
			back.SetZOrder(880);
			name.SetFlags(WidgetFlags.IGNORE_CURSOR);
			task.SetFlags(WidgetFlags.IGNORE_CURSOR);
			name.SetZOrder(881);
			task.SetZOrder(881);
			m_aBacks.Insert(back);
			m_aNames.Insert(name);
			m_aTasks.Insert(task);
		}
		return true;
	}

	static void Cleanup()
	{
		if (!s_Instance)
			return;
		foreach (ImageWidget back : s_Instance.m_aBacks)
		{
			if (back)
				back.RemoveFromHierarchy();
		}
		foreach (TextWidget name : s_Instance.m_aNames)
		{
			if (name)
				name.RemoveFromHierarchy();
		}
		foreach (TextWidget task : s_Instance.m_aTasks)
		{
			if (task)
				task.RemoveFromHierarchy();
		}
		foreach (TextWidget mark : s_Instance.m_aMarks)
		{
			if (mark)
				mark.RemoveFromHierarchy();
		}
		s_Instance = null;
	}
}
