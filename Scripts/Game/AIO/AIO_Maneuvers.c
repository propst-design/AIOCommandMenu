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

	void AIO_SetCombatMode(bool set, EAIGroupCombatMode mode)
	{
		m_bAIO_HasMode = set;
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
		if (m_iAIO_KeepFormation == 1)
			return true;
		if (m_iAIO_KeepFormation == 0)
			return false;
		return super.ShouldKeepFormation();
	}
}

// Bounding (Bound forward) and peeling (Peel back): two elements move in turns, one covers.
class AIO_Bound
{
	ref array<AIAgent> m_aA = {};
	ref array<AIAgent> m_aB = {};
	vector m_vDir;        // direction of travel (flat, normalised)
	vector m_vTarget;     // Bound forward: where to end up; Peel back: unused
	bool m_bPeel;
	float m_fStep;
	int m_iStepsLeft;
	bool m_bAMoving;
	int m_iStepStarted;   // tick count
	ref array<vector> m_aSpots = {}; // spots of the moving element
	int m_iPlayerId;
}

class AIO_Maneuvers
{
	static const float BOUND_STEP = 30;
	static const float PEEL_STEP = 25;
	static const int STEP_TIMEOUT_MS = 35000;
	static const float FLANK_WIDTH = 45;
	static const vector PROBE_EXTENT = "1.2 1.5 1.2";

