// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). Licensed under the GNU GPL v3 or later (see LICENSE).

// More > Fire mission: a squad mortar fires a number of rounds at the point (map cursor when the map
// is open). Uses the game's own mortar behaviour (SCR_AIStaticArtilleryBehavior): the gunner aims,
// fetches shells from a nearby ammo box or arsenal and fires; AiO counts the rounds and stops him
// at the ordered number. The gunner is a recipient already on a mortar, else the recipient nearest
// to a free mortar near the player (the behaviour takes the seat). Server only.

class AIO_FireMission
{
	AIAgent m_Agent;
	IEntity m_Mortar;
	ref SCR_AIStaticArtilleryBehavior m_Behavior;
	int m_iRounds;
	int m_iPlayerId;
	int m_iDeadline;  // tick count
}

class AIO_FireSupport
{
	static const float MORTAR_SEARCH = 100;
	static const int MISSION_MS = 240000;

	protected static ref array<ref AIO_FireMission> s_aMissions = {};
	protected static ref array<IEntity> s_aQueryHits;

	// Mission end (AIO_Game.c): the jobs belong to the world being torn down.
	static void Cleanup()
	{
		s_aMissions.Clear();
	}
	static int s_iLastFired = -1; // rounds of the last mission that ended (tests)

	static bool IsOrder(int order)
	{
		return order == AIO_EOrder.FIRE_HE_3 || order == AIO_EOrder.FIRE_HE_6 || order == AIO_EOrder.FIRE_SMOKE
			|| order == AIO_EOrder.FIRE_ILLUM || order == AIO_EOrder.FIRE_CEASE;
	}

	static string Order(array<IEntity> bodies, IEntity player, int order, vector point, int playerId)
	{
		if (order == AIO_EOrder.FIRE_CEASE)
			return Cease(playerId);
		if (point == vector.Zero)
			return "Point at the target, or open the map and put the cursor on it";
		int rounds = 3;
		SCR_EAIArtilleryAmmoType ammo = SCR_EAIArtilleryAmmoType.HIGH_EXPLOSIVE;
		string what = "HE";
		if (order == AIO_EOrder.FIRE_HE_6)
			rounds = 6;
		else if (order == AIO_EOrder.FIRE_SMOKE)
		{
			ammo = SCR_EAIArtilleryAmmoType.SMOKE;
			what = "smoke";
		}
		else if (order == AIO_EOrder.FIRE_ILLUM)
		{
			rounds = 1;
			ammo = SCR_EAIArtilleryAmmoType.ILLUMINATION;
			what = "illumination";
		}

		// A recipient already on a mortar, else the nearest recipient to a free mortar near the player.
		IEntity gunner = null;
		IEntity mortar = null;
		foreach (IEntity body : bodies)
		{
			if (!AIO_Server.IsAlive(body))
				continue;
			IEntity seat = CompartmentAccessComponent.GetVehicleIn(body);
			if (IsMortar(seat) && Gunner(seat) == body)
			{
				gunner = body;
				mortar = seat;
				break;
			}
		}
		if (!gunner)
		{
			vector around = point;
			if (player)
				around = player.GetOrigin();
			mortar = FreeMortarNear(around);
			if (!mortar)
				return "No mortar manned by the squad, and no free mortar nearby";
			float best = 1000000;
			foreach (IEntity body : bodies)
			{
				ChimeraCharacter character = ChimeraCharacter.Cast(body);
				if (!character || !AIO_Server.IsAlive(body) || character.IsInVehicle())
					continue;
				float distance = vector.Distance(body.GetOrigin(), mortar.GetOrigin());
				if (distance < best)
				{
					best = distance;
					gunner = body;
				}
			}
			if (!gunner)
				return "Nobody on foot to man the mortar";
		}
		SCR_AIStaticArtilleryVehicleUsageComponent usage = SCR_AIStaticArtilleryVehicleUsageComponent.Cast(mortar.FindComponent(SCR_AIStaticArtilleryVehicleUsageComponent));
		if (!usage || usage.GetAmmoResourceName(ammo).IsEmpty())
			return string.Format("That mortar has no %1 rounds", what);

		AIAgent agent = AIO_Tasks.AgentOf(gunner);
		SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(agent);
		if (!utility)
			return "The gunner can't take orders";
		Stop(agent);
		// On foot he must reach the mortar himself: no AiO hold may keep him at his old spot.
		if (!CompartmentAccessComponent.GetVehicleIn(gunner))
			AIO_Tasks.Release(gunner);
		array<ref AIActionBase> old = {};
		utility.FindActionsOfType(SCR_AIStaticArtilleryBehavior, old);
		foreach (AIActionBase action : old)
			action.Fail();
		SCR_AIStaticArtilleryBehavior behavior = new SCR_AIStaticArtilleryBehavior(utility, null, mortar, point, ammo, SCR_AIActionBase.PRIORITY_LEVEL_PLAYER);
		utility.AddAction(behavior);

		AIO_FireMission mission = new AIO_FireMission();
		mission.m_Agent = agent;
		mission.m_Mortar = mortar;
		mission.m_Behavior = behavior;
		mission.m_iRounds = rounds;
		mission.m_iPlayerId = playerId;
		mission.m_iDeadline = System.GetTickCount() + MISSION_MS;
		s_aMissions.Insert(mission);
		AIO_Tasks.Watch();
		Print(string.Format("[AIO] FIRE MISSION %1 on %2: %3 x %4 at %5", AIO_Server.NameOf(gunner), mortar, rounds, what, point));
		return string.Format("%1: %2 %3 rounds, %4 m out", AIO_Server.NameOf(gunner), rounds, what, Math.Round(vector.Distance(mortar.GetOrigin(), point)));
	}

