// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). Licensed under the GNU GPL v3 or later (see LICENSE).

// Attack maneuvers, Defense, per-man rules of engagement, engage/disengage, fire on my lead,
// colour teams (server side). Server only.

// Per-man fire control. Reforger normally lets the group decide (the soldier's combat mode is
// read from his group); an override set here wins for this one man.
modded class SCR_AICombatComponent
{
	protected bool m_bAIO_HasMode;
	protected EAIGroupCombatMode m_eAIO_Mode;

	void AIO_SetCombatMode(bool enabled, EAIGroupCombatMode mode)
	{
		m_bAIO_HasMode = enabled;
		m_eAIO_Mode = mode;
	}

	bool AIO_HasCombatMode()
	{
		return m_bAIO_HasMode;
	}

	override EAIGroupCombatMode GetCombatMode()
	{
		if (m_bAIO_HasMode)
			return m_eAIO_Mode;
		return super.GetCombatMode();
	}
}

// Engage at will / Disengage per man: whether he stays mindful of his formation in a fight.
modded class SCR_AIUtilityComponent
{
	protected int m_iAIO_KeepFormation = -1; // -1 = game decides, 1 = disengage (keep formation), 0 = engage at will
	protected AIO_HoldBehavior m_AIO_OrderHold;

	void AIO_SetOrderHold(AIO_HoldBehavior hold)
	{
		m_AIO_OrderHold = hold;
	}

	AIO_HoldBehavior AIO_GetOrderHold()
	{
		if (!m_AIO_OrderHold || m_AIO_OrderHold.GetRemoveAction() || m_AIO_OrderHold.GetActionState() == EAIActionState.FAILED || m_AIO_OrderHold.GetActionState() == EAIActionState.COMPLETED)
			return null;
		return m_AIO_OrderHold;
	}

	void AIO_SetKeepFormation(int value)
	{
		m_iAIO_KeepFormation = value;
	}

	int AIO_GetKeepFormation()
	{
		return m_iAIO_KeepFormation;
	}

	override bool ShouldKeepFormation()
	{
		AIO_HoldBehavior hold = AIO_GetOrderHold();
		if (hold && !hold.m_bAllowPursuit)
			return true;
		if (m_iAIO_KeepFormation == 1)
			return true;
		if (m_iAIO_KeepFormation == 0)
			return false;
		return super.ShouldKeepFormation();
	}
}

// Garrison / Clear on a building whose inside could not be found yet (navmesh still loading,
// or none from afar): the men walk up to it first and the order is given again when the inside
// is found. A garrison then stays here as m_bPosted until every man has reached his post or
// given up, and the player gets one honest summary.
class AIO_Approach
{
	ref array<AIAgent> m_aMen = {};
	IEntity m_Building;
	IEntity m_Player;
	int m_iOrder;
	int m_iPlayerId;
	int m_iTicks;
	bool m_bPosted;  // garrison posts handed out; waiting for the men to reach them
	bool m_bWaitTiles; // the building's navmesh was still streaming in: retry once it has
	int m_iGaveUp;   // men who could reach no post (back in formation)
}

// What the men learned about a building: posts nobody could reach and floors with no way up
// (stairs missing from the navmesh, a ladder-only attic). Garrison and Clear skip them from
// then on, so the squad doesn't rediscover a dead end man by man. Forgotten after a while in
// case a door was opened.
class AIO_BuildingMemory
{
	IEntity m_Building;
	float m_fUntil;
	ref array<vector> m_aDeadEnds = {};
	ref array<float> m_aDeadFloors = {};
}

class AIO_Maneuvers
{
	static const float FLANK_WIDTH = 45;
	static const int GARRISON_REPORT_S = 150; // summary even if someone is still on his way in
	static const float VIEW_EYE = 1.5;       // window height check (standing)
	static const float VIEW_PAST = 4;        // sight must carry on this far past a window
	static const vector PROBE_EXTENT = "1.2 1.5 1.2";


	// ---------------------------------------------------------------- teams

	protected static ref array<AIAgent> s_aTeamAgents = {};
	protected static ref array<int> s_aTeamValues = {};

	static void SetTeam(IEntity body, int team)
	{
		AIAgent agent = AIO_Tasks.AgentOf(body);
		if (!agent)
			return;
		int at = s_aTeamAgents.Find(agent);
		if (at >= 0)
		{
			s_aTeamAgents.RemoveOrdered(at);
			s_aTeamValues.RemoveOrdered(at);
		}
		if (team > 0)
		{
			s_aTeamAgents.Insert(agent);
			s_aTeamValues.Insert(team);
		}
	}

	static int TeamOf(IEntity body)
	{
		int at = s_aTeamAgents.Find(AIO_Tasks.AgentOf(body));
		if (at < 0)
			return 0;
		return s_aTeamValues[at];
	}

	// ---------------------------------------------------------------- fire control

