// All-in-One Command Menu for Arma Reforger
// Based on Leopard20's All-In-One Command Menu for Arma 3 (GPLv3); rebuilt in Enforce Script.
// Copyright (C) 2026 Jacob (propst-design). Licensed under the GNU GPL v3 or later (see LICENSE).

enum AIO_EMenuMode
{
	ORDERS,
	SELECT,
	TEAMS
}

// One soldier under the player's command, as the server last reported him.
class AIO_Member
{
	RplId m_Id;
	string m_sName;
	int m_iHealth; // percent
	int m_iTeam;   // AIO_Menu.TEAM_*
	string m_sTask; // what he's doing (from the server), "" = with the squad
	int m_iAmmo = -1; // magazines for his weapon, -1 = unknown/none
}

// The menu on the local player's screen. Client only; orders go to the server through
// the player controller (modded in AIO_PlayerController.c).
class AIO_Menu
{
	static const int TEAM_WHITE = 0;
	static const int TEAM_RED = 1;
	static const int TEAM_GREEN = 2;
	static const int TEAM_BLUE = 3;
	static const int TEAM_YELLOW = 4;
	static const int MEN_PER_PAGE = 7;
	static const int SELECTION_KEEP_MS = 8000;
	static const int FEEDBACK_MS = 4000;
	static const int SWALLOW_MS = 500; // keep the menu's keys captured this long after closing

	protected static ref AIO_Menu s_Instance;

	protected ref AIO_MenuItem m_Root;
	protected ref array<AIO_MenuItem> m_aStack = {};
	protected AIO_EMenuMode m_eMode;
	protected bool m_bOpen;
	protected int m_iSelectPage;

	protected ref array<ref AIO_Member> m_aMembers = {};
	protected ref array<RplId> m_aSelected = {};
	protected ref map<RplId, int> m_mTeams = new map<RplId, int>(); // kept across roster updates
	protected bool m_bAdmin;
	protected int m_iSelectionClearAt;   // tick count; 0 = keep
	protected string m_sFeedback;
	protected int m_iFeedbackUntil;
	protected int m_iNextRosterAsk;
	protected int m_iSwallowUntil; // tick count until which closed-menu key releases are swallowed

	// Input self-check, logged once ~5 s after the player controller starts updating.
	protected bool m_bDiagnosed;
	protected int m_iDiagnoseAt;

	protected void Diagnose(InputManager input, SCR_PlayerController pc)
	{
		m_bDiagnosed = true;
		int found;
		string names = "";
		for (int i = 0; i < input.GetActionCount(); i++)
		{
			string action = input.GetActionName(i);
			if (action.StartsWith("AIO_"))
			{
				found++;
				names += action + " ";
			}
		}
		bool activated = input.ActivateContext("AIO_OpenContext");
		bool active = input.IsContextActive("AIO_OpenContext");
		Print(string.Format("[AIO] INPUT check: %1 of %2 actions are ours (%3); open context activate=%4 active=%5; can use menu=%6",
			found, input.GetActionCount(), names, activated, active, CanUse(pc)));
		if (found == 0)
			Print("[AIO] INPUT check: our key config was NOT loaded (Configs/System/chimeraInputCommon.conf override)");
	}

	// Widgets
	protected ImageWidget m_wBack;
	protected ref array<TextWidget> m_aLines = {};
	protected TextWidget m_wFeedback;
	protected ref array<Widget> m_aWidgets = {};

	static AIO_Menu Get()
	{
		if (!s_Instance)
			s_Instance = new AIO_Menu();
		return s_Instance;
	}

	void AIO_Menu()
	{
		m_Root = AIO_MenuTree.Build();
		m_iDiagnoseAt = System.GetTickCount() + 5000;
	}

	bool IsOpen()
	{
		return m_bOpen;
	}

	// ------------------------------------------------------------------ frame