	protected static ref array<ref AIO_Bound> s_aBounds = {};
	protected static bool s_bIssuing; // our own orders must not end our own maneuvers

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
			|| order == AIO_EOrder.DISENGAGE;
	}

	// bodies: the men (never empty). point: where the player looked (zero if none).
	static string Attack(array<IEntity> bodies, IEntity player, int order, vector point, int playerId)
	{
		vector center = Centroid(bodies);
		if (order == AIO_EOrder.ENGAGE_AT_WILL || order == AIO_EOrder.DISENGAGE)
		{
			int keep = 0;
			if (order == AIO_EOrder.DISENGAGE)
				keep = 1;
			foreach (IEntity engaged : bodies)
				SetEngage(engaged, keep);
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
			return StartBound(bodies, forward * -1, vector.Zero, true, playerId);
		}
		if (point == vector.Zero)
			return "Look at a spot first";
		vector dir = Flat(point - center);
		vector side = Vector(dir[2], 0, -dir[0]); // right of the direction of travel
		if (order == AIO_EOrder.BOUND)
			return StartBound(bodies, dir, point, false, playerId);
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
					sent++;
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
		return string.Format("%1 men flanking %2", sent, which);
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

	static string StartBound(array<IEntity> bodies, vector dir, vector target, bool peel, int playerId)
	{
		if (bodies.Count() < 2)
			return "Bounding needs at least two men";
		AIO_Bound bound = new AIO_Bound();
		// Split by colour team when the men are in two or more teams, else alternate.
		int firstTeam = -1;
		foreach (IEntity body : bodies)
		{
			int team = TeamOf(body);
			if (team > 0 && (firstTeam < 0 || team < firstTeam))
				firstTeam = team;
		}
		bool byTeam = false;
		if (firstTeam > 0)
		{
			foreach (IEntity other : bodies)
			{
				if (TeamOf(other) != firstTeam)
					byTeam = true;
			}
		}
		for (int i = 0; i < bodies.Count(); i++)
		{
			AIAgent agent = AIO_Tasks.AgentOf(bodies[i]);
			if (!agent)
				continue;
			bool inA;
			if (byTeam)
				inA = TeamOf(bodies[i]) == firstTeam;
			else
				inA = i % 2 == 0;
			if (inA)
				bound.m_aA.Insert(agent);
			else
				bound.m_aB.Insert(agent);
		}
		if (bound.m_aA.IsEmpty() || bound.m_aB.IsEmpty())
			return "Bounding needs two groups of men";
		bound.m_vDir = dir;
		bound.m_vTarget = target;
		bound.m_bPeel = peel;
		bound.m_iPlayerId = playerId;
		if (peel)
		{
			bound.m_fStep = PEEL_STEP;
			bound.m_iStepsLeft = 4;
		}
		else
		{
			bound.m_fStep = BOUND_STEP;
			bound.m_iStepsLeft = 12;
		}
		// Everyone stops and covers; the first element moves.
		foreach (IEntity holder : bodies)
		{
			Leave(AIO_Tasks.AgentOf(holder));
		}
		s_bIssuing = true;
		foreach (IEntity coverer : bodies)
			AIO_Tasks.HoldHere(coverer);
		s_bIssuing = false;
		s_aBounds.Insert(bound);
		bound.m_bAMoving = false; // NextStep flips it to A
		NextStep(bound);
		AIO_Tasks.Watch();
		string kind = "bounding";
		if (peel)
			kind = "peeling back";
		string how = "alternating men";
		if (byTeam)
			how = "by team";
		return string.Format("%1 men %2 in two groups (%3)", bodies.Count(), kind, how);
	}

	static void Bodies(array<AIAgent> agents, notnull array<IEntity> bodies)
	{
		foreach (AIAgent agent : agents)
		{
			if (agent && agent.GetControlledEntity() && AIO_Server.IsAlive(agent.GetControlledEntity()))
				bodies.Insert(agent.GetControlledEntity());
		}
	}

	// The other element moves now. Returns false when the maneuver is over.
	protected static bool NextStep(AIO_Bound bound)
	{
		bound.m_bAMoving = !bound.m_bAMoving;
		array<IEntity> movers = {};
		if (bound.m_bAMoving)
			Bodies(bound.m_aA, movers);
		else
			Bodies(bound.m_aB, movers);
		if (movers.IsEmpty() || bound.m_iStepsLeft <= 0)
			return false;
		vector center = Centroid(movers);
		float step = bound.m_fStep;
		bool last = false;
		if (!bound.m_bPeel)
		{
			float left = vector.DistanceXZ(center, bound.m_vTarget);
			if (left <= step + 5)
			{
				step = left;
				last = true;
			}
		}
		vector goal = center + bound.m_vDir * step;
		vector across = Vector(bound.m_vDir[2], 0, -bound.m_vDir[0]);
		bound.m_aSpots.Clear();
		s_bIssuing = true;
		for (int i = 0; i < movers.Count(); i++)
		{
			vector spot = LineSpot(goal, across, i, movers.Count());
			bound.m_aSpots.Insert(spot);
			AIO_Tasks.MoveTo(movers[i], spot);
		}
		s_bIssuing = false;
		bound.m_iStepsLeft--;
		if (last)
		{
			if (bound.m_iStepsLeft > 1)
				bound.m_iStepsLeft = 1; // the other element closes up once more
		}
		bound.m_iStepStarted = System.GetTickCount();
		return true;
	}

	protected static bool StepDone(AIO_Bound bound)
	{
		if (System.GetTickCount() - bound.m_iStepStarted > STEP_TIMEOUT_MS)
			return true;
		array<IEntity> movers = {};
		if (bound.m_bAMoving)
			Bodies(bound.m_aA, movers);
		else
			Bodies(bound.m_aB, movers);
		for (int i = 0; i < movers.Count() && i < bound.m_aSpots.Count(); i++)
		{
			if (vector.DistanceXZ(movers[i].GetOrigin(), bound.m_aSpots[i]) > 4)
				return false;
		}
		return true;
	}

	// A new order for this man takes him out of any bound.
	static void Leave(AIAgent agent)
	{
		if (s_bIssuing || !agent)
			return;
		StopLead(agent);
		foreach (AIO_Bound bound : s_aBounds)
		{
			bound.m_aA.RemoveItem(agent);
			bound.m_aB.RemoveItem(agent);
		}
	}

	static bool IsBounding(AIAgent agent)
	{
		foreach (AIO_Bound bound : s_aBounds)
		{
			if (bound.m_aA.Contains(agent) || bound.m_aB.Contains(agent))
				return true;
		}
		return false;
	}

	static void Tick()
	{
		TickLead();
		for (int i = s_aBounds.Count() - 1; i >= 0; i--)
		{
			AIO_Bound bound = s_aBounds[i];
			array<IEntity> all = {};
			Bodies(bound.m_aA, all);
			Bodies(bound.m_aB, all);
			if (all.IsEmpty())
			{
				s_aBounds.Remove(i);
				continue;
			}
			if (!StepDone(bound))
				continue;
			if (!NextStep(bound))
			{
				string kind = "Bound";
				if (bound.m_bPeel)
					kind = "Peel back";
				AIO_Supplies.Report(bound.m_iPlayerId, kind + " complete");
				Print("[AIO] " + kind + " complete");
				s_aBounds.Remove(i);
			}
		}
	}

	// ---------------------------------------------------------------- defense

	static bool IsDefenseOrder(int order)
	{
		return order == AIO_EOrder.DEF_GARRISON || order == AIO_EOrder.DEF_CLEAR || order == AIO_EOrder.DEF_360 || order == AIO_EOrder.DEF_FORTIFY;
	}

	static string Defense(array<IEntity> bodies, IEntity player, int order, vector point, IEntity looked)
	{
		if (order == AIO_EOrder.DEF_360)
		{
			if (!player)
				return "You need to be on foot in the world to give that order";
			vector center = player.GetOrigin();
			int count = bodies.Count();
			float radius = Math.Clamp(count * 1.5, 5, 12);
			for (int i = 0; i < count; i++)
			{
				float angle = Math.PI2 * i / count;
				vector outDir = Vector(Math.Sin(angle), 0, Math.Cos(angle));
				vector spot = Ground(center + outDir * radius);
				HoldFacing(bodies[i], spot, center + outDir * 80, true);
			}
			return string.Format("%1 men in all-round defense", count);
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
		InteriorPoints(building, bodies[0], inside);
		if (inside.IsEmpty())
			return "No way into that building";
		vector mins, maxs;
		building.GetWorldBounds(mins, maxs);
		vector middle = (mins + maxs) * 0.5;
		if (order == AIO_EOrder.DEF_GARRISON)
		{
			// Spread over the building: farthest-apart points first.
			array<vector> posts = {};
			PickSpread(inside, bodies.Count(), posts);
			for (int g = 0; g < bodies.Count(); g++)
			{
				vector post = posts[g % posts.Count()];
				vector outward = Flat(post - middle);
				HoldFacing(bodies[g], post, post + outward * 60, false);
			}
			return string.Format("%1 men garrisoning (%2 positions)", bodies.Count(), posts.Count());
		}
		// Clear: everyone walks through the building room by room, then holds inside.
		array<vector> sweep = {};
		PickSpread(inside, Math.Min(inside.Count(), 6), sweep);
		// order the sweep from the men's side of the building inward
		vector approach = Centroid(bodies);
		SortByDistance(sweep, approach);
		for (int c = 0; c < bodies.Count(); c++)
		{
			array<vector> legs = {};
			foreach (vector room : sweep)
				legs.Insert(room + Vector(c * 0.6, 0, 0));
			Route(bodies[c], legs);
			SetEngage(bodies[c], 0);
		}
		return string.Format("%1 men clearing the building (%2 rooms)", bodies.Count(), sweep.Count());
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

	// The building the player means: the top entity of what he looked at, if it is big enough.
	static IEntity BuildingOf(IEntity looked)
	{
		if (!looked)
			return null;
		IEntity top = looked.GetRootParent();
		if (!top)
			top = looked;
		if (ChimeraCharacter.Cast(top) || top.FindComponent(InventoryItemComponent) || Vehicle.Cast(top))
			return null;
		vector mins, maxs;
		top.GetWorldBounds(mins, maxs);
		vector size = maxs - mins;
		if (size[0] < 4 || size[2] < 4 || size[1] < 2.5)
			return null;
		return top;
	}

	// Standable points inside the building: a grid over its footprint on each floor, snapped to
	// the navmesh, kept when they stay inside the walls.
	static void InteriorPoints(IEntity building, IEntity walker, notnull array<vector> points)
	{
		AIPathfindingComponent pathfinding = AIPathfindingComponent.Cast(walker.FindComponent(AIPathfindingComponent));
		if (!pathfinding)
			return;
		vector mins, maxs;
		building.GetWorldBounds(mins, maxs);
		float inset = 1.0;
		for (float y = mins[1] + 0.5; y < maxs[1] - 1.5; y += 3.0)
		{
			for (float x = mins[0] + inset; x <= maxs[0] - inset; x += 2.5)
			{
				for (float z = mins[2] + inset; z <= maxs[2] - inset; z += 2.5)
				{
					vector probe = Vector(x, y, z);
					vector corrected;
					if (!pathfinding.GetClosestPositionOnNavmesh(probe, PROBE_EXTENT, corrected))
						continue;
					if (corrected[0] < mins[0] + 0.3 || corrected[0] > maxs[0] - 0.3 || corrected[2] < mins[2] + 0.3 || corrected[2] > maxs[2] - 0.3)
						continue;
					// Under a roof: something solid above (rules out yards inside the bounds).
					if (!Covered(corrected, maxs[1] + 1))
						continue;
					bool near = false;
					foreach (vector existing : points)
					{
						if (vector.Distance(existing, corrected) < 2.0)
						{
							near = true;
							break;
						}
					}
					if (!near)
						points.Insert(corrected);
					if (points.Count() >= 40)
						return;
				}
			}
		}
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

	// 'count' points spread out over the set (greedy farthest-first).
	static void PickSpread(array<vector> from, int count, notnull array<vector> picked)
	{
		if (from.IsEmpty())
			return;
		picked.Insert(from[0]);
		while (picked.Count() < count && picked.Count() < from.Count())
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
