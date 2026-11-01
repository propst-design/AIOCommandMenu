// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). Licensed under the GNU GPL v3 or later (see LICENSE).

// Vehicles > Repair that vehicle: the nearest man on foot carrying a repair tool (an item with the
// game's repair support station, e.g. the wrench kit) walks to the vehicle, crouches by it and
// repairs it step by step with that tool's own numbers: damage healed per use and the health cap a
// field repair can reach. Destroyed vehicles are beyond a field repair, as for a player. Server only.

// Read access to the tool's repair settings (protected in the game's class).
modded class SCR_BaseDamageHealSupportStationComponent
{
	float AIO_MaxHealScaled()
	{
		return GetMaxHealScaled();
	}

	float AIO_HealPerUse()
	{
		return m_iDamageHealedEachExecution;
	}
}

class AIO_RepairJob
{
	AIAgent m_Agent;
	IEntity m_Vehicle;
	IEntity m_Tool;
	int m_iPlayerId;
	int m_iDeadline;      // tick count
	int m_iNextUse;       // tick count while working
	bool m_bWorking;
	vector m_vVehicleAt;  // where the vehicle stood when he started working
	vector m_vStand;
	bool m_bHadHold;
	vector m_vSpot;
	IEntity m_Anchor;
	vector m_vOffset;
	bool m_bEnding;
}

class AIO_Repair
{
	static const int WALK_MS = 120000;
	static const int WORK_MS = 240000;
	static const int USE_MS = 2500;   // about one player repair action
	static const float REACH = 2.0;

	protected static ref array<ref AIO_RepairJob> s_aJobs = {};

	// Mission end (AIO_Game.c): the jobs belong to the world being torn down.
	static void Cleanup()
	{
		s_aJobs.Clear();
	}

	static bool IsOrder(int order)
	{
		return order == AIO_EOrder.VEH_REPAIR;
	}

	static string Order(array<IEntity> bodies, IEntity looked, IEntity player, vector point, int playerId)
	{
		IEntity vehicle = AIO_Vehicles.TargetVehicle(looked, player, point);
		if (!vehicle)
			return "Look at the vehicle to repair";
		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(vehicle);
		if (!damage)
			return "That can't be repaired";
		if (damage.GetState() == EDamageState.DESTROYED)
			return "That vehicle is destroyed: beyond a field repair";
		IEntity engineer = null;
		IEntity tool = null;
		float best = 1000000;
		foreach (IEntity body : bodies)
		{
			ChimeraCharacter character = ChimeraCharacter.Cast(body);
			if (!character || !AIO_Server.IsAlive(body) || character.IsInVehicle() || IsRepairing(AIO_Tasks.AgentOf(body)))
				continue;
			IEntity hisTool = FindTool(body);
			if (!hisTool)
				continue;
			float distance = vector.Distance(body.GetOrigin(), vehicle.GetOrigin());
			if (distance < best)
			{
				best = distance;
				engineer = body;
				tool = hisTool;
			}
		}
		if (!engineer)
			return "Nobody there carries a repair kit";
		if (best > 400)
			return "The nearest soldier with a repair kit is too far away";
		if (ToRepair(vehicle, tool) <= 0)
			return "That vehicle needs no repair a field kit can do";
		Start(engineer, vehicle, tool, playerId);
		return string.Format("%1 repairing that vehicle", AIO_Server.NameOf(engineer));
	}

	static IEntity FindTool(IEntity body)
	{
		SCR_InventoryStorageManagerComponent inventory = AIO_Supplies.InventoryOf(body);
		if (!inventory)
			return null;
		array<IEntity> items = {};
		inventory.GetItems(items);
		foreach (IEntity item : items)
		{
			if (Station(item))
				return item;
		}
		return null;
	}

	static SCR_RepairSupportStationComponent Station(IEntity tool)
	{
		if (!tool)
			return null;
		return SCR_RepairSupportStationComponent.Cast(tool.FindComponent(SCR_RepairSupportStationComponent));
	}