	void Update(SCR_PlayerController pc)
	{
		InputManager input = GetGame().GetInputManager();
		if (!input)
			return;
		int now = System.GetTickCount();

		if (m_iSelectionClearAt > 0 && now > m_iSelectionClearAt && !m_bOpen)
		{
			m_aSelected.Clear();
			m_iSelectionClearAt = 0;
		}

		if (!m_bDiagnosed && now > m_iDiagnoseAt)
			Diagnose(input, pc);

		bool usable = CanUse(pc);
		if (!usable)
		{
			if (m_bOpen)
				Close();
			UpdateFeedback(now);
			AIO_SquadBar.Get().Hide();
			return;
		}
		// The squad bar needs the roster too: every 2 s while closed (1 s while open, below).
		if (!m_bOpen && now >= m_iNextRosterAsk)
		{
			m_iNextRosterAsk = now + 2000;
			pc.AIO_AskRoster();
		}
		AIO_SquadBar.Get().Update(now, m_aMembers, m_aSelected);

		if (!m_bOpen)
		{
			// Just closed: the key that closed the menu is still being released. Keep our
			// context on so that release can't reach the weapon/quick-slot keys.
			if (now < m_iSwallowUntil)
			{
				input.ActivateContext("AIO_MenuContext");
				UpdateFeedback(now);
				return;
			}
			input.ActivateContext("AIO_OpenContext");
			if (input.GetActionTriggered("AIO_Menu"))
				Open(pc);
			UpdateFeedback(now);
			return;
		}

		// Open: our context outranks the character's, so 0-9 and Backspace reach only the menu.
		input.ActivateContext("AIO_MenuContext");
		if (now >= m_iNextRosterAsk)
		{
			m_iNextRosterAsk = now + 1000;
			pc.AIO_AskRoster();
		}

		if (input.GetActionTriggered("AIO_Menu"))
		{
			Close();
			return;
		}
		if (input.GetActionTriggered("AIO_Back"))
		{
			OnBack();
		}
		else
		{
			for (int key = 0; key <= 9; key++)
			{
				if (input.GetActionTriggered("AIO_Key" + key.ToString()))
				{
					OnKey(pc, key);
					break;
				}
			}
		}
		if (m_bOpen)
			Render();
		UpdateFeedback(now);
	}

	protected bool CanUse(SCR_PlayerController pc)
	{
		if (!pc)
			return false;
		ChimeraCharacter character = ChimeraCharacter.Cast(pc.GetControlledEntity());
		if (!character)
			return false;
		CharacterControllerComponent controller = character.GetCharacterController();
		if (controller && controller.IsDead())
			return false;
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (mapEntity && mapEntity.IsOpen())
			return false;
		return true;
	}

	// ------------------------------------------------------------------ open / close

	protected void Open(SCR_PlayerController pc)
	{
		m_bOpen = true;
		m_eMode = AIO_EMenuMode.ORDERS;
		m_aStack.Clear();
		m_aStack.Insert(m_Root);
		m_iSelectPage = 0;
		m_iSelectionClearAt = 0;
		m_iNextRosterAsk = 0;
		pc.AIO_AskRoster();
		Render();
	}

	void Close()
	{
		m_bOpen = false;
		m_iSwallowUntil = System.GetTickCount() + SWALLOW_MS;
		SetVisible(false);
		if (!m_aSelected.IsEmpty() && m_iSelectionClearAt == 0)
			m_iSelectionClearAt = System.GetTickCount() + SELECTION_KEEP_MS;
	}

	// ------------------------------------------------------------------ keys

	protected void OnBack()
	{
		if (m_eMode == AIO_EMenuMode.SELECT)
		{
			m_aSelected.Clear(); // Backspace on the select page = whole squad again
			return;
		}
		if (m_eMode == AIO_EMenuMode.TEAMS)
		{
			m_eMode = AIO_EMenuMode.SELECT;
			return;
		}
		if (m_aStack.Count() > 1)
			m_aStack.Remove(m_aStack.Count() - 1);
		else
			Close();
	}