	static bool IsMortar(IEntity entity)
	{
		return entity && entity.FindComponent(SCR_AIStaticArtilleryVehicleUsageComponent);
	}

	static IEntity Gunner(IEntity mortar)
	{
		SCR_AIStaticArtilleryVehicleUsageComponent usage = SCR_AIStaticArtilleryVehicleUsageComponent.Cast(mortar.FindComponent(SCR_AIStaticArtilleryVehicleUsageComponent));
		if (!usage || !usage.GetTurretCompartmentSlot())
			return null;
		return usage.GetTurretCompartmentSlot().GetOccupant();
	}

	protected static bool QueryHit(IEntity entity)
	{
		s_aQueryHits.Insert(entity);
		return true;
	}

	static IEntity FreeMortarNear(vector point)
	{
		s_aQueryHits = {};
		GetGame().GetWorld().QueryEntitiesBySphere(point, MORTAR_SEARCH, QueryHit, null, EQueryEntitiesFlags.ALL);
		IEntity best = null;
		float bestDistance = float.MAX;
		foreach (IEntity hit : s_aQueryHits)
		{
			if (!IsMortar(hit) || Gunner(hit))
				continue;
			SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(hit);
			if (damage && damage.GetState() == EDamageState.DESTROYED)
				continue;
			float distance = vector.Distance(hit.GetOrigin(), point);
			if (distance < bestDistance)
			{
				best = hit;
				bestDistance = distance;
			}
		}
		s_aQueryHits = null;
		return best;
	}

	static string Label(AIAgent agent)
	{
		foreach (AIO_FireMission mission : s_aMissions)
		{
			if (mission.m_Agent == agent && mission.m_Behavior)
				return string.Format("Fire mission %1/%2", mission.m_Behavior.GetShotCount(), mission.m_iRounds);
		}
		return "";
	}

	protected static void Stop(AIAgent agent)
	{
		for (int i = s_aMissions.Count() - 1; i >= 0; i--)
		{
			if (s_aMissions[i].m_Agent != agent)
				continue;
			if (s_aMissions[i].m_Behavior)
				s_aMissions[i].m_Behavior.Fail();
			s_aMissions.Remove(i);
		}
	}

	static string Cease(int playerId)
	{
		int stopped;
		for (int i = s_aMissions.Count() - 1; i >= 0; i--)
		{
			if (s_aMissions[i].m_iPlayerId != playerId)
				continue;
			if (s_aMissions[i].m_Behavior)
				s_aMissions[i].m_Behavior.Fail();
			s_aMissions.Remove(i);
			stopped++;
		}
		if (stopped == 0)
			return "No fire mission running";
		return "Cease fire";
	}

	// Called by the AIO_Tasks watchdog every second.
	static void Tick()
	{
		int now = System.GetTickCount();
		for (int i = s_aMissions.Count() - 1; i >= 0; i--)
		{
			AIO_FireMission mission = s_aMissions[i];
			IEntity body = null;
			if (mission.m_Agent)
				body = mission.m_Agent.GetControlledEntity();
			SCR_AIStaticArtilleryBehavior behavior = mission.m_Behavior;
			int fired = 0;
			if (behavior)
				fired = behavior.GetShotCount();
			string name = "The gunner";
			if (body)
				name = AIO_Server.NameOf(body);
			if (!body || !AIO_Server.IsAlive(body))
			{
				AIO_Supplies.Report(mission.m_iPlayerId, string.Format("Fire mission stopped after %1 rounds: gunner down", fired));
				s_aMissions.Remove(i);
				continue;
			}
			if (fired >= mission.m_iRounds)
			{
				behavior.Fail();
				AIO_Supplies.Report(mission.m_iPlayerId, string.Format("%1: rounds complete (%2)", name, fired));
				Print(string.Format("[AIO] FIRE MISSION %1 complete, %2 rounds", name, fired));
				s_iLastFired = fired;
				s_aMissions.Remove(i);
				continue;
			}
			bool ended = !behavior || behavior.GetActionState() == EAIActionState.FAILED || behavior.GetActionState() == EAIActionState.COMPLETED;
			if (ended || now > mission.m_iDeadline)
			{
				if (behavior && !ended)
					behavior.Fail();
				string why = "can't fire (no shells nearby, or out of range)";
				if (now > mission.m_iDeadline)
					why = "ran out of time";
				AIO_Supplies.Report(mission.m_iPlayerId, string.Format("%1: fire mission stopped after %2 rounds, %3", name, fired, why));
				Print(string.Format("[AIO] FIRE MISSION %1 stopped after %2 rounds: %3", name, fired, why));
				s_iLastFired = fired;
				s_aMissions.Remove(i);
			}
		}
	}
}
