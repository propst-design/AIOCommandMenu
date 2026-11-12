// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). Licensed under the GNU GPL v3 or later (see LICENSE).

// Order marker (Jacob, 11 Nov 2026: "very subtle movement order markers such as those that existed in
// prior arma titles"): a faint chevron with a short tag and the distance where the player's squad is
// heading, read from the squad's current vanilla waypoint. So it shows any order source the same way:
// AiO's whole-squad moves, the game's own commanding, a Game Master's waypoints or another mod's AI
// commander. Only the next waypoint, never the chain.
//
// A group's waypoints live only on the server (AIGroup's waypoint list is not replicated), so the
// server reads them once a second and sends the next one to each player on change only (another
// order, a move of 4 m or more, or none left): AIO_OrderMarkers below, AIO_RpcOrderMarker on the
// player controller. Leader and members alike: the squad is the AI group linked to the player's group,
// the player's group itself, or the AI group of a soldier a Game Master possesses.
//
// Client (AIO_OrderMarker): drawn like the aim marker (#3), same colours per kind of order, smaller
// and fainter. Fades out inside 15 m and while aiming down sights; hidden with the map open, with the
// HUD hidden (Game Master editor, pause) and whenever the view isn't the player's own camera (any
// cinematic, intro, spectator or RTS camera). More > Aim & order markers: on/off and opacity, saved.

// Server: who gets which waypoint.
class AIO_OrderMarkers
{
	static const int TICK_MS = 1000;
	static const float MOVED_M = 4.0; // a waypoint that moves less than this (an entity follow) isn't resent

	// What each player was last sent (by player id).
	protected static ref array<int> s_aIds;
	protected static ref array<bool> s_aHas;
	protected static ref array<vector> s_aPos;
	protected static ref array<string> s_aLabels;
	protected static bool s_bRunning;

	// Tests (-aioOrderMarkerTest): a stand-in listener for a squad with no player in it. Its sends are
	// counted and handed straight to the client half instead of travelling as an RPC.
	static const int TEST_ID = -7;
	static SCR_AIGroup s_TestPlayable;
	static SCR_AIGroup s_TestPossessed;
	static int s_iTestSends;

	// Called by AIO_StaticInit at game start (static initializer budget: tools/check_static_init.py).
	static void AIO_InitStatics()
	{
		s_aIds = {};
		s_aHas = {};
		s_aPos = {};
		s_aLabels = {};
	}

	static void Start()
	{
		if (s_bRunning || !Replication.IsServer())
			return;
		s_bRunning = true;
		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
	}

	static void Cleanup()
	{
		GetGame().GetCallqueue().Remove(Tick);
		s_bRunning = false;
		s_aIds.Clear();
		s_aHas.Clear();
		s_aPos.Clear();
		s_aLabels.Clear();
		s_TestPlayable = null;
		s_TestPossessed = null;
		s_iTestSends = 0;
	}

	protected static void Tick()
	{
		array<int> players = {};
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (playerManager)
			playerManager.GetPlayers(players);
		for (int i = s_aIds.Count() - 1; i >= 0; i--)
		{
			if (s_aIds[i] != TEST_ID && !players.Contains(s_aIds[i]))
				Forget(i);
		}
		SCR_GroupsManagerComponent groups = SCR_GroupsManagerComponent.GetInstance();
		foreach (int id : players)
		{
			SCR_AIGroup playerGroup;
			if (groups)
				playerGroup = groups.GetPlayerGroup(id);
			Update(id, OrderGroup(playerGroup, AIO_Server.PossessedGroup(id)));
		}
		if (s_TestPlayable || s_TestPossessed)
			Update(TEST_ID, OrderGroup(s_TestPlayable, s_TestPossessed));
	}

	// The group whose current waypoint is the squad's order: the AI group a possessing player leads,
	// else the AI ("slave") group linked to the player's group, else the player's group itself.
	static SCR_AIGroup OrderGroup(SCR_AIGroup playerGroup, SCR_AIGroup possessed)
	{
		if (possessed && possessed.GetCurrentWaypoint())
			return possessed;
		if (!playerGroup)
			return null;
		SCR_AIGroup slave = playerGroup.GetSlave();
		if (slave && slave.GetCurrentWaypoint())
			return slave;
		if (playerGroup.GetCurrentWaypoint())
			return playerGroup;
		return null;
	}

	// The group's next waypoint: where, and a short tag for it. False when it has none.
	static bool Read(SCR_AIGroup group, out vector position, out string label)
	{
		if (!group)
			return false;
		AIWaypoint waypoint = group.GetCurrentWaypoint();
		if (!waypoint)
			return false;
		position = waypoint.GetOrigin();
		label = Label(waypoint);
		return true;
	}

