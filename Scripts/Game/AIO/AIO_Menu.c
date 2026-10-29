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
	protected ref array<string> m_aFeedbackQueue = {};
	protected ref array<vector> m_aRoutePoints = {};
	protected ref array<RplId> m_aRouteMembers = {}; // frozen when the first point is added
	protected string m_sRouteTarget;
	protected int m_iNextRosterAsk;
	protected int m_iSwallowUntil; // tick count until which closed-menu key releases are swallowed

	// Mouse control (old Arma style, More > Mouse control): wheel moves a highlight over the lines,
	// left click gives it, right click goes back. Not with the map open (the map needs the mouse).
	protected ref array<int> m_aLineKeys = {}; // the key each drawn line stands for; -1 = not selectable
	protected int m_iHighlight;               // index into the selectable lines
	protected string m_sHighlightPage;        // which page the highlight belongs to (reset on change)
	protected int m_iNextWheel;
	protected ImageWidget m_wHighlight;

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
	protected TextWidget m_wStatus;
	protected string m_sOrderStatus;
	protected int m_iRosterReceivedAt;
	protected int m_iNextStatusDraw;
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
		AIO_Settings.Load();
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
			AIO_AimMarker.Get().Hide();
			if (m_wStatus) m_wStatus.SetVisible(false);
			return;
		}
		// Keep owner feedback current even while the command menu is closed.
		if (now >= m_iNextRosterAsk)
		{
			m_iNextRosterAsk = now + 1000;
			pc.AIO_AskRoster();
		}
		UpdateStatus(now);
		// The squad bar and the markers over the men show only while the menu is open.
		if (m_bOpen)
			AIO_SquadBar.Get().Update(now, m_aMembers, m_aSelected);
		else
			AIO_SquadBar.Get().Hide();

		if (!m_bOpen)
		{
			// Just closed: the key that closed the menu is still being released. Keep our
			// context on so that release can't reach the weapon/quick-slot keys.
			// A click that gave an order must not reach the weapon while the button is still down.
			if (MouseActive() && input.GetActionValue("AIO_MouseHeld") > 0)
				m_iSwallowUntil = Math.Max(m_iSwallowUntil, now + 100);
			if (now < m_iSwallowUntil)
			{
				ActivateMenuContext(input);
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
		ActivateMenuContext(input);
		if (input.GetActionTriggered("AIO_Menu"))
		{
			Close();
			return;
		}
		bool mouse = MouseActive();
		if (input.GetActionTriggered("AIO_Back") || (mouse && input.GetActionTriggered("AIO_MouseBack")))
		{
			OnBack();
		}
		else if (mouse && input.GetActionTriggered("AIO_MouseSelect"))
		{
			int clicked = HighlightedKey();
			if (clicked >= 0)
				OnKey(pc, clicked);
		}
		else
		{
			if (mouse)
				Wheel(input, now);
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
		UpdateAim(pc);
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
		return true;
	}

	protected void ActivateMenuContext(InputManager input)
	{
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (mapEntity && mapEntity.IsOpen())
			input.ActivateContext("AIO_MapMenuContext");
		else
			input.ActivateContext("AIO_MenuContext");
		// Takes the mouse buttons and wheel from the weapon while the menu is open.
		if (MouseActive())
			input.ActivateContext("AIO_MouseMenuContext");
	}

	static bool MouseActive()
	{
		if (!AIO_Settings.s_bMouse)
			return false;
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		return !mapEntity || !mapEntity.IsOpen();
	}

	// One highlight step per wheel notch (a short gap so one notch read over two frames counts once).
	protected void Wheel(InputManager input, int now)
	{
		float value = input.GetActionValue("AIO_MouseWheel");
		if (value == 0 || now < m_iNextWheel)
			return;
		m_iNextWheel = now + 60;
		int count = SelectableCount();
		if (count == 0)
			return;
		if (value > 0)
			m_iHighlight = (m_iHighlight + count - 1) % count;
		else
			m_iHighlight = (m_iHighlight + 1) % count;
	}

	protected int SelectableCount()
	{
		int count;
		foreach (int key : m_aLineKeys)
		{
			if (key >= 0)
				count++;
		}
		return count;
	}

	// The key the highlighted line stands for, or -1.
	protected int HighlightedKey()
	{
		int index;
		foreach (int key : m_aLineKeys)
		{
			if (key < 0)
				continue;
			if (index == m_iHighlight)
				return key;
			index++;
		}
		return -1;
	}

	// The order under the highlight on an Orders page (null for pages, Back and other modes).
	protected AIO_MenuItem HighlightedItem()
	{
		if (m_eMode != AIO_EMenuMode.ORDERS)
			return null;
		int key = HighlightedKey();
		if (key <= 0)
			return null;
		array<AIO_MenuItem> items = {};
		VisibleItems(m_aStack[m_aStack.Count() - 1], items);
		if (key > items.Count() || items[key - 1].IsPage())
			return null;
		return items[key - 1];
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
		AIO_AimMarker.Get().Hide();
		if (!m_aSelected.IsEmpty() && m_iSelectionClearAt == 0)
			m_iSelectionClearAt = System.GetTickCount() + SELECTION_KEEP_MS;
	}

	// The aim preview follows the page shown (Orders pages only; Select/Teams have no aimed orders).
	protected void UpdateAim(SCR_PlayerController pc)
	{
		if (!m_bOpen || m_eMode != AIO_EMenuMode.ORDERS)
		{
			AIO_AimMarker.Get().Hide();
			return;
		}
		AIO_MenuItem focus = null;
		if (MouseActive())
			focus = HighlightedItem();
		AIO_AimMarker.Get().Update(pc, m_aStack[m_aStack.Count() - 1], m_aStack.Count() == 1, focus);
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
			OnTeamKey(pc, key);
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
		if (item.m_eOrder == AIO_EOrder.ROUTE_CLEAR)
		{
			m_aRoutePoints.Clear();
			m_aRouteMembers.Clear();
			m_sRouteTarget = "";
			ShowFeedback("Draft route cleared");
			return;
		}
		if (item.m_eOrder == AIO_EOrder.ROUTE_START || item.m_eOrder == AIO_EOrder.ROUTE_PATROL)
		{
			if (m_aRoutePoints.IsEmpty() || (item.m_eOrder == AIO_EOrder.ROUTE_PATROL && m_aRoutePoints.Count() < 2))
			{
				ShowFeedback("Add waypoints first (patrol needs at least two)");
				return;
			}
			pc.AIO_SendRoute(m_aRouteMembers, m_aRoutePoints, item.m_eOrder == AIO_EOrder.ROUTE_PATROL);
			Close();
			return; // retain the draft so a rejected start can be corrected
		}
		vector point = vector.Zero;
		RplId target = RplId.Invalid();
		if (item.m_eTarget == AIO_ETarget.MAP_POINT)
		{
			SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
			if (!mapEntity || !mapEntity.IsOpen() || !mapEntity.GetMapWidget())
			{
				ShowFeedback("Open the map and place your cursor first");
				return;
			}
			float x, z;
			mapEntity.GetMapCursorWorldPosition(x, z);
			point = Vector(x, GetGame().GetWorld().GetSurfaceY(x, z), z);
		}
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
		if (item.m_eOrder == AIO_EOrder.ROUTE_ADD)
		{
			if (m_aRoutePoints.Count() >= AIO_Routes.MAX_POINTS)
			{
				ShowFeedback("Route limit: 16 waypoints");
				return;
			}
			if (m_aRoutePoints.IsEmpty())
			{
				m_aRouteMembers.Clear();
				m_sRouteTarget = TargetText();
				foreach (AIO_Member member : m_aMembers)
				{
					if (m_aSelected.IsEmpty() || m_aSelected.Contains(member.m_Id))
						m_aRouteMembers.Insert(member.m_Id);
				}
			}
			if (m_aRouteMembers.IsEmpty())
			{
				ShowFeedback("No soldiers selected for route; wait for the squad roster");
				return;
			}
			m_aRoutePoints.Insert(point);
			ShowFeedback(string.Format("Waypoint %1 added for %2 men", m_aRoutePoints.Count(), m_aRouteMembers.Count()));
			return; // keep Routes open while the player moves the map cursor
		}
		if (item.m_eOrder == AIO_EOrder.TOGGLE_SQUAD_BAR)
		{
			bool shown = AIO_SquadBar.Get().Toggle();
			if (shown)
				ShowFeedback("Squad bar on");
			else
				ShowFeedback("Squad bar off");
			AIO_Settings.Save();
			Close();
			return;
		}
		if (item.m_eOrder == AIO_EOrder.MOUSE_CONTROL)
		{
			AIO_Settings.s_bMouse = !AIO_Settings.s_bMouse;
			if (AIO_Settings.s_bMouse)
				ShowFeedback("Mouse control on: wheel to choose, left click to give, right click back");
			else
				ShowFeedback("Mouse control off");
			AIO_Settings.Save();
			return; // keep the menu open to try it
		}
		if (AIO_AimMarker.IsOption(item.m_eOrder))
		{
			ShowFeedback(AIO_AimMarker.Get().Option(item.m_eOrder));
			AIO_Settings.Save();
			return; // keep the page open to see the change
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

	protected void OnTeamKey(SCR_PlayerController pc, int key)
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
			// The server needs teams too (Bound forward / Peel back split by team).
			array<RplId> teamIds = {};
			teamIds.Copy(m_aSelected);
			pc.AIO_SendTeam(teamIds, team);
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
		if (System.GetTickCount() < m_iFeedbackUntil || !m_aFeedbackQueue.IsEmpty())
		{
			if (m_aFeedbackQueue.Count() < 16)
				m_aFeedbackQueue.Insert(text);
			return;
		}
		DisplayFeedback(text);
	}

	protected void DisplayFeedback(string text)
	{
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
		if (now <= m_iFeedbackUntil)
			return;
		if (!m_aFeedbackQueue.IsEmpty())
		{
			string text = m_aFeedbackQueue[0];
			m_aFeedbackQueue.RemoveOrdered(0);
			DisplayFeedback(text);
		}
		else if (m_wFeedback)
			m_wFeedback.SetVisible(false);
	}

	// ------------------------------------------------------------------ look point

	// Where the player is looking (camera ray, 1.5 km), and what entity is there if any.
	static bool LookPoint(SCR_PlayerController pc, out vector point, out IEntity hitEntity)
	{
		vector normal;
		return LookSurface(pc, point, hitEntity, normal);
	}

	// LookPoint plus the normal of the surface hit (straight up on the map).
	static bool LookSurface(SCR_PlayerController pc, out vector point, out IEntity hitEntity, out vector normal)
	{
		normal = Vector(0, 1, 0);
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (mapEntity && mapEntity.IsOpen())
		{
			if (!mapEntity.GetMapWidget())
				return false;
			float x, z;
			mapEntity.GetMapCursorWorldPosition(x, z);
			point = Vector(x, GetGame().GetWorld().GetSurfaceY(x, z), z);
			// Resolve the entity under the map cursor for building/vehicle/supply orders.
			// The destination itself remains the cursor's terrain position.
			vector hitPoint, hitNormal;
			TraceSurface(pc.GetControlledEntity(), point + Vector(0, 500, 0), point - Vector(0, 50, 0), hitPoint, hitEntity, hitNormal);
			return true;
		}
		CameraManager cameras = GetGame().GetCameraManager();
		if (!cameras)
			return false;
		CameraBase camera = cameras.CurrentCamera();
		if (!camera)
			return false;
		vector transform[4];
		camera.GetTransform(transform);
		return TraceSurface(pc.GetControlledEntity(), transform[3], transform[3] + transform[2] * 1500, point, hitEntity, normal);
	}

	static bool TracePoint(IEntity body, vector start, vector end, out vector point, out IEntity hitEntity)
	{
		vector normal;
		return TraceSurface(body, start, end, point, hitEntity, normal);
	}

	static bool TraceSurface(IEntity body, vector start, vector end, out vector point, out IEntity hitEntity, out vector normal)
	{
		TraceParam trace = new TraceParam();
		trace.Start = start;
		trace.End = end;
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		array<IEntity> excluded = {};
		IEntity vehicle = AIO_Vehicles.VehicleOf(body);
		if (vehicle)
			ExcludeTree(vehicle, excluded);
		if (body && !excluded.Contains(body))
			excluded.Insert(body);
		trace.ExcludeArray = excluded;
		float fraction = GetGame().GetWorld().TraceMove(trace, null);
		if (fraction >= 1)
			return false;
		point = trace.Start + (trace.End - trace.Start) * fraction;
		hitEntity = trace.TraceEnt;
		normal = trace.TraceNorm;
		return true;
	}

	// Vehicle colliders can live on child entities (turrets, windows and passengers).
	protected static void ExcludeTree(IEntity entity, notnull array<IEntity> excluded)
	{
		excluded.Insert(entity);
		IEntity child = entity.GetChildren();
		while (child)
		{
			ExcludeTree(child, excluded);
			child = child.GetSibling();
		}
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
		array<int> keys = m_aLineKeys;
		keys.Clear();
		int white = ARGB(255, 255, 255, 255);
		int dim = ARGB(255, 160, 160, 160);
		int head = ARGB(255, 255, 210, 127);

		if (m_eMode == AIO_EMenuMode.ORDERS)
		{
			AIO_MenuItem page = m_aStack[m_aStack.Count() - 1];
			string title = "Orders";
			if (m_aStack.Count() > 1)
				title = page.m_sLabel;
			string targetText = TargetText();
			if (!m_aRoutePoints.IsEmpty() && page.m_aChildren.Count() > 0 && page.m_aChildren[0].m_eOrder == AIO_EOrder.ROUTE_ADD)
				targetText = m_sRouteTarget + " (draft members)";
			texts.Insert(title + "  ->  " + targetText);
			colors.Insert(head);
			keys.Insert(-1);
			array<AIO_MenuItem> items = {};
			VisibleItems(page, items);
			for (int i = 0; i < items.Count(); i++)
			{
				string label = items[i].m_sLabel;
				if (items[i].m_eOrder == AIO_EOrder.ROUTE_ADD)
					label += string.Format(" (%1/16, %2 men)", m_aRoutePoints.Count(), m_aRouteMembers.Count());
				if (items[i].IsPage())
					label += "  >";
				texts.Insert(string.Format("%1   %2", i + 1, label));
				colors.Insert(white);
				keys.Insert(i + 1);
			}
			if (m_aStack.Count() == 1)
				texts.Insert("0   Select members  >");
			else
				texts.Insert("0   Back");
			colors.Insert(dim);
			keys.Insert(0);
		}
		else if (m_eMode == AIO_EMenuMode.SELECT)
		{
			texts.Insert(string.Format("Select members (page %1/%2)", m_iSelectPage + 1, PageCount()));
			colors.Insert(head);
			keys.Insert(-1);
			if (m_aMembers.IsEmpty())
			{
				texts.Insert("    No soldiers under your command");
				colors.Insert(dim);
				keys.Insert(-1);
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
				keys.Insert(m + 1);
			}
			texts.Insert("8   Next page");
			colors.Insert(dim);
			keys.Insert(8);
			texts.Insert("9   Teams  >");
			colors.Insert(white);
			keys.Insert(9);
			texts.Insert("0   Done        Backspace: clear (whole squad)");
			colors.Insert(dim);
			keys.Insert(0);
		}
		else
		{
			texts.Insert("Teams  ->  " + TargetText());
			colors.Insert(head);
			keys.Insert(-1);
			for (int t = 1; t <= 4; t++)
			{
				texts.Insert(string.Format("%1   Select %2", t, TeamName(t)));
				colors.Insert(TeamColor(t));
				keys.Insert(t);
			}
			for (int a = 1; a <= 4; a++)
			{
				texts.Insert(string.Format("%1   Assign selected -> %2", a + 4, TeamName(a)));
				colors.Insert(TeamColor(a));
				keys.Insert(a + 4);
			}
			texts.Insert("9   Unassign (White)");
			colors.Insert(white);
			keys.Insert(9);
			texts.Insert("0   Back");
			colors.Insert(dim);
			keys.Insert(0);
		}
		// A new page starts the highlight on its first line; toggling men keeps it in place.
		string pageId = string.Format("%1/%2/%3", m_eMode, m_aStack.Count(), m_iSelectPage);
		if (m_eMode == AIO_EMenuMode.ORDERS)
			pageId += "/" + m_aStack[m_aStack.Count() - 1].m_sLabel;
		if (pageId != m_sHighlightPage)
		{
			m_sHighlightPage = pageId;
			m_iHighlight = 0;
		}
		m_iHighlight = Math.Clamp(m_iHighlight, 0, Math.Max(SelectableCount() - 1, 0));
		Draw(texts, colors);
	}

	void SetOrderStatus(string status)
	{
		m_sOrderStatus = status;
		m_iRosterReceivedAt = System.GetTickCount();
	}

	// The full squad bar stays menu-only; this small summary persists during play.
	protected void UpdateStatus(int now)
	{
		if (!m_wStatus) CreateWidgets();
		if (!m_wStatus || now < m_iNextStatusDraw) return;
		m_iNextStatusDraw = now + 250;
		if (m_aMembers.IsEmpty())
		{
			m_wStatus.SetVisible(false);
			return;
		}
		string text = "Orders to: " + TargetText();
		if (!m_aSelected.IsEmpty() && !m_bOpen) text += " (selection expires after 8s closed)";
		if (now - m_iRosterReceivedAt < 3500)
		{
			if (!m_sOrderStatus.IsEmpty())
			{
				if (!text.IsEmpty()) text += "\n";
				text += m_sOrderStatus;
			}
		}
		array<string> rows = {};
		text.Split("\n", rows, false);
		int height = rows.Count() * 24 + 8;
		WorkspaceWidget ws = GetGame().GetWorkspace();
		FrameSlot.SetPos(m_wStatus, 40, -150 - height);
		FrameSlot.SetSize(m_wStatus, Math.Min(1100, ws.DPIUnscale(ws.GetWidth()) - 80), height);
		m_wStatus.SetText(text);
		m_wStatus.SetVisible(!text.IsEmpty());
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
		m_wHighlight = ImageWidget.Cast(ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.IGNORE_CURSOR | WidgetFlags.BLEND, Color.FromRGBA(255, 210, 127, 56), 0, ws));
		if (m_wHighlight)
		{
			m_wHighlight.SetZOrder(900);
			m_wHighlight.SetVisible(false);
			m_aWidgets.Insert(m_wHighlight);
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
		m_wStatus = TextWidget.Cast(ws.CreateWidgets("{B537936D5B7E2BA1}UI/layouts/WidgetLibrary/TextWidgets/Text_BodySmall.layout", ws));
		if (m_wStatus)
		{
			m_wStatus.SetFlags(WidgetFlags.IGNORE_CURSOR);
			FrameSlot.SetAnchorMin(m_wStatus, 0, 1);
			FrameSlot.SetAnchorMax(m_wStatus, 0, 1);
			FrameSlot.SetPos(m_wStatus, 40, -460);
			FrameSlot.SetSize(m_wStatus, 1100, 310);
			m_wStatus.SetZOrder(890);
			m_wStatus.SetColorInt(ARGB(255, 255, 210, 127));
			m_wStatus.SetVisible(false);
			m_aWidgets.Insert(m_wStatus);
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
		if (!m_wHighlight)
			return;
		int line = -1;
		int index;
		for (int k = 0; k < m_aLineKeys.Count(); k++)
		{
			if (m_aLineKeys[k] < 0)
				continue;
			if (index == m_iHighlight)
				line = k;
			index++;
		}
		m_wHighlight.SetVisible(MouseActive() && line >= 0);
		if (line >= 0)
		{
			FrameSlot.SetPos(m_wHighlight, 34, 186 + line * lineHeight);
			FrameSlot.SetSize(m_wHighlight, 452, lineHeight);
		}
	}

	protected void SetVisible(bool visible)
	{
		if (m_wBack)
			m_wBack.SetVisible(visible);
		if (m_wHighlight && !visible)
			m_wHighlight.SetVisible(false);
		foreach (TextWidget line : m_aLines)
			line.SetVisible(visible && line.IsVisible());
	}

	// The workspace outlives the mission: remove our widgets when the world goes away.
	static void Cleanup()
	{
		AIO_SquadBar.Cleanup();
		AIO_AimMarker.Cleanup();
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