	// Damage left that this tool can still repair (up to its cap on each hit zone).
	static float ToRepair(IEntity vehicle, IEntity tool)
	{
		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(vehicle);
		SCR_RepairSupportStationComponent station = Station(tool);
		if (!damage || !station)
			return 0;
		array<HitZone> zones = {};
		damage.GetAllHitZones(zones);
		return damage.GetHitZonesDamage(station.AIO_MaxHealScaled(), zones);
	}

	static bool IsRepairing(AIAgent agent)
	{
		foreach (AIO_RepairJob job : s_aJobs)
		{
			if (job.m_Agent == agent)
				return true;
		}
		return false;
	}

	static string Label(AIAgent agent)
	{
		foreach (AIO_RepairJob job : s_aJobs)
		{
			if (job.m_Agent == agent)
				return "Repairing";
		}
		return "";
	}

	static void Cancel(AIAgent agent)
	{
		for (int i = s_aJobs.Count() - 1; i >= 0; i--)
		{
			if (s_aJobs[i].m_Agent == agent && !s_aJobs[i].m_bEnding)
			{
				AIO_Tasks.SetStance(agent, -1);
				s_aJobs.Remove(i);
			}
		}
	}

	protected static void Start(IEntity body, IEntity vehicle, IEntity tool, int playerId)
	{
		AIAgent agent = AIO_Tasks.AgentOf(body);
		SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(agent);
		if (!utility)
			return;
		AIO_RepairJob job = new AIO_RepairJob();
		job.m_Agent = agent;
		job.m_Vehicle = vehicle;
		job.m_Tool = tool;
		job.m_iPlayerId = playerId;
		job.m_iDeadline = System.GetTickCount() + WALK_MS;
		AIO_HoldBehavior hold = AIO_Tasks.ActiveHold(utility);
		if (hold)
		{
			job.m_bHadHold = true;
			job.m_vSpot = hold.m_vSpot;
			job.m_Anchor = hold.m_Anchor;
			job.m_vOffset = hold.m_vOffset;
		}
		AIO_Tasks.Release(body);
		job.m_vStand = StandSpot(vehicle, body.GetOrigin());
		utility.AddAction(new AIO_MoveBehavior(utility, null, job.m_vStand, radius: 1.0));
		utility.AddAction(new AIO_HoldBehavior(utility, null, job.m_vStand));
		s_aJobs.Insert(job);
		AIO_Tasks.Watch();
	}

	// Beside the vehicle on his side, just clear of its bounds.
	static vector StandSpot(IEntity vehicle, vector from)
	{
		vector mins, maxs;
		vehicle.GetWorldBounds(mins, maxs);
		vector centre = (mins + maxs) * 0.5;
		float half = Math.Max(maxs[0] - mins[0], maxs[2] - mins[2]) * 0.5;
		vector away = from - centre;
		away[1] = 0;
		if (away.Length() < 0.1)
			away = "1 0 0";
		away.Normalize();
		// Bounds are axis-aligned and generous; stand a little inside the longer half-size.
		vector stand = centre + away * (half * 0.8 + 0.8);
		stand[1] = GetGame().GetWorld().GetSurfaceY(stand[0], stand[2]);
		return stand;
	}

