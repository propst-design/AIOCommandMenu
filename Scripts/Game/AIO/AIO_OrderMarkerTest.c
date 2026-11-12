// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). Licensed under the GNU GPL v3 or later (see LICENSE).

// -aioOrderMarkerTest 1: the order marker's server side (AIO_OrderMarkers). A fire team linked as the
// AI group of a playable group, like a player's squad, stands in for a player (AIO_OrderMarkers.TEST_ID):
// its sends are counted and handed to the client half directly. Checks: nothing is sent with no order;
// a new waypoint reaches the client half with its tag and position; nothing is resent while it doesn't
// change; a different order replaces it; completion clears it; the possessed / slave / own-group
// choice; tags from vanilla waypoint prefabs. The RPC hop to a real client and the look are checked live.
class AIO_OrderMarkerTest
{
	static const ResourceName TEAM = "{84E5BBAB25EA23E5}Prefabs/Groups/BLUFOR/Group_US_FireTeam.et";
	static const ResourceName WP_DEFEND = "{93291E72AC23930F}Prefabs/AI/Waypoints/AIWaypoint_Defend.et";
	static const ResourceName WP_SND = "{B3E7B8DC2BAB8ACC}Prefabs/AI/Waypoints/AIWaypoint_SearchAndDestroy.et";
	static const ResourceName WP_GETOUT = "{C40316EE26846CAB}Prefabs/AI/Waypoints/AIWaypoint_GetOut.et";
	static const ResourceName WP_GETIN = "{712F4795CF8B91C7}Prefabs/AI/Waypoints/AIWaypoint_GetIn.et";

	protected static SCR_AIGroup s_Master, s_Squad;
	protected static vector s_vBase, s_vTarget;
	protected static int s_iSent, s_iPass, s_iFail;

	static void Check(string name, bool ok, string detail)
	{
		string verdict = "FAIL";
		if (ok)
		{
			verdict = "PASS";
			s_iPass++;
		}
		else
		{
			s_iFail++;
		}
		Print(string.Format("[AIO] TEST %1 %2 %3", name, verdict, detail));
	}

	protected static void Finish()
	{
		AIO_OrderMarkers.s_TestPlayable = null;
		AIO_OrderMarkers.s_TestPossessed = null;
		Print(string.Format("[AIO] TEST RESULT %1 passed, %2 failed", s_iPass, s_iFail));
		AIO_TestVerdict.Report(s_iPass, s_iFail);
		Print("[AIO] TEST COMPLETE");
	}

	protected static string State()
	{
		AIO_OrderMarker marker = AIO_OrderMarker.Get();
		return string.Format("sends=%1 has=%2 label=%3 pos=%4", AIO_OrderMarkers.s_iTestSends, marker.Has(), marker.Label(), marker.Position());
	}

	// A dry, fairly flat spot near the middle of the map (as AIO_SquadTradeTest).
	protected static vector Spot(vector near)
	{
		BaseWorld world = GetGame().GetWorld();
		float sea = world.GetOceanBaseHeight();
		for (int ring = 0; ring < 40; ring++)
		{
			for (int k = 0; k < 8; k++)
			{
				float angle = 0.785 * k;
				float x = near[0] + Math.Cos(angle) * ring * 25;
				float z = near[2] + Math.Sin(angle) * ring * 25;
				float y = world.GetSurfaceY(x, z);
				if (y < sea + 2)
					continue;
				float slope = Math.AbsFloat(world.GetSurfaceY(x + 3, z) - y) + Math.AbsFloat(world.GetSurfaceY(x, z + 3) - y);
				if (slope > 1.2)
					continue;
				return Vector(x, y, z);
			}
		}
		return vector.Zero;
	}

	static void Start()
	{
		vector mins, maxs;
		GetGame().GetWorldEntity().GetTerrain(0, 0).GetTerrainBoundBox(mins, maxs);
		s_vBase = Spot((mins + maxs) * 0.5);
		SCR_GroupsManagerComponent manager = SCR_GroupsManagerComponent.GetInstance();
		SCR_FactionManager factions = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		if (s_vBase == vector.Zero || !manager || !factions)
		{
			Check("setup", false, "no dry spot, groups manager or faction manager");
			Finish();
			return;
		}
		s_Master = manager.CreateNewPlayableGroup(factions.GetFactionByKey("US"));
		if (s_Master)
		{
			s_Master.SetDeleteWhenEmpty(false);
			s_Master.SetCanDeleteIfNoPlayer(false);
		}
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = s_vBase;
		s_Squad = SCR_AIGroup.Cast(GetGame().SpawnEntityPrefab(Resource.Load(TEAM), GetGame().GetWorld(), params));
		Print(string.Format("[AIO] TEST ordermarker: team spawned at %1", s_vBase));
		// CreateNewPlayableGroup finishes on a delayed call: link a few seconds later.
		GetGame().GetCallqueue().CallLater(Link, 4000, false);
	}

	protected static void Link()
	{
		SCR_GroupsManagerComponent manager = SCR_GroupsManagerComponent.GetInstance();
		if (manager && s_Master && s_Squad)
			manager.RequestSetGroupSlave(SCR_EntityHelper.EntityToRplId(s_Master), SCR_EntityHelper.EntityToRplId(s_Squad));
		GetGame().GetCallqueue().CallLater(NoOrder, 4000, false);
	}