	// mode -1 = back to the group's rules.
	static void SetROE(IEntity body, int mode)
	{
		SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(body));
		if (!utility || !utility.m_CombatComponent)
			return;
		if (mode < 0)
			utility.m_CombatComponent.AIO_SetCombatMode(false, EAIGroupCombatMode.FIRE_AT_WILL);
		else
			utility.m_CombatComponent.AIO_SetCombatMode(true, mode);
	}

	static int ROEOf(IEntity body)
	{
		SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(body));
		if (!utility || !utility.m_CombatComponent)
			return -1;
		if (!utility.m_CombatComponent.AIO_HasCombatMode())
			return -1;
		return utility.m_CombatComponent.GetCombatMode();
	}

	static void SetEngage(IEntity body, int keepFormation)
	{
		SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(body));
		if (utility)
			utility.AIO_SetKeepFormation(keepFormation);
	}

	// Fire on my lead: hold fire until the player fires.
	protected static ref array<AIAgent> s_aLeadAgents = {};
	protected static ref array<IEntity> s_aLeadPlayers = {};
	protected static ref array<int> s_aLeadAmmo = {};

	static int PlayerAmmo(IEntity player)
	{
		BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(player.FindComponent(BaseWeaponManagerComponent));
		if (!weapons || !weapons.GetCurrentWeapon() || !weapons.GetCurrentWeapon().GetCurrentMuzzle())
			return -1;
		return weapons.GetCurrentWeapon().GetCurrentMuzzle().GetAmmoCount();
	}

	static void FireOnMyLead(IEntity body, IEntity player)
	{
		AIAgent agent = AIO_Tasks.AgentOf(body);
		if (!agent || !player)
			return;
		StopLead(agent);
		SetROE(body, EAIGroupCombatMode.HOLD_FIRE);
		s_aLeadAgents.Insert(agent);
		s_aLeadPlayers.Insert(player);
		s_aLeadAmmo.Insert(PlayerAmmo(player));
		AIO_Tasks.Watch();
	}

	static void StopLead(AIAgent agent)
	{
		int at = s_aLeadAgents.Find(agent);
		if (at < 0)
			return;
		s_aLeadAgents.RemoveOrdered(at);
		s_aLeadPlayers.RemoveOrdered(at);
		s_aLeadAmmo.RemoveOrdered(at);
	}

	static bool IsWaitingForLead(AIAgent agent)
	{
		return s_aLeadAgents.Contains(agent);
	}

	// The player's first shot (his loaded rounds drop) releases everyone waiting on him.
	protected static void TickLead()
	{
		for (int i = s_aLeadAgents.Count() - 1; i >= 0; i--)
		{
			AIAgent agent = s_aLeadAgents[i];
			IEntity player = s_aLeadPlayers[i];
			IEntity body = null;
			if (agent)
				body = agent.GetControlledEntity();
			if (!body || !player || !AIO_Server.IsAlive(body) || !AIO_Server.IsAlive(player))
			{
				if (body)
					SetROE(body, -1);
				s_aLeadAgents.RemoveOrdered(i);
				s_aLeadPlayers.RemoveOrdered(i);
				s_aLeadAmmo.RemoveOrdered(i);
				continue;
			}
			int ammo = PlayerAmmo(player);
			int before = s_aLeadAmmo[i];
			if (ammo >= 0 && before >= 0 && ammo < before)
			{
				Fired(player);
				return; // lists changed
			}
			s_aLeadAmmo[i] = ammo; // reloads and weapon switches just reset the count
		}
	}

	static void Fired(IEntity player)
	{
		int released;
		for (int i = s_aLeadAgents.Count() - 1; i >= 0; i--)
		{
			if (s_aLeadPlayers[i] != player)
				continue;
			AIAgent agent = s_aLeadAgents[i];
			if (agent && agent.GetControlledEntity())
				SetROE(agent.GetControlledEntity(), EAIGroupCombatMode.FIRE_AT_WILL);
			s_aLeadAgents.RemoveOrdered(i);
			s_aLeadPlayers.RemoveOrdered(i);
			s_aLeadAmmo.RemoveOrdered(i);
			released++;
		}
		Print(string.Format("[AIO] fire on my lead: %1 men open fire", released));
	}

	// ---------------------------------------------------------------- geometry

	static vector Centroid(array<IEntity> bodies)
	{
		vector sum = vector.Zero;
		foreach (IEntity body : bodies)
			sum = sum + body.GetOrigin();
		if (bodies.IsEmpty())
			return sum;
		return sum * (1.0 / bodies.Count());
	}

	static vector Flat(vector v)
	{
		v[1] = 0;
		if (v.Length() < 0.01)
			return Vector(0, 0, 1);
		v.Normalize();
		return v;
	}

	static vector Ground(vector point)
	{
		point[1] = GetGame().GetWorld().GetSurfaceY(point[0], point[2]);
		return point;
	}

	// Men side by side across 'across', 5 m apart, centred on 'center'.
	static vector LineSpot(vector center, vector across, int index, int count)
	{
		float slot = index - (count - 1) * 0.5;
		return Ground(center + across * (slot * 5));
	}

	// Walk a route (legs in order) and hold at the end.
	static bool Route(IEntity body, array<vector> legs)
	{
		if (legs.IsEmpty())
			return false;
		SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(body));
		if (!utility)
			return false;
		AIO_Tasks.Release(body);
		vector first = legs[0];
		AIO_HoldBehavior hold = new AIO_HoldBehavior(utility, null, first);
		hold.m_aLegs = {};
		for (int i = 1; i < legs.Count(); i++)
			hold.m_aLegs.Insert(legs[i]);
		utility.AddAction(new AIO_MoveBehavior(utility, null, first, radius: 2.0));
		utility.AddAction(hold);
		AIO_Tasks.Watch();
		return true;
	}

	// ---------------------------------------------------------------- attack

	static bool IsAttackOrder(int order)
	{
		return order == AIO_EOrder.ATK_ADVANCE || order == AIO_EOrder.ATK_FLANK_LEFT || order == AIO_EOrder.ATK_FLANK_RIGHT
			|| order == AIO_EOrder.ASSAULT || order == AIO_EOrder.SUPPRESS || order == AIO_EOrder.FIRE_ON_MY_LEAD
			|| order == AIO_EOrder.BOUND || order == AIO_EOrder.PEEL || order == AIO_EOrder.ENGAGE_AT_WILL
			|| order == AIO_EOrder.DISENGAGE || order == AIO_EOrder.RESUME_MANEUVER;
	}

	// bodies: the men (never empty). point: where the player looked (zero if none).
	static string Attack(array<IEntity> bodies, IEntity player, int order, vector point, int playerId)
	{
		if (order == AIO_EOrder.RESUME_MANEUVER)
			return AIO_Bounding.Resume(bodies, playerId);
		vector center = Centroid(bodies);
		if (order == AIO_EOrder.ENGAGE_AT_WILL || order == AIO_EOrder.DISENGAGE)
		{
			int keep = 0;
			if (order == AIO_EOrder.DISENGAGE)
				keep = 1;
			foreach (IEntity engaged : bodies)
			{
				if (keep == 0)
					AIO_Tasks.Release(engaged); // explicit permission to leave the assigned post
				SetEngage(engaged, keep);
			}
			if (keep == 1)
				return string.Format("%1 men disengaging: they stay with the formation", bodies.Count());
			return string.Format("%1 men engaging at will", bodies.Count());
		}
		if (order == AIO_EOrder.FIRE_ON_MY_LEAD)
		{
			if (!player)
				return "You need to be on foot in the world to give that order";
			foreach (IEntity waiter : bodies)
				FireOnMyLead(waiter, player);
			return string.Format("%1 men holding fire until you open up", bodies.Count());
		}
		if (order == AIO_EOrder.PEEL)
		{
			if (!player)
				return "You need to be on foot in the world to give that order";
			vector forward, right;
			AIO_Tasks.Facing(player, forward, right);
			AIO_Bound peeling;
			string peeled = AIO_Bounding.Start(bodies, forward * -1, vector.Zero, true, playerId, player, peeling);
			if (!peeling)
				return peeled;
			return peeled + AIO_Grenades.Cover(bodies, AIO_Grenades.ThreatPoint(bodies, player.GetOrigin() + forward * 50), 2, playerId);
		}
		if (point == vector.Zero)
			return "Look at a spot first";
		vector dir = Flat(point - center);
		vector side = Vector(dir[2], 0, -dir[0]); // right of the direction of travel
		if (order == AIO_EOrder.BOUND)
			return StartBound(bodies, dir, point, false, playerId, player);
		if (order == AIO_EOrder.SUPPRESS)
		{
			int suppressing;
			foreach (IEntity gunner : bodies)
			{
				if (Suppress(gunner, point, 20))
					suppressing++;
			}
			return string.Format("%1 men suppressing", suppressing);
		}
		int count = bodies.Count();
		int sent;
		if (order == AIO_EOrder.ATK_ADVANCE || order == AIO_EOrder.ASSAULT)
		{
			for (int i = 0; i < count; i++)
			{
				if (order == AIO_EOrder.ASSAULT)
					SetEngage(bodies[i], 0); // free to fight their way in
				if (AIO_Tasks.MoveTo(bodies[i], LineSpot(point, side, i, count)))
				{
					sent++;
					if (order == AIO_EOrder.ASSAULT)
					{
						AIO_HoldBehavior assaultHold = AIO_Tasks.ActiveHold(AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(bodies[i])));
						if (assaultHold)
							assaultHold.m_bAllowPursuit = true;
					}
				}
			}
			if (order == AIO_EOrder.ASSAULT)
				return string.Format("%1 men assaulting", sent);
			return string.Format("%1 men advancing", sent);
		}
		// Flank: swing out wide to one side, come up beside the target, hold there facing it.
		float sign = 1;
		string which = "right";
		if (order == AIO_EOrder.ATK_FLANK_LEFT)
		{
			sign = -1;
			which = "left";
		}
		float distance = vector.DistanceXZ(center, point);
		float width = Math.Max(FLANK_WIDTH, distance * 0.5);
		vector outward = side * sign;
		vector legOut = center + dir * (distance * 0.35) + outward * width;
		vector legBeside = point + outward * Math.Max(30, distance * 0.35);
		for (int f = 0; f < count; f++)
		{
			array<vector> legs = {};
			legs.Insert(LineSpot(legOut, dir, f, count));
			legs.Insert(LineSpot(legBeside, dir, f, count));
			if (Route(bodies[f], legs))
				sent++;
		}
		string smoke;
		if (sent > 0)
			smoke = AIO_Grenades.Cover(bodies, point, 2, playerId); // screens the swing out
		return string.Format("%1 men flanking %2", sent, which) + smoke;
	}

	static bool Suppress(IEntity body, vector point, float seconds)
	{
		SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(body));
		if (!utility)
			return false;
		SCR_AISuppressionVolumeSphere volume = new SCR_AISuppressionVolumeSphere(point + Vector(0, 1, 0), 4);
		utility.AddAction(new SCR_AISuppressBehavior(utility, null, volume, seconds, 1));
		return true;
	}

	// ---------------------------------------------------------------- bound / peel

	static string StartBound(array<IEntity> bodies, vector dir, vector target, bool peel, int playerId, IEntity leader = null)
	{
		AIO_Bound started;
		return AIO_Bounding.Start(bodies, dir, target, peel, playerId, leader, started);
	}

	static void Bodies(array<AIAgent> agents, notnull array<IEntity> bodies)
	{
		foreach (AIAgent agent : agents)
		{
			if (agent && agent.GetControlledEntity() && AIO_Server.IsAlive(agent.GetControlledEntity()))
				bodies.Insert(agent.GetControlledEntity());
		}
	}

	// A new order for this man takes him out of any bound.
	static void Leave(AIAgent agent)
	{
		if (AIO_Bounding.IsIssuing() || !agent)
			return;
		// Fire on my lead set a per-man hold fire: hand his fire back, or he never shoots again
		if (IsWaitingForLead(agent) && agent.GetControlledEntity())
			SetROE(agent.GetControlledEntity(), -1);
		StopLead(agent);
		for (int i = s_aApproaches.Count() - 1; i >= 0; i--)
		{
			s_aApproaches[i].m_aMen.RemoveItem(agent);
			if (s_aApproaches[i].m_bPosted && s_aApproaches[i].m_aMen.IsEmpty() && s_aApproaches[i].m_iGaveUp == 0)
				s_aApproaches.Remove(i); // everyone has a new order: nothing to report
		}
		AIO_Bounding.Leave(agent);
	}

	static bool IsBounding(AIAgent agent)
	{
		return AIO_Bounding.ActiveFor(agent) != null;
	}

	static void Tick()
	{
		TickLead();
		TickApproaches();
		AIO_Bounding.Tick();
	}

	// ---------------------------------------------------------------- defense

	static bool Is360Order(int order)
	{
		return order == AIO_EOrder.DEF_360 || (order >= AIO_EOrder.DEF_360_5 && order <= AIO_EOrder.DEF_360_50);
	}

	static float DefenseRadius(int order, int count)
	{
		switch (order)
		{
			case AIO_EOrder.DEF_360_5: return 5;
			case AIO_EOrder.DEF_360_10: return 10;
			case AIO_EOrder.DEF_360_20: return 20;
			case AIO_EOrder.DEF_360_30: return 30;
			case AIO_EOrder.DEF_360_50: return 50;
		}
		return Math.Clamp(count * 1.5, 5, 12);
	}

	static bool IsDefenseOrder(int order)
	{
		return order == AIO_EOrder.DEF_GARRISON || order == AIO_EOrder.DEF_CLEAR || Is360Order(order) || order == AIO_EOrder.DEF_FORTIFY;
	}

	static string Defense(array<IEntity> bodies, IEntity player, int order, vector point, IEntity looked)
	{
		if (Is360Order(order))
		{
			if (!player)
				return "You need to be on foot in the world to give that order";
			vector center = player.GetOrigin();
			int count = bodies.Count();
			float radius = DefenseRadius(order, count);
			for (int i = 0; i < count; i++)
			{
				float angle = Math.PI2 * i / count;
				vector outDir = Vector(Math.Sin(angle), 0, Math.Cos(angle));
				vector spot = Ground(center + outDir * radius);
				HoldFacing(bodies[i], spot, center + outDir * 80, true);
			}
			return string.Format("%1 men in all-round defense (%2 m radius)", count, radius);
		}
		if (order == AIO_EOrder.DEF_FORTIFY)
		{
			if (point == vector.Zero)
				return "Look at a spot first";
			vector from = point;
			if (player)
				from = player.GetOrigin();
			vector threatDir = Flat(point - from);
			vector threat = point + threatDir * 100;
			int inCover;
			int index;
			foreach (IEntity body : bodies)
			{
				vector fallback = AIO_Tasks.SpreadSpot(point, index);
				index++;
				if (CoverNear(body, point, 20, threat, fallback))
					inCover++;
			}
			return string.Format("%1 men fortifying (%2 in cover)", bodies.Count(), inCover);
		}
		// Garrison / Clear: a building.
		IEntity building = BuildingOf(looked);
		if (!building)
			return "Look at a building";
		array<vector> inside = {};
		array<vector> views = {};
		InteriorPoints(building, bodies[0], inside, views);
		Print("[AIO] Interior of " + InteriorReport(building));
		if (inside.IsEmpty())
			return Approach(bodies, player, order, building);
		DropCutOff(building, inside, views);
		vector mins, maxs;
		building.GetWorldBounds(mins, maxs);
		vector middle = (mins + maxs) * 0.5;
		int playerId;
		if (player)
			playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(player);
		if (order == AIO_EOrder.DEF_GARRISON)
		{
			// Posts at windows first (a view out), spread over the building; then any other room.
			array<vector> windows = {};
			array<vector> rooms = {};
			for (int w = 0; w < inside.Count(); w++)
			{
				if (views[w] != vector.Zero)
					windows.Insert(inside[w]);
				else
					rooms.Insert(inside[w]);
			}
			array<vector> posts = {};
			PickSpread(windows, bodies.Count(), posts);
			int atWindows = posts.Count();
			if (posts.Count() < bodies.Count())
				PickSpread(rooms, bodies.Count(), posts);
			// Nearest man to each post, so the squad doesn't cross paths in the doorway.
			array<int> assigned = {};
			AssignNearest(bodies, posts, assigned);
			AIO_Approach job = new AIO_Approach();
			job.m_Building = building;
			job.m_Player = player;
			job.m_iOrder = order;
			job.m_iPlayerId = playerId;
			job.m_bPosted = true;
			for (int g = 0; g < bodies.Count(); g++)
			{
				if (assigned[g] < 0)
				{
					AIO_Tasks.Rejoin(bodies[g], player);
					continue;
				}
				vector post = posts[assigned[g]];
				HoldFacing(bodies[g], post, post + PostFacing(post, inside, views, middle) * 60, false);
				AIO_HoldBehavior garrison = AIO_Tasks.ActiveHold(AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(bodies[g])));
				if (!garrison)
					continue;
				garrison.m_Building = building;
				AIO_Tasks.GuardInterior(bodies[g], garrison, playerId);
				job.m_aMen.Insert(AIO_Tasks.AgentOf(bodies[g]));
			}
			if (!job.m_aMen.IsEmpty())
			{
				s_aApproaches.Insert(job);
				AIO_Tasks.Watch();
			}
			return string.Format("%1 men garrisoning, %2 at windows (%3 remain in formation)", posts.Count(), atWindows, bodies.Count() - posts.Count());
		}
		// Clear: the rooms are shared out, each man walks his own nearest-first and holds inside
		// at his last one. All men walking every room in single file jammed narrow houses.
		array<vector> sweep = {};
		PickSpread(inside, Math.Min(inside.Count(), Math.Max(6, bodies.Count() * 2)), sweep);
		// order the sweep from the men's side of the building inward
		vector approach = Centroid(bodies);
		SortByDistance(sweep, approach);
		for (int c = 0; c < bodies.Count(); c++)
		{
			array<vector> legs = {};
			for (int r = c % sweep.Count(); r < sweep.Count(); r += bodies.Count())
				legs.Insert(sweep[r]); // sideways offsets can put later squad members inside walls
			Route(bodies[c], legs);
			AIO_Tasks.GuardInterior(bodies[c], AIO_Tasks.ActiveHold(AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(bodies[c]))), playerId);
			AIO_HoldBehavior clearing = AIO_Tasks.ActiveHold(AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(bodies[c])));
			if (clearing)
				clearing.m_Building = building;
			SetEngage(bodies[c], 0);
		}
		return string.Format("%1 men clearing the building (%2 rooms)", bodies.Count(), sweep.Count());
	}

	static ref array<ref AIO_Approach> s_aApproaches = {};

	// No inside found (yet): walk the men to the side of the building facing them, then try again.
	static string Approach(array<IEntity> bodies, IEntity player, int order, IEntity building)
	{
		vector mins, maxs;
		building.GetWorldBounds(mins, maxs);
		vector middle = (mins + maxs) * 0.5;
		vector side = Flat(Centroid(bodies) - middle);
		if (side == vector.Zero)
			side = "1 0 0";
		float half = Math.Max(maxs[0] - mins[0], maxs[2] - mins[2]) * 0.5;
		vector door = Ground(middle + side * (half + 3));
		AIO_Approach approach = new AIO_Approach();
		approach.m_Building = building;
		approach.m_Player = player;
		approach.m_iOrder = order;
		approach.m_bWaitTiles = s_bTilesLoading;
		if (player)
			approach.m_iPlayerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(player);
		int index;
		foreach (IEntity body : bodies)
		{
			AIO_Tasks.MoveTo(body, AIO_Tasks.SpreadSpot(door, index));
			index++;
			AIAgent agent = AIO_Tasks.AgentOf(body);
			if (agent)
				approach.m_aMen.Insert(agent);
		}
		if (approach.m_aMen.IsEmpty())
			return "No way into that building";
		s_aApproaches.Insert(approach);
		AIO_Tasks.Watch();
		return string.Format("%1 men moving up to the building first", approach.m_aMen.Count());
	}

	// Men still walking up to a building (not yet given posts).
	static int Approaching()
	{
		int count;
		foreach (AIO_Approach approach : s_aApproaches)
		{
			if (!approach.m_bPosted)
				count++;
		}
		return count;
	}

	// The garrison a man belongs to, or null.
	static AIO_Approach GarrisonOf(AIAgent agent)
	{
		foreach (AIO_Approach approach : s_aApproaches)
		{
			if (approach.m_bPosted && approach.m_aMen.Contains(agent))
				return approach;
		}
		return null;
	}

	static const float MEMORY_MS = 600000;
	static ref array<ref AIO_BuildingMemory> s_aMemories = {};

	static AIO_BuildingMemory MemoryOf(IEntity building, bool create)
	{
		float now = GetGame().GetWorld().GetWorldTime();
		for (int i = s_aMemories.Count() - 1; i >= 0; i--)
		{
			AIO_BuildingMemory memory = s_aMemories[i];
			if (!memory.m_Building || memory.m_fUntil < now)
			{
				s_aMemories.Remove(i);
				continue;
			}
			if (memory.m_Building == building)
				return memory;
		}
		if (!create || !building)
			return null;
		AIO_BuildingMemory added = new AIO_BuildingMemory();
		added.m_Building = building;
		s_aMemories.Insert(added);
		return added;
	}

	// A post or room he could not reach: nobody is sent near it again. When the pathfinder gave
	// up ('pathEnd', not just a jam on the stairs) while he stood within the building's footprint
	// on another level, that whole floor is cut off from here.
	static void DeadEnd(IEntity building, IEntity body, vector spot, bool pathEnd)
	{
		AIO_BuildingMemory memory = MemoryOf(building, true);
		if (!memory)
			return;
		memory.m_fUntil = GetGame().GetWorld().GetWorldTime() + MEMORY_MS;
		memory.m_aDeadEnds.Insert(spot);
		vector at = body.GetOrigin();
		vector mins, maxs;
		building.GetWorldBounds(mins, maxs);
		bool under = at[0] > mins[0] && at[0] < maxs[0] && at[2] > mins[2] && at[2] < maxs[2];
		if (pathEnd && Math.AbsFloat(spot[1] - at[1]) > 1.5 && under && !memory.m_aDeadFloors.Contains(spot[1]))
		{
			memory.m_aDeadFloors.Insert(spot[1]);
			Print(string.Format("[AIO] Floor at height %1 of %2 has no way in from %3; skipping it", spot[1], building, at));
		}
	}

	// Known to be cut off: near a dead end on the same floor, or on a dead floor.
	static bool CutOff(IEntity building, vector point)
	{
		AIO_BuildingMemory memory = MemoryOf(building, false);
		if (!memory)
			return false;
		foreach (vector dead : memory.m_aDeadEnds)
		{
			if (vector.DistanceXZ(dead, point) < 2 && Math.AbsFloat(dead[1] - point[1]) < 1)
				return true;
		}
		foreach (float floor : memory.m_aDeadFloors)
		{
			if (Math.AbsFloat(floor - point[1]) < 1)
				return true;
		}
		return false;
	}

	// Drop what the squad already knows to be cut off (all of it if nothing else is left).
	static void DropCutOff(IEntity building, notnull array<vector> points, array<vector> views)
	{
		array<vector> keptPoints = {};
		array<vector> keptViews = {};
		for (int i = 0; i < points.Count(); i++)
		{
			if (CutOff(building, points[i]))
				continue;
			keptPoints.Insert(points[i]);
			if (views)
				keptViews.Insert(views[i]);
		}
		if (keptPoints.IsEmpty())
			return;
		points.Copy(keptPoints);
		if (views)
			views.Copy(keptViews);
	}

	// He could reach none of his posts; he rejoins the formation and the summary says so.
	static void GaveUp(AIAgent agent)
	{
		AIO_Approach job = GarrisonOf(agent);
		if (job)
			job.m_iGaveUp++;
	}

	static string s_sLastGarrison; // for the garrison test

	// One report per garrison once nobody is still on the way in: who is at a post, who isn't.
	protected static bool TickGarrison(AIO_Approach job)
	{
		int inPlace;
		int moving;
		for (int m = job.m_aMen.Count() - 1; m >= 0; m--)
		{
			AIAgent agent = job.m_aMen[m];
			IEntity body;
			if (agent)
				body = agent.GetControlledEntity();
			AIO_HoldBehavior hold = AIO_Tasks.ActiveHold(AIO_Tasks.UtilityOf(agent));
			if (!body || !AIO_Server.IsAlive(body) || !hold || hold.m_Building != job.m_Building)
			{
				job.m_aMen.Remove(m);
				continue;
			}
			if (hold.m_bReachedPost)
				inPlace++;
			else
				moving++;
		}
		if (moving > 0 && job.m_iTicks < GARRISON_REPORT_S)
			return false;
		if (inPlace + moving + job.m_iGaveUp == 0)
			return true;
		string result = string.Format("Garrison: %1 men in position", inPlace);
		if (moving > 0)
			result += string.Format(", %1 still working their way in", moving);
		if (job.m_iGaveUp > 0)
			result += string.Format(", %1 found no way to a post and rejoined the formation", job.m_iGaveUp);
		AIO_Supplies.Report(job.m_iPlayerId, result);
		Print("[AIO] " + result);
		s_sLastGarrison = result;
		return true;
	}

	protected static void TickApproaches()
	{
		for (int i = s_aApproaches.Count() - 1; i >= 0; i--)
		{
			if (i >= s_aApproaches.Count())
				continue; // a Rejoin below (Release -> Leave) can drop other approaches
			AIO_Approach approach = s_aApproaches[i];
			approach.m_iTicks++;
			if (approach.m_bPosted)
			{
				if (TickGarrison(approach))
					s_aApproaches.Remove(i);
				continue;
			}
			array<IEntity> men = {};
			Bodies(approach.m_aMen, men);
			if (men.IsEmpty() || !approach.m_Building)
			{
				s_aApproaches.Remove(i);
				foreach (IEntity man : men)
					AIO_Tasks.Rejoin(man, approach.m_Player);
				continue;
			}
			vector mins, maxs;
			approach.m_Building.GetWorldBounds(mins, maxs);
			vector middle = (mins + maxs) * 0.5;
			float half = Math.Max(maxs[0] - mins[0], maxs[2] - mins[2]) * 0.5;
			bool close = vector.DistanceXZ(Centroid(men), middle) < half + 10;
			// Try as soon as the building's navmesh has loaded, every 5 s on the way, and once
			// more when they are there.
			bool loaded = approach.m_bWaitTiles && NavmeshReady(approach.m_Building, false);
			if (loaded)
				approach.m_bWaitTiles = false;
			if (!close && !loaded && approach.m_iTicks % 5 != 0 && approach.m_iTicks < 90)
				continue;
			array<vector> inside = {};
			InteriorPoints(approach.m_Building, men[0], inside);
			if (inside.IsEmpty() && !close && approach.m_iTicks < 90)
				continue;
			s_aApproaches.Remove(i);
			string result;
			if (inside.IsEmpty())
			{
				Print("[AIO] Interior of " + InteriorReport(approach.m_Building));
				result = "No way into that building";
				foreach (IEntity man : men)
					AIO_Tasks.Rejoin(man, approach.m_Player);
			}
			else
				result = Defense(men, approach.m_Player, approach.m_iOrder, vector.Zero, approach.m_Building);
			AIO_Supplies.Report(approach.m_iPlayerId, result);
			Print("[AIO] " + result);
			return; // lists may have changed
		}
	}

	// Hold a spot looking outward; crouched if asked.
	static void HoldFacing(IEntity body, vector spot, vector look, bool crouch)
	{
		if (!AIO_Tasks.MoveTo(body, spot))
			return;
		SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(body));
		AIO_HoldBehavior hold = AIO_Tasks.ActiveHold(utility);
		if (!hold)
			return;
		hold.m_vLook = look;
		if (crouch)
		{
			hold.m_bCrouched = true;
			AIO_Tasks.SetStance(AIO_Tasks.AgentOf(body), ECharacterStance.CROUCH);
		}
	}

	// Cover within radius of a point facing the threat; else the fallback spot, crouched.
	static bool CoverNear(IEntity body, vector point, float radius, vector threat, vector fallback)
	{
		AIAgent agent = AIO_Tasks.AgentOf(body);
		SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(agent);
		if (!utility)
			return false;
		ChimeraCoverManagerComponent covers = AIO_Tasks.Covers();
		AIPathfindingComponent pathfinding = AIPathfindingComponent.Cast(body.FindComponent(AIPathfindingComponent));
		bool found;
		vector coverPos, tallest;
		int tileX, tileY, coverId;
		if (covers && pathfinding)
		{
			CoverQueryProperties query = new CoverQueryProperties();
			query.m_vNearestPolyHalfExtend = SCR_AIFindCover.NEAREST_POLY_HALF_EXTEND;
			query.m_fNmAreaCostScale = SCR_AIFindCover.NAVMESH_AREA_COST_SCALE;
			query.m_vSectorPos = point;
			query.m_vSectorDir = vector.Zero;
			query.m_vThreatPos = threat;
			query.m_fQuerySectorAngleCosMin = -1.0;
			query.m_fSectorDistMin = 0;
			query.m_fSectorDistMax = radius;
			query.m_fCoverToThreatAngleCosMin = 0.3;
			query.m_fScoreWeightDirection = 0;
			query.m_fScoreWeightDistance = 1.0;
			query.m_bCheckVisibility = false;
			query.m_bSelectHighestScore = false;
			query.m_iMaxCoversToCheck = 25;
			found = covers.GetBestCover("Soldiers", pathfinding, query, coverPos, tallest, tileX, tileY, coverId);
		}
		vector spot = fallback;
		if (found)
			spot = coverPos;
		HoldFacing(body, spot, threat, true);
		if (found)
		{
			covers.SetOccupiedCover(tileX, tileY, coverId, true);
			AIO_HoldBehavior hold = AIO_Tasks.ActiveHold(utility);
			if (hold)
			{
				hold.m_iCoverTileX = tileX;
				hold.m_iCoverTileY = tileY;
				hold.m_iCoverId = coverId;
			}
		}
		return found;
	}

	// The building the player means: walking up from what he looked at, the topmost entity
	// that has a model of its own and is big enough (a composition root has no model; a door,
	// window or piece of furniture is too small).
	static IEntity BuildingOf(IEntity looked)
	{
		if (!looked)
			return null;
		IEntity best = null;
		IEntity walk = looked;
		while (walk)
		{
			if (ChimeraCharacter.Cast(walk) || walk.FindComponent(InventoryItemComponent) || Vehicle.Cast(walk))
				return null;
			if (walk.GetVObject() && BigEnough(walk))
				best = walk;
			walk = walk.GetParent();
		}
		return best;
	}

	static bool BigEnough(IEntity entity)
	{
		vector mins, maxs;
		entity.GetWorldBounds(mins, maxs);
		vector size = maxs - mins;
		return size[0] >= 4 && size[2] >= 4 && size[1] >= 2.5;
	}

	// Why the last InteriorPoints call found nothing (for the log).
	static int s_iProbes;
	static int s_iNavHits;
	static int s_iOutside;
	static int s_iOpen;
	static int s_iFurniture;
	static bool s_bTilesLoading;

	// The navmesh streams in around AI. A building nobody is near has no navmesh yet, which
	// used to read as "no inside": ask the engine to load its tiles (takes a few frames).
	static bool NavmeshReady(IEntity building, bool request)
	{
		AIWorld ai = GetGame().GetAIWorld();
		if (!ai)
			return true;
		NavmeshWorldComponent mesh = ai.GetNavmeshWorldComponent("Soldiers");
		if (!mesh)
			return true;
		vector mins, maxs;
		building.GetWorldBounds(mins, maxs);
		array<vector> corners = {mins, Vector(mins[0], mins[1], maxs[2]), Vector(maxs[0], mins[1], mins[2]), Vector(maxs[0], mins[1], maxs[2]), (mins + maxs) * 0.5};
		bool ready = true;
		foreach (vector corner : corners)
		{
			if (mesh.IsTileLoaded(corner))
				continue;
			ready = false;
			if (request && !mesh.IsTileRequested(corner))
				mesh.LoadTileIn(corner);
		}
		return ready;
	}

	// Standable points inside the building, and for each the direction of a view out of a
	// window (zero = none) when 'views' is given.
	static void InteriorPoints(IEntity building, IEntity walker, notnull array<vector> points, array<vector> views = null)
	{
		s_bTilesLoading = !NavmeshReady(building, true);
		SamplePoints(building, walker, points);
		if (!views)
			return;
		foreach (vector point : points)
			views.Insert(ViewOut(point, walker));
	}

	// A grid over the building's footprint on each floor, snapped to the navmesh, kept when they
	// stay inside the walls, stand on the building's own floor (not furniture) and have a roof
	// over them. Buildings with no roofed navmesh (open towers, decks) fall back to the open
	// points inside.
	protected static void SamplePoints(IEntity building, IEntity walker, notnull array<vector> points)
	{
		s_iProbes = 0;
		s_iNavHits = 0;
		s_iOutside = 0;
		s_iOpen = 0;
		s_iFurniture = 0;
		AIPathfindingComponent pathfinding = AIPathfindingComponent.Cast(walker.FindComponent(AIPathfindingComponent));
		if (!pathfinding)
			return;
		vector mins, maxs;
		building.GetWorldBounds(mins, maxs);
		array<vector> open = {};
		float inset = 1.0;
		for (float y = mins[1] + 0.5; y < maxs[1] - 1.0; y += 2.5)
		{
			for (float x = mins[0] + inset; x <= maxs[0] - inset; x += 2.0)
			{
				for (float z = mins[2] + inset; z <= maxs[2] - inset; z += 2.0)
				{
					if (s_iProbes >= 4000)
						break;
					s_iProbes++;
					vector probe = Vector(x, y, z);
					vector corrected;
					if (!pathfinding.GetClosestPositionOnNavmesh(probe, PROBE_EXTENT, corrected))
						continue;
					s_iNavHits++;
					if (corrected[0] < mins[0] + 0.3 || corrected[0] > maxs[0] - 0.3 || corrected[2] < mins[2] + 0.3 || corrected[2] > maxs[2] - 0.3 || corrected[1] < mins[1] - 1.0)
					{
						s_iOutside++;
						continue;
					}
					if (!StandingRoom(corrected, walker) || !BuildingFloor(corrected, walker, building, true))
						continue;
					// Under a roof: something solid above (rules out yards inside the bounds).
					if (!Covered(corrected, maxs[1] + 1))
					{
						s_iOpen++;
						if (open.Count() < 200)
							AddSpaced(open, corrected, 2.0);
						continue;
					}
					AddSpaced(points, corrected, 2.0);
					if (points.Count() >= 40)
						return;
				}
			}
		}
		// Nothing roofed: use the open points well inside the footprint (a tower's deck).
		if (points.IsEmpty())
		{
			vector middle = (mins + maxs) * 0.5;
			float reach = Math.Min(maxs[0] - mins[0], maxs[2] - mins[2]) * 0.5;
			foreach (vector spot : open)
			{
				if (vector.DistanceXZ(spot, middle) <= reach && spot[1] > GetGame().GetWorld().GetSurfaceY(spot[0], spot[2]) + 1.5)
					AddSpaced(points, spot, 2.0);
				if (points.Count() >= 40)
					return;
			}
		}
	}

	static void AddSpaced(notnull array<vector> points, vector spot, float spacing)
	{
		foreach (vector existing : points)
		{
			if (vector.Distance(existing, spot) < spacing)
				return;
		}
		points.Insert(spot);
	}

	static string InteriorReport(IEntity building)
	{
		vector mins, maxs;
		building.GetWorldBounds(mins, maxs);
		string name = "?";
		if (building.GetPrefabData())
			name = building.GetPrefabData().GetPrefabName();
		string report = string.Format("%1 size %2: %3 probes, %4 on navmesh, %5 outside, %6 unroofed, %7 on furniture", name, maxs - mins, s_iProbes, s_iNavHits, s_iOutside, s_iOpen, s_iFurniture);
		if (s_bTilesLoading)
			report += " (navmesh still loading)";
		return report;
	}

	// The open direction at a window: sight at eye height through glass and window frames that
	// carries on past the window, or a long clear view (an open deck, an empty window hole).
	// Zero when every direction faces a wall.
	static vector ViewOut(vector point, IEntity walker)
	{
		vector eye = point + Vector(0, VIEW_EYE, 0);
		vector best = vector.Zero;
		float bestScore = 0;
		for (int i = 0; i < 8; i++)
		{
			float angle = Math.PI2 * i / 8;
			vector dir = Vector(Math.Sin(angle), 0, Math.Cos(angle));
			float score = SightOut(eye, dir, walker);
			if (score > bestScore)
			{
				bestScore = score;
				best = dir;
			}
		}
		return best;
	}

	// > 0 when the line of sight leaves the building: metres seen beyond a window, or the full
	// range for an unbroken 20 m view.
	protected static float SightOut(vector eye, vector dir, IEntity walker)
	{
		float range = 20;
		vector start = eye;
		IEntity skip = walker;
		float windowAt = -1;
		for (int pass = 0; pass < 4; pass++)
		{
			TraceParam trace = new TraceParam();
			trace.Start = start;
			trace.End = eye + dir * range;
			trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
			trace.Exclude = skip;
			float fraction = GetGame().GetWorld().TraceMove(trace, null);
			vector hit = trace.Start + (trace.End - trace.Start) * fraction;
			float seen = vector.Distance(eye, hit);
			if (fraction >= 1 || !SeeThrough(trace.TraceEnt))
			{
				if (windowAt >= 0 && seen - windowAt >= VIEW_PAST)
					return seen - windowAt;
				if (windowAt < 0 && fraction >= 1)
					return range;
				return 0;
			}
			if (windowAt < 0)
				windowAt = seen;
			skip = trace.TraceEnt;
			start = hit + dir * 0.3; // past the pane and frame
		}
		return 0;
	}

	// Window frames and glass are separate entities in the building: they don't block the view.
	static bool SeeThrough(IEntity entity)
	{
		while (entity)
		{
			if (entity.GetPrefabData())
			{
				string name = entity.GetPrefabData().GetPrefabName();
				if (name.Contains("/Windows/") || name.Contains("Glass"))
					return true;
			}
			entity = entity.GetParent();
		}
		return false;
	}

	// Where a post's man should look: out of its window, else away from the middle.
	static vector PostFacing(vector post, array<vector> inside, array<vector> views, vector middle)
	{
		for (int i = 0; i < inside.Count() && i < views.Count(); i++)
		{
			if (inside[i] == post && views[i] != vector.Zero)
				return views[i];
		}
		return Flat(post - middle);
	}

	// Each post to the nearest free man (greedy, closest pair first); -1 = no post for him.
	static void AssignNearest(array<IEntity> bodies, array<vector> posts, notnull array<int> assigned)
	{
		array<bool> taken = {};
		foreach (vector post : posts)
			taken.Insert(false);
		foreach (IEntity body : bodies)
			assigned.Insert(-1);
		int pairs = Math.Min(bodies.Count(), posts.Count());
		for (int n = 0; n < pairs; n++)
		{
			float best = float.MAX;
			int bestMan = -1;
			int bestPost = -1;
			for (int g = 0; g < bodies.Count(); g++)
			{
				if (assigned[g] >= 0)
					continue;
				for (int p = 0; p < posts.Count(); p++)
				{
					if (taken[p])
						continue;
					float cost = PostCost(bodies[g].GetOrigin(), posts[p]);
					if (cost < best)
					{
						best = cost;
						bestMan = g;
						bestPost = p;
					}
				}
			}
			if (bestMan < 0)
				return;
			assigned[bestMan] = bestPost;
			taken[bestPost] = true;
		}
	}

	// Distance with climbing counted extra (stairs are the slow part).
	static float PostCost(vector from, vector post)
	{
		return vector.Distance(from, post) + Math.AbsFloat(post[1] - from[1]) * 2;
	}

	// A free interior point for a man who could not reach his post: near where he is, not one he
	// (or anyone) already found unreachable and not one another man of his garrison holds.
	// Window posts preferred. 'look' = where to face. Zero = none left.
	static vector AlternatePost(IEntity body, AIO_HoldBehavior hold, out vector look)
	{
		look = vector.Zero;
		if (!hold.m_Building)
			return vector.Zero;
		array<vector> inside = {};
		array<vector> views = {};
		InteriorPoints(hold.m_Building, body, inside, views);
		array<vector> claimed = {};
		if (hold.m_aFailedSpots)
			claimed.InsertAll(hold.m_aFailedSpots);
		claimed.Insert(hold.m_vSpot);
		AIO_Approach job = GarrisonOf(AIO_Tasks.AgentOf(body));
		if (job)
		{
			foreach (AIAgent agent : job.m_aMen)
			{
				AIO_HoldBehavior other = AIO_Tasks.ActiveHold(AIO_Tasks.UtilityOf(agent));
				if (other && other != hold)
					claimed.Insert(other.m_vSpot);
			}
		}
		vector mins, maxs;
		hold.m_Building.GetWorldBounds(mins, maxs);
		vector middle = (mins + maxs) * 0.5;
		vector best = vector.Zero;
		float bestCost = float.MAX;
		for (int i = 0; i < inside.Count(); i++)
		{
			bool free = true;
			foreach (vector spot : claimed)
			{
				if (vector.Distance(spot, inside[i]) < 1.5)
				{
					free = false;
					break;
				}
			}
			if (!free || CutOff(hold.m_Building, inside[i]))
				continue;
			float cost = PostCost(body.GetOrigin(), inside[i]);
			if (views[i] == vector.Zero)
				cost += 4;
			if (cost < bestCost)
			{
				bestCost = cost;
				best = inside[i];
				look = PostFacing(inside[i], inside, views, middle);
			}
		}
		return best;
	}

	static bool Covered(vector point, float top)
	{
		TraceParam trace = new TraceParam();
		trace.Start = point + Vector(0, 1.0, 0);
		trace.End = Vector(point[0], top, point[2]);
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		float fraction = GetGame().GetWorld().TraceMove(trace, null);
		return fraction < 1;
	}

	// Navmesh samples can land on low roof/attic surfaces that do not fit a standing soldier.
	static bool StandingRoom(vector point, IEntity walker)
	{
		TraceParam trace = new TraceParam();
		trace.Start = point + Vector(0, 0.15, 0);
		trace.End = point + Vector(0, 1.8, 0);
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.Exclude = walker;
		return GetGame().GetWorld().TraceMove(trace, null) >= 1;
	}

	// A roof overhead alone also accepts grass underneath overhangs/foundations. Require an
	// actual, reasonably level floor beneath the sample. Bare-ground/separate-floor rooms must
	// be enclosed by this building, so barns work while grass beneath an overhang is rejected.
	// 'structural' (choosing posts) also rejects the tops of beds, tables and cupboards: the
	// navmesh has small islands on furniture that nobody can walk onto.
	static bool BuildingFloor(vector point, IEntity walker, IEntity building, bool structural = false)
	{
		TraceParam trace = new TraceParam();
		trace.Start = point + Vector(0, 0.4, 0);
		trace.End = point - Vector(0, 0.5, 0);
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.Exclude = walker;
		if (GetGame().GetWorld().TraceMove(trace, null) >= 1 || trace.TraceNorm[1] < 0.9)
			return false;
		IEntity floor = trace.TraceEnt;
		if (structural && floor && floor != building && Furniture(floor))
		{
			s_iFurniture++;
			return false;
		}
		while (floor)
		{
			if (floor == building)
				return true;
			floor = floor.GetParent();
		}
		vector mins, maxs;
		building.GetWorldBounds(mins, maxs);
		array<vector> ends = {Vector(mins[0] - 1, point[1] + 1, point[2]), Vector(maxs[0] + 1, point[1] + 1, point[2]), Vector(point[0], point[1] + 1, mins[2] - 1), Vector(point[0], point[1] + 1, maxs[2] + 1)};
		int walls;
		foreach (vector end : ends)
		{
			TraceParam wall = new TraceParam();
			wall.Start = point + Vector(0, 1, 0);
			wall.End = end;
			wall.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
			wall.Exclude = walker;
			if (GetGame().GetWorld().TraceMove(wall, null) >= 1)
				continue;
			IEntity entity = wall.TraceEnt;
			while (entity && entity != building)
				entity = entity.GetParent();
			if (entity)
				walls++;
		}
		return walls >= 3;
	}

	// A small separate object (bed, table, cupboard), not a floor slab of the building.
	static bool Furniture(IEntity entity)
	{
		vector mins, maxs;
		entity.GetWorldBounds(mins, maxs);
		return maxs[0] - mins[0] < 3.5 && maxs[2] - mins[2] < 3.5;
	}

	// Up to 'count' points in all, spread out over the set (greedy farthest-first); points
	// already in 'picked' count as chosen.
	static void PickSpread(array<vector> from, int count, notnull array<vector> picked)
	{
		if (from.IsEmpty())
			return;
		int start = picked.Count();
		if (picked.IsEmpty())
			picked.Insert(from[0]);
		while (picked.Count() < count && picked.Count() - start < from.Count())
		{
			vector best = from[0];
			float bestDistance = -1;
			foreach (vector candidate : from)
			{
				float nearest = float.MAX;
				foreach (vector chosen : picked)
				{
					float d = vector.Distance(candidate, chosen);
					if (d < nearest)
						nearest = d;
				}
				if (nearest > bestDistance)
				{
					bestDistance = nearest;
					best = candidate;
				}
			}
			if (bestDistance <= 0.5)
				break;
			picked.Insert(best);
		}
	}

	static void SortByDistance(array<vector> points, vector from)
	{
		for (int i = 1; i < points.Count(); i++)
		{
			for (int j = i; j > 0; j--)
			{
				if (vector.Distance(points[j], from) >= vector.Distance(points[j - 1], from))
					break;
				vector swap = points[j];
				points.Set(j, points[j - 1]);
				points.Set(j - 1, swap);
			}
		}
	}
}