	protected void OnKey(SCR_PlayerController pc, int key)
	{
		if (m_eMode == AIO_EMenuMode.SELECT)
		{
			OnSelectKey(key);
			return;
		}
		if (m_eMode == AIO_EMenuMode.TEAMS)
		{
			OnTeamKey(key);
			return;
		}

		AIO_MenuItem page = m_aStack[m_aStack.Count() - 1];
		bool onRoot = m_aStack.Count() == 1;
		if (key == 0)
		{
			if (onRoot)
			{
				m_eMode = AIO_EMenuMode.SELECT;
				m_iSelectPage = 0;
			}
			else
			{
				m_aStack.Remove(m_aStack.Count() - 1);
			}
			return;
		}

		array<AIO_MenuItem> items = {};
		VisibleItems(page, items);
		int index = key - 1;
		if (index < 0 || index >= items.Count())
			return;
		AIO_MenuItem item = items[index];
		if (item.IsPage())
		{
			m_aStack.Insert(item);
			return;
		}
		Give(pc, item);
	}

	protected void VisibleItems(AIO_MenuItem page, notnull array<AIO_MenuItem> items)
	{
		foreach (AIO_MenuItem item : page.m_aChildren)
		{
			if (item.m_bAdmin && !m_bAdmin)
				continue;
			if (item.IsPage() && !HasVisibleChild(item))
				continue;
			items.Insert(item);
			if (items.Count() == 9)
				break;
		}
	}

	protected bool HasVisibleChild(AIO_MenuItem page)
	{
		foreach (AIO_MenuItem child : page.m_aChildren)
		{
			if (child.m_bAdmin && !m_bAdmin)
				continue;
			if (child.IsPage() && !HasVisibleChild(child))
				continue;
			return true;
		}
		return false;
	}

	protected void Give(SCR_PlayerController pc, AIO_MenuItem item)
	{
		vector point = vector.Zero;
		RplId target = RplId.Invalid();
		if (item.m_eTarget == AIO_ETarget.POINT)
		{
			IEntity hitEntity;
			if (!LookPoint(pc, point, hitEntity))
			{
				ShowFeedback("Look at a spot first");
				return;
			}
			// The ray may hit a part (a weapon on a body, a wheel): use the nearest replicated parent.
			while (hitEntity && !hitEntity.FindComponent(RplComponent))
				hitEntity = hitEntity.GetParent();
			if (hitEntity)
			{
				RplComponent rpl = RplComponent.Cast(hitEntity.FindComponent(RplComponent));
				target = rpl.Id();
			}
		}
		if (item.m_eOrder == AIO_EOrder.TOGGLE_SQUAD_BAR)
		{
			bool shown = AIO_SquadBar.Get().Toggle();
			if (shown)
				ShowFeedback("Squad bar on");
			else
				ShowFeedback("Squad bar off");
			Close();
			return;
		}
		array<RplId> members = {};
		members.Copy(m_aSelected);
		pc.AIO_SendOrder(item.m_eOrder, members, point, target);
		Close();
		if (!m_aSelected.IsEmpty())
			m_iSelectionClearAt = System.GetTickCount() + SELECTION_KEEP_MS;
	}

	protected void OnSelectKey(int key)
	{
		if (key == 0)
		{
			m_eMode = AIO_EMenuMode.ORDERS;
			return;
		}
		if (key == 9)
		{
			m_eMode = AIO_EMenuMode.TEAMS;
			return;
		}
		if (key == 8)
		{
			int pages = PageCount();
			m_iSelectPage++;
			if (m_iSelectPage >= pages)
				m_iSelectPage = 0;
			return;
		}
		int index = m_iSelectPage * MEN_PER_PAGE + key - 1;
		if (index < 0 || index >= m_aMembers.Count())
			return;
		RplId id = m_aMembers[index].m_Id;
		int at = m_aSelected.Find(id);
		if (at >= 0)
			m_aSelected.Remove(at);
		else
			m_aSelected.Insert(id);
	}