	protected static void NoOrder()
	{
		bool linked = s_Master && s_Squad && s_Master.GetSlave() == s_Squad;
		Check("linked", linked, "the fire team is the playable group's AI group");
		if (!linked)
		{
			Finish();
			return;
		}
		AIO_Server.ClearWaypoints(s_Squad);
		AIO_Server.ClearWaypoints(s_Master);
		AIO_OrderMarkers.s_iTestSends = 0;
		AIO_OrderMarkers.s_TestPlayable = s_Master;
		GetGame().GetCallqueue().CallLater(NoOrderCheck, 2500, false);
	}

	protected static void NoOrderCheck()
	{
		Check("no order, nothing sent", AIO_OrderMarkers.s_iTestSends == 0 && !AIO_OrderMarker.Get().Has(), State());
		s_vTarget = s_vBase + Vector(80, 0, 0);
		s_vTarget[1] = GetGame().GetWorld().GetSurfaceY(s_vTarget[0], s_vTarget[2]);
		AIO_Server.AddWaypoint(s_Squad, AIO_Server.WP_MOVE, s_vTarget);
		GetGame().GetCallqueue().CallLater(MoveCheck, 2500, false);
	}

	protected static void MoveCheck()
	{
		AIO_OrderMarker marker = AIO_OrderMarker.Get();
		Check("move reaches the squad", AIO_OrderMarkers.s_iTestSends == 1 && marker.Has() && marker.Label() == "Move"
			&& vector.Distance(marker.Position(), s_vTarget) < 1.0, State());
		s_iSent = AIO_OrderMarkers.s_iTestSends;
		GetGame().GetCallqueue().CallLater(StillCheck, 3000, false);
	}

	protected static void StillCheck()
	{
		Check("unchanged order not resent", AIO_OrderMarkers.s_iTestSends == s_iSent, State() + " (3 s, 3 ticks)");
		// A new order replaces the old one, as an AI commander or a Game Master would.
		AIO_Server.ClearWaypoints(s_Squad);
		s_vTarget = s_vBase + Vector(0, 0, 70);
		s_vTarget[1] = GetGame().GetWorld().GetSurfaceY(s_vTarget[0], s_vTarget[2]);
		AIO_Server.AddWaypoint(s_Squad, WP_DEFEND, s_vTarget);
		GetGame().GetCallqueue().CallLater(NewOrderCheck, 2500, false);
	}

	protected static void NewOrderCheck()
	{
		AIO_OrderMarker marker = AIO_OrderMarker.Get();
		Check("new order updates the marker", AIO_OrderMarkers.s_iTestSends == s_iSent + 1 && marker.Has() && marker.Label() == "Defend"
			&& vector.Distance(marker.Position(), s_vTarget) < 1.0, State());
		AIWaypoint current = s_Squad.GetCurrentWaypoint();
		if (current)
			s_Squad.CompleteWaypoint(current);
		GetGame().GetCallqueue().CallLater(CompleteCheck, 2500, false);
	}

	protected static void CompleteCheck()
	{
		Check("completed order clears the marker", AIO_OrderMarkers.s_iTestSends == s_iSent + 2 && !AIO_OrderMarker.Get().Has(), State());
		Paths();
		Labels();
		Finish();
	}

	// Which group's waypoint counts: possessed AI group, else the AI group, else the player's own group.
	protected static void Paths()
	{
		AIO_OrderMarkers.s_TestPlayable = null;
		bool none = AIO_OrderMarkers.OrderGroup(s_Master, null) == null;
		AIO_Server.AddWaypoint(s_Master, AIO_Server.WP_MOVE, s_vBase + Vector(-40, 0, 0));
		bool own = AIO_OrderMarkers.OrderGroup(s_Master, null) == s_Master;
		AIO_Server.AddWaypoint(s_Squad, AIO_Server.WP_MOVE, s_vBase + Vector(40, 0, 0));
		bool slave = AIO_OrderMarkers.OrderGroup(s_Master, null) == s_Squad;
		bool possessed = AIO_OrderMarkers.OrderGroup(s_Master, s_Squad) == s_Squad && AIO_OrderMarkers.OrderGroup(null, s_Squad) == s_Squad;
		Check("order group choice", none && own && slave && possessed,
			string.Format("none=%1 own group=%2 AI group first=%3 possessed=%4", none, own, slave, possessed));
		AIO_Server.ClearWaypoints(s_Squad);
		AIO_Server.ClearWaypoints(s_Master);
	}

	protected static void Labels()
	{
		string snd = PrefabLabel(WP_SND);
		string getOut = PrefabLabel(WP_GETOUT);
		string getIn = PrefabLabel(WP_GETIN);
		string defend = PrefabLabel(WP_DEFEND);
		string move = PrefabLabel(AIO_Server.WP_MOVE);
		Check("tags from vanilla waypoints", snd == "Attack" && getOut == "Get out" && getIn == "Get in" && defend == "Defend" && move == "Move",
			string.Format("search and destroy=%1 get out=%2 get in=%3 defend=%4 move=%5", snd, getOut, getIn, defend, move));
	}

	protected static string PrefabLabel(ResourceName prefab)
	{
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = s_vBase;
		AIWaypoint waypoint = AIWaypoint.Cast(GetGame().SpawnEntityPrefab(Resource.Load(prefab), GetGame().GetWorld(), params));
		if (!waypoint)
			return "(no waypoint)";
		string label = AIO_OrderMarkers.Label(waypoint);
		SCR_EntityHelper.DeleteEntityAndChildren(waypoint);
		return label;
	}
}