	// Called by the AIO_Tasks watchdog every second.
	static void Tick()
	{
		int now = System.GetTickCount();
		for (int i = s_aJobs.Count() - 1; i >= 0; i--)
		{
			AIO_RepairJob job = s_aJobs[i];
			IEntity body = null;
			if (job.m_Agent)
				body = job.m_Agent.GetControlledEntity();
			if (!body || !AIO_Server.IsAlive(body))
			{
				s_aJobs.Remove(i);
				continue;
			}
			SCR_DamageManagerComponent damage = null;
			if (job.m_Vehicle)
				damage = SCR_DamageManagerComponent.GetDamageManager(job.m_Vehicle);
			if (!damage || damage.GetState() == EDamageState.DESTROYED)
			{
				End(job, body, "can't repair it any more: the vehicle is gone or destroyed");
				s_aJobs.Remove(i);
				continue;
			}
			if (!AIO_Explosives.Carries(body, job.m_Tool))
			{
				End(job, body, "no longer has his repair kit");
				s_aJobs.Remove(i);
				continue;
			}
			if (now > job.m_iDeadline)
			{
				string why = "couldn't reach the vehicle";
				if (job.m_bWorking)
					why = "ran out of time on the repair";
				End(job, body, why);
				s_aJobs.Remove(i);
				continue;
			}
			if (!job.m_bWorking)
			{
				if (vector.DistanceXZ(body.GetOrigin(), job.m_vStand) > REACH)
				{
					// The vehicle moved: follow it.
					if (vector.DistanceXZ(StandSpot(job.m_Vehicle, body.GetOrigin()), job.m_vStand) > 4)
						Restand(job, body);
					continue;
				}
				job.m_bWorking = true;
				job.m_vVehicleAt = job.m_Vehicle.GetOrigin();
				job.m_iDeadline = now + WORK_MS;
				job.m_iNextUse = now + USE_MS;
				AIO_Tasks.SetStance(job.m_Agent, ECharacterStance.CROUCH);
				continue;
			}
			if (vector.Distance(job.m_Vehicle.GetOrigin(), job.m_vVehicleAt) > 3)
			{
				End(job, body, "stopped: the vehicle drove off");
				s_aJobs.Remove(i);
				continue;
			}
			if (now < job.m_iNextUse)
				continue;
			job.m_iNextUse = now + USE_MS;
			SCR_RepairSupportStationComponent station = Station(job.m_Tool);
			array<HitZone> zones = {};
			damage.GetAllHitZones(zones);
			damage.HealHitZones(station.AIO_HealPerUse(), true, station.AIO_MaxHealScaled(), zones);
			if (damage.GetHitZonesDamage(station.AIO_MaxHealScaled(), zones) > 0.01)
				continue;
			int percent = Math.Round(damage.GetHealthScaled() * 100);
			End(job, body, "");
			string done = string.Format("%1: vehicle repaired (%2%)", AIO_Server.NameOf(body), percent);
			if (station.AIO_MaxHealScaled() < 1)
				done = string.Format("%1: vehicle repaired as far as a field kit allows (%2%)", AIO_Server.NameOf(body), percent);
			AIO_Supplies.Report(job.m_iPlayerId, done);
			Print("[AIO] REPAIR " + done);
			s_aJobs.Remove(i);
		}
	}

	protected static void Restand(AIO_RepairJob job, IEntity body)
	{
		SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(job.m_Agent);
		if (!utility)
			return;
		job.m_bEnding = true; // Release would cancel this job
		AIO_Tasks.Release(body);
		job.m_bEnding = false;
		job.m_vStand = StandSpot(job.m_Vehicle, body.GetOrigin());
		utility.AddAction(new AIO_MoveBehavior(utility, null, job.m_vStand, radius: 1.0));
		utility.AddAction(new AIO_HoldBehavior(utility, null, job.m_vStand));
	}

	protected static void End(AIO_RepairJob job, IEntity body, string problem)
	{
		job.m_bEnding = true;
		AIO_Tasks.SetStance(job.m_Agent, -1);
		AIO_Tasks.Rejoin(body, GetGame().GetPlayerManager().GetPlayerControlledEntity(job.m_iPlayerId));
		if (job.m_bHadHold)
		{
			if (job.m_Anchor && AIO_Server.IsAlive(job.m_Anchor))
				AIO_Tasks.KeepOffset(body, job.m_Anchor, job.m_vOffset);
			else if (!job.m_Anchor)
				AIO_Tasks.MoveTo(body, job.m_vSpot);
		}
		if (!problem.IsEmpty())
		{
			AIO_Supplies.Report(job.m_iPlayerId, string.Format("%1 %2", AIO_Server.NameOf(body), problem));
			Print(string.Format("[AIO] REPAIR %1 %2", AIO_Server.NameOf(body), problem));
		}
	}
}