	protected void OnTeamKey(int key)
	{
		if (key == 0)
		{
			m_eMode = AIO_EMenuMode.SELECT;
			return;
		}
		if (key >= 1 && key <= 4)
		{
			// Select that team (replaces the selection).
			m_aSelected.Clear();
			foreach (AIO_Member member : m_aMembers)
			{
				if (member.m_iTeam == key)
					m_aSelected.Insert(member.m_Id);
			}
			if (m_aSelected.IsEmpty())
				ShowFeedback(TeamName(key) + " team is empty");
			m_eMode = AIO_EMenuMode.ORDERS;
			return;
		}
		if (key >= 5 && key <= 9)
		{
			int team = TEAM_WHITE;
			if (key <= 8)
				team = key - 4;
			if (m_aSelected.IsEmpty())
			{
				ShowFeedback("Select men first");
				return;
			}
			foreach (RplId id : m_aSelected)
				m_mTeams.Set(id, team);
			foreach (AIO_Member assigned : m_aMembers)
			{
				if (m_aSelected.Contains(assigned.m_Id))
					assigned.m_iTeam = team;
			}
			ShowFeedback(string.Format("%1 men -> %2", m_aSelected.Count(), TeamName(team)));
		}
	}

	protected int PageCount()
	{
		int count = m_aMembers.Count();
		int pages = (count + MEN_PER_PAGE - 1) / MEN_PER_PAGE;
		if (pages < 1)
			pages = 1;
		return pages;
	}

	// ------------------------------------------------------------------ server data

	void SetRoster(array<RplId> ids, array<string> names, array<int> health, array<string> tasks, array<int> ammo, bool admin)
	{
		m_bAdmin = admin;
		m_aMembers.Clear();
		for (int i = 0; i < ids.Count(); i++)
		{
			AIO_Member member = new AIO_Member();
			member.m_Id = ids[i];
			member.m_sName = WidgetManager.Translate(names[i]);
			member.m_iHealth = health[i];
			if (tasks && i < tasks.Count())
				member.m_sTask = tasks[i];
			if (ammo && i < ammo.Count())
				member.m_iAmmo = ammo[i];
			int team;
			if (m_mTeams.Find(ids[i], team))
				member.m_iTeam = team;
			m_aMembers.Insert(member);
		}
		// Drop selections of men who are gone.
		for (int s = m_aSelected.Count() - 1; s >= 0; s--)
		{
			if (!ids.Contains(m_aSelected[s]))
				m_aSelected.Remove(s);
		}
		if (m_iSelectPage >= PageCount())
			m_iSelectPage = 0;
	}

	// Server messages carry names as localisation keys (#AR-Surname_...): translate those words.
	static string TranslateWords(string text)
	{
		if (!text.Contains("#"))
			return text;
		array<string> words = {};
		text.Split(" ", words, false);
		string result = "";
		foreach (string word : words)
		{
			string tail = "";
			if (word.StartsWith("#"))
			{
				// keep trailing punctuation outside the key ("#AR-Surname_EN_Smith," -> "Smith,")
				while (word.Length() > 1 && (word.EndsWith(",") || word.EndsWith(".") || word.EndsWith(":")))
				{
					tail = word.Substring(word.Length() - 1, 1) + tail;
					word = word.Substring(0, word.Length() - 1);
				}
				word = WidgetManager.Translate(word);
			}
			if (!result.IsEmpty())
				result += " ";
			result += word + tail;
		}
		return result;
	}

	void ShowFeedback(string text)
	{
		text = TranslateWords(text);
		m_sFeedback = text;
		m_iFeedbackUntil = System.GetTickCount() + FEEDBACK_MS;
		if (!m_wFeedback)
			CreateWidgets();
		if (m_wFeedback)
		{
			m_wFeedback.SetText(text);
			m_wFeedback.SetVisible(true);
		}
	}

	protected void UpdateFeedback(int now)
	{
		if (m_wFeedback && m_wFeedback.IsVisible() && now > m_iFeedbackUntil)
			m_wFeedback.SetVisible(false);
	}

	// ------------------------------------------------------------------ look point

