// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). GPL v3 or later (see LICENSE).

class AIO_Route
{
	SCR_AIGroup m_Group;
	IEntity m_Leader;
	int m_iPlayerId;
	string m_sElement;
	ref array<IEntity> m_aMen = {};
	ref array<ref AIO_HoldBehavior> m_aHolds = {};
	ref array<vector> m_aPoints = {};
	int m_iWaypoint;
	bool m_bPatrol;
	bool m_bNotifyOnly;
	bool m_bNativeMove;
	ref array<IEntity> m_aCancelled = {};
	float m_fProgressAt;
	float m_fStarted;
	float m_fBestDistance;
	float m_fTravelBudget = 300000; // grows for long map legs, allowing slow walking plus detours
	int m_iReached; // also used by server integration tests
	bool m_bFinished;
	bool m_bBlocked;
	string m_sBlockedMen;
	ref array<IEntity> m_aBlockedMen = {};
}

class AIO_Routes
{
	static const int MAX_POINTS = 16;
	protected static ref array<ref AIO_Route> s_aRoutes = {};

	// Observe ordinary Move here without replacing vanilla driving/group movement.
	static AIO_Route TrackMove(SCR_AIGroup group, IEntity leader, array<IEntity> men, vector point, int playerId, bool nativeMove)
	{
		if (men.IsEmpty())
			return null;
		AIO_Route route = new AIO_Route();
		route.m_Group = group;
		route.m_Leader = leader;
		route.m_iPlayerId = playerId;
		route.m_sElement = Element(men);
		route.m_aMen.Copy(men);
		route.m_aPoints.Insert(point);
		route.m_bNotifyOnly = true;
		route.m_bNativeMove = nativeMove;
		foreach (IEntity man : men)
		{
			route.m_aHolds.Insert(AIO_Tasks.ActiveHold(AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(man))));
			route.m_fTravelBudget = Math.Max(route.m_fTravelBudget, vector.Distance(man.GetOrigin(), point) * 2000 + 120000);
		}
		route.m_fProgressAt = GetGame().GetWorld().GetWorldTime();
		route.m_fStarted = route.m_fProgressAt;
		route.m_fBestDistance = 1e10;
		s_aRoutes.Insert(route);
		AIO_Tasks.Watch();
		return route;
	}

	static void Cleanup()
	{
		s_aRoutes.Clear();
	}

	static void CancelObserver(IEntity man)
	{
		foreach (AIO_Route route : s_aRoutes)
		{
			if (route.m_bNativeMove && route.m_aMen.Contains(man) && !route.m_aCancelled.Contains(man))
				route.m_aCancelled.Insert(man);
		}
	}

	static string Element(array<IEntity> men)
	{
		int team = AIO_Maneuvers.TeamOf(men[0]);
		foreach (IEntity man : men)
		{
			if (AIO_Maneuvers.TeamOf(man) != team)
				team = 0;
		}
		if (team > 0)
		{
			if (men.Count() == 1)
				return AIO_Menu.TeamName(team) + " team (1 soldier)";
			return string.Format("%1 team (%2 men)", AIO_Menu.TeamName(team), men.Count());
		}
		if (men.Count() == 1)
			return "1 soldier";
		return string.Format("%1 men", men.Count());
	}

	// Validation is server-side too: draft membership must still belong to this leader.
	static string Start(SCR_AIGroup group, IEntity leader, array<IEntity> men, array<vector> points, bool patrol, int playerId, out AIO_Route started)
	{
		started = null;
		int active;
		foreach (AIO_Route existing : s_aRoutes)
		{
			if (existing.m_iPlayerId == playerId)
				active++;
		}
		if (active >= 8)
			return "Too many active routes; cancel an existing route with Follow or Halt";
		if (!group || !AIO_Server.IsAlive(leader) || !points || points.IsEmpty() || points.Count() > MAX_POINTS)
			return "Route needs 1-16 waypoints";
		if (patrol && points.Count() < 2)
			return "A patrol needs at least two waypoints";
		array<IEntity> squad = {};
		AIO_Server.Members(group, squad);
		if (!men || men.IsEmpty())
			return "No soldiers selected for route";
		foreach (IEntity man : men)
		{
			if (!squad.Contains(man) || man == leader)
				return "Route members are no longer in your squad";
			if (CompartmentAccessComponent.GetVehicleIn(man))
				return "Routes need soldiers on foot; disembark first";
		}
		vector mins, maxs;
		GetGame().GetWorld().GetBoundBox(mins, maxs);
		foreach (vector point : points)
		{
			if (point[0] != point[0] || point[1] != point[1] || point[2] != point[2] || Math.AbsFloat(point[1]) > 20000)
				return "Invalid waypoint position";
			if (point[0] < mins[0] || point[0] > maxs[0] || point[2] < mins[2] || point[2] > maxs[2])
				return "Waypoint lies outside the map";
		}
		AIO_Route route = new AIO_Route();
		route.m_Group = group;
		route.m_Leader = leader;
		route.m_iPlayerId = playerId;
		route.m_sElement = Element(men);
		route.m_aMen.Copy(men);
		route.m_aPoints.Copy(points);
		route.m_bPatrol = patrol;
		if (AIO_Server.IsPossessed(group) && men.Count() >= squad.Count() - 1)
			AIO_Server.ClearWaypoints(group);
		Dispatch(route);
		s_aRoutes.Insert(route);
		started = route;
		AIO_Tasks.Watch();
		string mode = "route";
		if (patrol)
			mode = "patrol";
		return string.Format("%1: %2 started, %3 waypoints", route.m_sElement, mode, points.Count());
	}

	protected static void Dispatch(AIO_Route route)
	{
		route.m_aHolds.Clear();
		route.m_fTravelBudget = 300000;
		for (int i = 0; i < route.m_aMen.Count(); i++)
		{
			IEntity man = route.m_aMen[i];
			route.m_fTravelBudget = Math.Max(route.m_fTravelBudget, vector.Distance(man.GetOrigin(), route.m_aPoints[route.m_iWaypoint]) * 2000 + 120000);
			AIO_Tasks.MoveTo(man, AIO_Tasks.SpreadSpot(route.m_aPoints[route.m_iWaypoint], i), true);
			route.m_aHolds.Insert(AIO_Tasks.ActiveHold(AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(man))));
		}
		route.m_fProgressAt = GetGame().GetWorld().GetWorldTime();
		route.m_fStarted = route.m_fProgressAt;
		route.m_fBestDistance = 1e10;
	}

	static bool Arrived(IEntity man, AIO_HoldBehavior hold)
	{
		return hold && vector.DistanceXZ(man.GetOrigin(), hold.m_vSpot) <= 5 && Math.AbsFloat(man.GetOrigin()[1] - hold.m_vSpot[1]) <= 2.5;
	}

	static void Report(AIO_Route route, string text)
	{
		string message = route.m_sElement + ": " + text;
		Print("[AIO] ROUTE " + message);
		AIO_Supplies.Report(route.m_iPlayerId, message);
	}

	static string Label(IEntity man)
	{
		foreach (AIO_Route route : s_aRoutes)
		{
			if (route.m_bNotifyOnly || !route.m_aMen.Contains(man)) continue;
			string mode = "Route";
			if (route.m_bPatrol) mode = "Patrol";
			if (route.m_bBlocked) mode += " paused";
			return string.Format("%1 %2/%3", mode, route.m_iWaypoint + 1, route.m_aPoints.Count());
		}
		return "";
	}

	protected static string BlockedMen(AIO_Route route)
	{
		string names;
		int count;
		foreach (IEntity man : route.m_aBlockedMen)
		{
			if (!man || !route.m_aMen.Contains(man)) continue;
			count++;
			if (count > 3) continue;
			if (!names.IsEmpty()) names += ", ";
			names += AIO_Server.NameOf(man);
		}
		if (count > 3) names += string.Format(" +%1", count - 3);
		if (count == 0) names = "blocked men departed; resume when ready";
		return names;
	}

	static string Status(int playerId)
	{
		string text;
		foreach (AIO_Route route : s_aRoutes)
		{
			if (route.m_iPlayerId != playerId || route.m_bNotifyOnly || AIO_Server.Slave(playerId) != route.m_Group) continue;
			if (!text.IsEmpty()) text += "\n";
			text += route.m_sElement + ": " + Label(route.m_aMen[0]);
			if (route.m_bBlocked) text += " | blocked: " + BlockedMen(route) + " | More > Routes: Resume / Skip";
		}
		return text;
	}

	// A selected member identifies his whole route. No selection means all owned paused routes.
	static string Recover(array<IEntity> bodies, int playerId, bool skip)
	{
		Tick(); // discard stale holds, casualties and lost command authority before issuing anything
		int count;
		for (int r = s_aRoutes.Count() - 1; r >= 0; r--)
		{
			AIO_Route route = s_aRoutes[r];
			if (route.m_iPlayerId != playerId || route.m_bNotifyOnly || !route.m_bBlocked) continue;
			bool selected;
			foreach (IEntity man : route.m_aMen)
				if (bodies.Contains(man)) selected = true;
			if (!selected) continue;
			count++;
			if (skip)
			{
				Report(route, string.Format("Waypoint %1/%2 skipped", route.m_iWaypoint + 1, route.m_aPoints.Count()));
				route.m_iWaypoint++;
				if (route.m_iWaypoint >= route.m_aPoints.Count())
				{
					if (route.m_bPatrol) route.m_iWaypoint = 0;
					else
					{
						Report(route, "Route ended after skipping final waypoint. Holding.");
						route.m_bFinished = true;
						s_aRoutes.RemoveOrdered(r);
						continue;
					}
				}
			}
			route.m_bBlocked = false;
			route.m_sBlockedMen = "";
			route.m_aBlockedMen.Clear();
			Dispatch(route);
			Report(route, string.Format("Resuming waypoint %1/%2", route.m_iWaypoint + 1, route.m_aPoints.Count()));
		}
		if (count == 0) return "No paused routes for those soldiers";
		return string.Format("%1 paused routes recovered", count);
	}

	static void Tick()
	{
		float now = GetGame().GetWorld().GetWorldTime();
		for (int r = s_aRoutes.Count() - 1; r >= 0; r--)
		{
			AIO_Route route = s_aRoutes[r];
			bool authorized = AIO_Server.IsAlive(route.m_Leader) && (route.m_iPlayerId <= 0 || AIO_Server.Slave(route.m_iPlayerId) == route.m_Group);
			array<IEntity> squad = {};
			AIO_Server.Members(route.m_Group, squad);
			for (int i = route.m_aMen.Count() - 1; i >= 0; i--)
			{
				IEntity man = route.m_aMen[i];
				AIO_HoldBehavior actual = AIO_Tasks.ActiveHold(AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(man)));
				// Routes need soldiers on foot; a notify-only observer (selected Move here) also watches seated men,
				// so a driver keeps his order and drives instead of being released on the first tick.
				bool replaced = !actual || actual != route.m_aHolds[i] || (!route.m_bNotifyOnly && CompartmentAccessComponent.GetVehicleIn(man));
				if (route.m_bNativeMove)
					replaced = route.m_aCancelled.Contains(man);
				if (!authorized || !squad.Contains(man) || replaced)
				{
					if (!route.m_bNativeMove && man && actual && actual == route.m_aHolds[i])
						AIO_Tasks.Release(man);
					route.m_aMen.RemoveOrdered(i);
					route.m_aHolds.RemoveOrdered(i);
				}
			}
			if (route.m_aMen.IsEmpty())
			{
				route.m_bFinished = true;
				s_aRoutes.RemoveOrdered(r);
				continue;
			}
			route.m_sElement = Element(route.m_aMen);
			if (route.m_bBlocked) continue; // still prune replaced members while paused
			bool arrived = true;
			bool combat;
			float distance;
			for (int m = 0; m < route.m_aMen.Count(); m++)
			{
				IEntity body = route.m_aMen[m];
				AIO_HoldBehavior hold = route.m_aHolds[m];
				if (route.m_bNativeMove)
				{
					float toPoint = vector.Distance(body.GetOrigin(), route.m_aPoints[0]);
					if (toPoint > Math.Max(8, route.m_aMen.Count() * 2))
						arrived = false;
					distance += toPoint;
				}
				else
				{
					if (!Arrived(body, hold))
						arrived = false;
					distance += vector.Distance(body.GetOrigin(), hold.m_vSpot);
				}
				SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(body));
				SCR_AIBehaviorBase current;
				if (utility)
					current = SCR_AIBehaviorBase.Cast(utility.GetExecutedAction());
				if (current && current.GetCause() >= SCR_EAIBehaviorCause.COMBAT)
					combat = true;
			}
			if (arrived)
			{
				route.m_iReached++;
				if (route.m_bNotifyOnly)
				{
					Report(route, "Destination reached.");
					route.m_bFinished = true;
					s_aRoutes.RemoveOrdered(r);
					continue;
				}
				if (!route.m_bPatrol && route.m_iWaypoint < route.m_aPoints.Count() - 1)
					Report(route, string.Format("Waypoint %1/%2 reached", route.m_iWaypoint + 1, route.m_aPoints.Count()));
				route.m_iWaypoint++;
				if (route.m_iWaypoint == route.m_aPoints.Count())
				{
					if (!route.m_bPatrol)
					{
						Report(route, string.Format("Waypoint %1/%2 reached. Route complete. Holding.", route.m_iWaypoint, route.m_aPoints.Count()));
						route.m_bFinished = true;
						s_aRoutes.RemoveOrdered(r);
						continue;
					}
					route.m_iWaypoint = 0;
				}
				Dispatch(route);
				continue;
			}
			if (distance < route.m_fBestDistance - 1 || combat)
			{
				route.m_fBestDistance = distance;
				route.m_fProgressAt = now;
			}
			if (combat)
				route.m_fStarted += AIO_Tasks.WATCH_MS;
			if (now - route.m_fProgressAt < 60000 && now - route.m_fStarted < route.m_fTravelBudget)
				continue;
			if (route.m_bNotifyOnly)
				Report(route, "Destination not reached; movement blocked or stalled.");
			else
			{
				route.m_aBlockedMen.Clear();
				for (int b = 0; b < route.m_aMen.Count(); b++)
					if (!Arrived(route.m_aMen[b], route.m_aHolds[b])) route.m_aBlockedMen.Insert(route.m_aMen[b]);
				route.m_sBlockedMen = BlockedMen(route);
				Report(route, string.Format("Waypoint %1/%2 blocked: %3. Paused; More > Routes: Resume or Skip waypoint.", route.m_iWaypoint + 1, route.m_aPoints.Count(), route.m_sBlockedMen));
			}
			route.m_bBlocked = true;
			if (route.m_bNotifyOnly)
			{
				route.m_bFinished = true;
				s_aRoutes.RemoveOrdered(r);
			}
			else
			{
				route.m_aHolds.Clear();
				foreach (IEntity stalled : route.m_aMen)
				{
					AIO_Tasks.HoldHere(stalled);
					route.m_aHolds.Insert(AIO_Tasks.ActiveHold(AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(stalled))));
				}
			}
		}
	}
}