	// Tag from the waypoint's prefab name (vanilla Prefabs/AI/Waypoints/AIWaypoint_*.et and mods'
	// copies of them), else its class.
	static string Label(AIWaypoint waypoint)
	{
		string name = "";
		EntityPrefabData data = waypoint.GetPrefabData();
		if (data)
			name = data.GetPrefabName();
		name.ToLower();
		if (name.Contains("searchanddestroy") || name.Contains("attack"))
			return "Attack";
		if (name.Contains("suppress"))
			return "Suppress";
		if (name.Contains("artillery"))
			return "Fire mission";
		if (name.Contains("defend") || name.Contains("observationpost"))
			return "Defend";
		if (name.Contains("patrol") || name.Contains("cycle"))
			return "Patrol";
		if (name.Contains("getout"))
			return "Get out";
		if (name.Contains("getin"))
			return "Get in";
		if (name.Contains("follow"))
			return "Follow";
		if (name.Contains("scout"))
			return "Scout";
		if (name.Contains("supplies"))
			return "Supplies";
		if (name.Contains("heal"))
			return "Heal";
		if (name.Contains("capture"))
			return "Capture";
		if (name.Contains("wait") || name.Contains("atease") || name.Contains("loiter"))
			return "Wait";
		if (SCR_DefendWaypoint.Cast(waypoint))
			return "Defend";
		if (SCR_SearchAndDestroyWaypoint.Cast(waypoint))
			return "Attack";
		if (SCR_BoardingWaypoint.Cast(waypoint))
			return "Get in";
		if (AIWaypointCycle.Cast(waypoint))
			return "Patrol";
		return "Move";
	}

	protected static void Update(int id, SCR_AIGroup group)
	{
		vector position;
		string label = "";
		bool has = Read(group, position, label);
		int i = s_aIds.Find(id);
		if (i < 0)
		{
			s_aIds.Insert(id);
			s_aHas.Insert(false);
			s_aPos.Insert(vector.Zero);
			s_aLabels.Insert("");
			i = s_aIds.Count() - 1;
			if (!has)
				return; // a client starts with no marker: nothing to send
		}
		else if (s_aHas[i] == has && (!has || (s_aLabels[i] == label && vector.Distance(s_aPos[i], position) < MOVED_M)))
		{
			return;
		}
		s_aHas[i] = has;
		s_aPos[i] = position;
		s_aLabels[i] = label;
		if (!Send(id, has, position, label))
			Forget(i); // no controller yet: try again next tick
	}

	protected static bool Send(int id, bool has, vector position, string label)
	{
		if (id == TEST_ID)
		{
			s_iTestSends++;
			AIO_OrderMarker.Get().Set(has, position, label);
			return true;
		}
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(id));
		if (!pc)
			return false;
		pc.AIO_SendOrderMarker(has, position, label);
		return true;
	}

	protected static void Forget(int i)
	{
		s_aIds.Remove(i);
		s_aHas.Remove(i);
		s_aPos.Remove(i);
		s_aLabels.Remove(i);
	}
}

// Client: draws the marker the server sent.
class AIO_OrderMarker
{
	static const float LIFT = 1.0;    // metres above the waypoint, so grass and low walls don't bury it
	static const float NEAR_M = 15.0; // fades from here...
	static const float GONE_M = 4.0;  // ...to nothing here
	static const float ADS_FADE = 0.25;
	static const float DOT = 2.0;

	protected static ref AIO_OrderMarker s_Instance;

	protected bool m_bHas;
	protected vector m_vPos;
	protected string m_sLabel;
	protected ref array<ImageWidget> m_aDots = {}; // the chevron, five dots in a V
	protected TextWidget m_wLabel;

	static AIO_OrderMarker Get()
	{
		if (!s_Instance)
			s_Instance = new AIO_OrderMarker();
		return s_Instance;
	}

	void Set(bool has, vector position, string label)
	{
		m_bHas = has;
		m_vPos = position;
		m_sLabel = label;
		if (!has)
			Hide();
	}

	bool Has()
	{
		return m_bHas;
	}

	vector Position()
	{
		return m_vPos;
	}

	string Label()
	{
		return m_sLabel;
	}

	// More > Aim & order markers lines. Returns the feedback line.
	string Option(AIO_EOrder order)
	{
		if (order == AIO_EOrder.ORDER_MARKER)
		{
			AIO_Settings.s_bOrderMarkerOff = !AIO_Settings.s_bOrderMarkerOff;
			if (AIO_Settings.s_bOrderMarkerOff)
			{
				Hide();
				return "Order marker off";
			}
			return "Order marker on";
		}
		if (order == AIO_EOrder.ORDER_MARKER_OPACITY)
		{
			AIO_Settings.s_iOrderMarkerOpacity = (AIO_Settings.s_iOrderMarkerOpacity + 1) % 3;
			if (AIO_Settings.s_iOrderMarkerOpacity == 0)
				return "Order marker: faint";
			if (AIO_Settings.s_iOrderMarkerOpacity == 1)
				return "Order marker: light";
			return "Order marker: clear";
		}
		return "";
	}

	static bool IsOption(AIO_EOrder order)
	{
		return order == AIO_EOrder.ORDER_MARKER || order == AIO_EOrder.ORDER_MARKER_OPACITY;
	}