	// Where the player is looking (camera ray, 1.5 km), and what entity is there if any.
	static bool LookPoint(SCR_PlayerController pc, out vector point, out IEntity hitEntity)
	{
		CameraManager cameras = GetGame().GetCameraManager();
		if (!cameras)
			return false;
		CameraBase camera = cameras.CurrentCamera();
		if (!camera)
			return false;
		vector transform[4];
		camera.GetTransform(transform);
		TraceParam trace = new TraceParam();
		trace.Start = transform[3];
		trace.End = transform[3] + transform[2] * 1500;
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.Exclude = pc.GetControlledEntity();
		float fraction = GetGame().GetWorld().TraceMove(trace, null);
		if (fraction >= 1)
			return false;
		point = trace.Start + (trace.End - trace.Start) * fraction;
		hitEntity = trace.TraceEnt;
		return true;
	}

	// ------------------------------------------------------------------ drawing

	static string TeamName(int team)
	{
		switch (team)
		{
			case TEAM_RED: return "Red";
			case TEAM_GREEN: return "Green";
			case TEAM_BLUE: return "Blue";
			case TEAM_YELLOW: return "Yellow";
		}
		return "White";
	}

	static int TeamColor(int team)
	{
		switch (team)
		{
			case TEAM_RED: return ARGB(255, 255, 80, 80);
			case TEAM_GREEN: return ARGB(255, 96, 224, 96);
			case TEAM_BLUE: return ARGB(255, 96, 160, 255);
			case TEAM_YELLOW: return ARGB(255, 255, 224, 80);
		}
		return ARGB(255, 255, 255, 255);
	}

	protected string TargetText()
	{
		if (m_aSelected.IsEmpty())
			return "Whole squad";
		string text = "";
		foreach (AIO_Member member : m_aMembers)
		{
			if (!m_aSelected.Contains(member.m_Id))
				continue;
			if (!text.IsEmpty())
				text += ", ";
			text += member.m_sName;
		}
		if (text.IsEmpty())
			return string.Format("%1 men", m_aSelected.Count());
		return text;
	}

	protected void Render()
	{
		if (!m_wBack)
			CreateWidgets();
		if (!m_wBack)
			return;

		array<string> texts = {};
		array<int> colors = {};
		int white = ARGB(255, 255, 255, 255);
		int dim = ARGB(255, 160, 160, 160);
		int head = ARGB(255, 255, 210, 127);

		if (m_eMode == AIO_EMenuMode.ORDERS)
		{
			AIO_MenuItem page = m_aStack[m_aStack.Count() - 1];
			string title = "Orders";
			if (m_aStack.Count() > 1)
				title = page.m_sLabel;
			texts.Insert(title + "  ->  " + TargetText());
			colors.Insert(head);
			array<AIO_MenuItem> items = {};
			VisibleItems(page, items);
			for (int i = 0; i < items.Count(); i++)
			{
				string label = items[i].m_sLabel;
				if (items[i].IsPage())
					label += "  >";
				texts.Insert(string.Format("%1   %2", i + 1, label));
				colors.Insert(white);
			}
			if (m_aStack.Count() == 1)
				texts.Insert("0   Select members  >");
			else
				texts.Insert("0   Back");
			colors.Insert(dim);
		}
		else if (m_eMode == AIO_EMenuMode.SELECT)
		{
			texts.Insert(string.Format("Select members (page %1/%2)", m_iSelectPage + 1, PageCount()));
			colors.Insert(head);
			if (m_aMembers.IsEmpty())
			{
				texts.Insert("    No soldiers under your command");
				colors.Insert(dim);
			}
			int first = m_iSelectPage * MEN_PER_PAGE;
			for (int m = 0; m < MEN_PER_PAGE; m++)
			{
				int index = first + m;
				if (index >= m_aMembers.Count())
					break;
				AIO_Member member = m_aMembers[index];
				string mark = "[  ]";
				if (m_aSelected.Contains(member.m_Id))
					mark = "[x]";
				texts.Insert(string.Format("%1   %2 %3   %4%", m + 1, mark, member.m_sName, member.m_iHealth));
				colors.Insert(TeamColor(member.m_iTeam));
			}
			texts.Insert("8   Next page");
			colors.Insert(dim);
			texts.Insert("9   Teams  >");
			colors.Insert(white);
			texts.Insert("0   Done        Backspace: clear (whole squad)");
			colors.Insert(dim);
		}
		else
		{
			texts.Insert("Teams  ->  " + TargetText());
			colors.Insert(head);
			for (int t = 1; t <= 4; t++)
			{
				texts.Insert(string.Format("%1   Select %2", t, TeamName(t)));
				colors.Insert(TeamColor(t));
			}
			for (int a = 1; a <= 4; a++)
			{
				texts.Insert(string.Format("%1   Assign selected -> %2", a + 4, TeamName(a)));
				colors.Insert(TeamColor(a));
			}
			texts.Insert("9   Unassign (White)");
			colors.Insert(white);
			texts.Insert("0   Back");
			colors.Insert(dim);
		}
		Draw(texts, colors);
	}

	protected void CreateWidgets()
	{
		WorkspaceWidget ws = GetGame().GetWorkspace();
		if (!ws)
			return;
		m_wBack = ImageWidget.Cast(ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.BLEND, Color.FromRGBA(16, 20, 24, 176), 0, ws));
		if (m_wBack)
		{
			FrameSlot.SetPos(m_wBack, 30, 180);
			FrameSlot.SetSize(m_wBack, 460, 100);
			m_wBack.SetZOrder(900);
			m_aWidgets.Insert(m_wBack);
		}
		m_wFeedback = TextWidget.Cast(ws.CreateWidgets("{B537936D5B7E2BA1}UI/layouts/WidgetLibrary/TextWidgets/Text_BodySmall.layout", ws));
		if (m_wFeedback)
		{
			m_wFeedback.SetFlags(WidgetFlags.IGNORE_CURSOR);
			FrameSlot.SetPos(m_wFeedback, 40, 150);
			FrameSlot.SetSize(m_wFeedback, 600, 24);
			m_wFeedback.SetZOrder(902);
			m_wFeedback.SetColorInt(ARGB(255, 255, 210, 127));
			m_wFeedback.SetVisible(false);
			m_aWidgets.Insert(m_wFeedback);
		}
		SetVisible(false);
	}

	protected TextWidget Line(int index)
	{
		WorkspaceWidget ws = GetGame().GetWorkspace();
		while (m_aLines.Count() <= index)
		{
			TextWidget text = TextWidget.Cast(ws.CreateWidgets("{B537936D5B7E2BA1}UI/layouts/WidgetLibrary/TextWidgets/Text_BodySmall.layout", ws));
			if (!text)
				return null;
			text.SetFlags(WidgetFlags.IGNORE_CURSOR);
			FrameSlot.SetSize(text, 440, 24);
			text.SetZOrder(901);
			m_aLines.Insert(text);
			m_aWidgets.Insert(text);
		}
		return m_aLines[index];
	}

	protected void Draw(array<string> texts, array<int> colors)
	{
		int lineHeight = 26;
		for (int i = 0; i < texts.Count(); i++)
		{
			TextWidget line = Line(i);
			if (!line)
				continue;
			FrameSlot.SetPos(line, 42, 188 + i * lineHeight);
			line.SetText(texts[i]);
			line.SetColorInt(colors[i]);
			line.SetVisible(true);
		}
		for (int j = texts.Count(); j < m_aLines.Count(); j++)
			m_aLines[j].SetVisible(false);
		FrameSlot.SetSize(m_wBack, 460, texts.Count() * lineHeight + 16);
		m_wBack.SetVisible(true);
	}

	protected void SetVisible(bool visible)
	{
		if (m_wBack)
			m_wBack.SetVisible(visible);
		foreach (TextWidget line : m_aLines)
			line.SetVisible(visible && line.IsVisible());
	}

	// The workspace outlives the mission: remove our widgets when the world goes away.
	static void Cleanup()
	{
		AIO_SquadBar.Cleanup();
		if (!s_Instance)
			return;
		foreach (Widget widget : s_Instance.m_aWidgets)
		{
			if (widget)
				widget.RemoveFromHierarchy();
		}
		s_Instance = null;
	}
}