	protected static int Alpha()
	{
		if (AIO_Settings.s_iOrderMarkerOpacity == 0)
			return 70;
		if (AIO_Settings.s_iOrderMarkerOpacity == 2)
			return 160; // the aim marker's own alpha
		return 110;
	}

	// The aim marker's colour for the same kind of order, at the given alpha.
	protected static int Tint(string label, int alpha)
	{
		if (label == "Attack" || label == "Suppress" || label == "Fire mission")
			return ARGB(alpha, 255, 100, 60);
		if (label == "Defend" || label == "Patrol")
			return ARGB(alpha, 90, 170, 255);
		if (label == "Get in" || label == "Get out")
			return ARGB(alpha, 110, 230, 110);
		if (label == "Supplies" || label == "Heal")
			return ARGB(alpha, 255, 215, 90);
		return ARGB(alpha, 240, 240, 240);
	}

	// Every frame (AIO_Menu.Update). usable: the player is alive and awake.
	void Update(SCR_PlayerController pc, bool usable)
	{
		if (!usable || !m_bHas || AIO_Settings.s_bOrderMarkerOff || ViewHidden())
		{
			Hide();
			return;
		}
		IEntity player = pc.GetControlledEntity();
		if (!player)
		{
			Hide();
			return;
		}
		float distance = vector.Distance(player.GetOrigin(), m_vPos);
		float fade = Math.Clamp((distance - GONE_M) / (NEAR_M - GONE_M), 0, 1);
		if (Aiming(player))
			fade = fade * ADS_FADE;
		int alpha = Math.Round(Alpha() * fade);
		if (alpha < 8)
		{
			Hide();
			return;
		}
		vector centre;
		if (!Project(m_vPos + Vector(0, LIFT, 0), centre) || !EnsureWidgets())
		{
			Hide();
			return;
		}
		int color = Tint(m_sLabel, alpha);
		// A small V pointing down at the spot, a fixed size on screen like Arma's own move markers.
		for (int i = 0; i < 5; i++)
		{
			float x = (i - 2) * 3.0;
			float y = Math.AbsFloat(i - 2) * -3.0;
			ImageWidget dot = m_aDots[i];
			FrameSlot.SetPos(dot, centre[0] + x - DOT / 2, centre[1] + y - DOT / 2);
			FrameSlot.SetSize(dot, DOT, DOT);
			dot.SetColorInt(color);
			dot.SetVisible(true);
		}
		FrameSlot.SetPos(m_wLabel, centre[0] + 10, centre[1] - 14);
		m_wLabel.SetText(string.Format("%1  %2 m", m_sLabel, Math.Round(distance)));
		m_wLabel.SetColorInt(color);
		m_wLabel.SetVisible(true);
	}

	// Map open, HUD hidden, or a camera that isn't the player's own (intro, cinematic, spectator, RTS).
	protected static bool ViewHidden()
	{
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (mapEntity && mapEntity.IsOpen())
			return true;
		CameraManager cameras = GetGame().GetCameraManager();
		if (!cameras || !PlayerCamera.Cast(cameras.CurrentCamera()))
			return true;
		SCR_HUDManagerComponent hud = SCR_HUDManagerComponent.GetHUDManager();
		if (hud)
		{
			Widget root = hud.GetHUDRootWidget();
			if (root && !root.IsVisible())
				return true;
		}
		return false;
	}

	protected static bool Aiming(IEntity player)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(player);
		if (!character)
			return false;
		CharacterControllerComponent controller = character.GetCharacterController();
		return controller && controller.IsWeaponADS();
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

	protected bool EnsureWidgets()
	{
		if (m_wLabel)
			return true;
		WorkspaceWidget ws = GetGame().GetWorkspace();
		if (!ws)
			return false;
		for (int i = m_aDots.Count(); i < 5; i++) // a half-made set (no label) is finished, not doubled
		{
			ImageWidget dot = ImageWidget.Cast(ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.IGNORE_CURSOR | WidgetFlags.BLEND, Color.FromRGBA(255, 255, 255, 110), 0, ws));
			if (!dot)
				return false;
			dot.SetZOrder(855); // under the aim marker (860)
			dot.SetVisible(false);
			m_aDots.Insert(dot);
		}
		m_wLabel = TextWidget.Cast(ws.CreateWidgets("{B537936D5B7E2BA1}UI/layouts/WidgetLibrary/TextWidgets/Text_BodySmall.layout", ws));
		if (!m_wLabel)
			return false;
		m_wLabel.SetFlags(WidgetFlags.IGNORE_CURSOR);
		m_wLabel.SetZOrder(855);
		FrameSlot.SetSize(m_wLabel, 240, 24);
		m_wLabel.SetVisible(false);
		return true;
	}

	void Hide()
	{
		foreach (ImageWidget dot : m_aDots)
			dot.SetVisible(false);
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
		if (s_Instance.m_wLabel)
			s_Instance.m_wLabel.RemoveFromHierarchy();
		s_Instance = null;
	}
}
