// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). Licensed under the GNU GPL v3 or later (see LICENSE).

// Supplies > Explosives: a soldier carrying a demolition charge walks to what the player looks at,
// places it (stuck to a vehicle, which then carries it, or on a wall or the ground) and arms it.
// Server only. Uses the game's own charge items: the charge leaves his inventory the way a player's
// does (placement transform, then removal from storage), the timed fuze or his own detonator arm it
// and he is the charge's user (kill credit).
// - Timed: 30 s fuze; he runs 35 m away and returns to his previous orders once it has gone off.
// - On my order: wired to the detonator he carries; Detonate fires every such charge whose placer
//   is alive, still has that detonator and is within its range.
// - Lay mine: a man carrying a mine puts it on the ground at the point and arms its own trigger.
// - Disarm mines: the nearest man walks to each armed mine within DISARM_RADIUS of the point, works
//   on it for a few seconds and switches its trigger off (the mine stays where it is, harmless).

class AIO_Demolition
{
	AIAgent m_Agent;
	int m_iKind;               // AIO_Explosives.KIND_*
	IEntity m_Charge;          // charge or mine, in his inventory until placed
	ref array<IEntity> m_aMines = {}; // disarm: mines still to do, nearest first
	int m_iDisarmed;
	IEntity m_Detonator;       // remote only
	IEntity m_Target;          // vehicle the charge sticks to (null: wall or ground)
	vector m_vPoint;
	bool m_bRemote;
	int m_iPlayerId;
	int m_iDeadline;           // tick count
	int m_iStage;              // 0 walking there, 1 placing (inventory operation running), 2 retreating, 3 disarming
	vector m_vRetreat;
	vector m_aPlaced[4];       // world transform the item is placed with
	// what he was doing before (restored afterwards)
	bool m_bHadHold;
	vector m_vSpot;
	IEntity m_Anchor;
	vector m_vOffset;
	bool m_bEnding;
}

// A placed charge waiting for the player's Detonate.
class AIO_RemoteCharge
{
	IEntity m_Charge;
	IEntity m_Placer;
	IEntity m_Detonator;
	int m_iPlayerId;
}

// Finishes the placement once the charge has left his inventory (inventory operations can be delayed).
class AIO_ChargePlacedCallback : ScriptedInventoryOperationCallback
{
	ref AIO_Demolition m_Job;

	protected override void OnComplete()
	{
		AIO_Explosives.Placed(m_Job, true);
	}

	protected override void OnFailed()
	{
		AIO_Explosives.Placed(m_Job, false);
	}
}

class AIO_Explosives
{
	static const float REACH = 2.2;
	static const float RETREAT = 35;
	static const float FUZE_SECONDS = 30;
	static const int WALK_MS = 120000;
	static const int RETREAT_MS = 45000;
	static const float DISARM_RADIUS = 10;
	static const int DISARM_MS = 6000;
	static const int KIND_TIMED = 0;
	static const int KIND_REMOTE = 1;
	static const int KIND_MINE = 2;
	static const int KIND_DISARM = 3;

	protected static ref array<ref AIO_Demolition> s_aJobs = {};
	protected static ref array<ref AIO_RemoteCharge> s_aRemote = {};

	static bool IsOrder(int order)
	{
		return order == AIO_EOrder.EXP_PLACE_TIMED || order == AIO_EOrder.EXP_PLACE_REMOTE || order == AIO_EOrder.EXP_DETONATE
			|| order == AIO_EOrder.EXP_LAY_MINE || order == AIO_EOrder.EXP_DISARM_MINES;
	}

	static string Order(array<IEntity> bodies, IEntity player, int order, vector point, IEntity looked, int playerId)
	{
		if (order == AIO_EOrder.EXP_DETONATE)
			return Detonate(playerId);
		if (point == vector.Zero)
			return "Look at the vehicle, wall or spot for that";
		if (order == AIO_EOrder.EXP_LAY_MINE)
			return LayMine(bodies, point, playerId);
		if (order == AIO_EOrder.EXP_DISARM_MINES)
			return DisarmMines(bodies, point, playerId);
		bool remote = order == AIO_EOrder.EXP_PLACE_REMOTE;
		// The nearest man on foot who carries what the job needs (and isn't placing one already).
		IEntity placer = null;
		IEntity charge = null;
		IEntity detonator = null;
		float best = 1000000;
		bool anyCharge = false;
		foreach (IEntity body : bodies)
		{
			ChimeraCharacter character = ChimeraCharacter.Cast(body);
			if (!character || !AIO_Server.IsAlive(body) || character.IsInVehicle() || IsPlacing(AIO_Tasks.AgentOf(body)))
				continue;
			IEntity hisCharge = FindCharge(body);
			if (!hisCharge)
				continue;
			anyCharge = true;
			IEntity hisDetonator = null;
			if (remote)
			{
				hisDetonator = FindDetonator(body);
				if (!hisDetonator)
					continue;
			}
			float distance = vector.Distance(body.GetOrigin(), point);
			if (distance < best)
			{
				best = distance;
				placer = body;
				charge = hisCharge;
				detonator = hisDetonator;
			}
		}
		if (!placer)
		{
			if (remote && anyCharge)
				return "Nobody with a charge also carries a detonator";
			return "Nobody there carries a demolition charge";
		}
		if (best > 400)
			return "The nearest soldier with a charge is too far away";
		IEntity vehicle = AIO_Vehicles.VehicleRoot(looked);
		int kind = KIND_TIMED;
		if (remote)
			kind = KIND_REMOTE;
		if (Start(placer, kind, charge, detonator, vehicle, point, playerId))
		{
			string where = "there";
			if (vehicle)
				where = "on that vehicle";
			string how = string.Format("%1 s timer", FUZE_SECONDS);
			if (remote)
				how = "detonate on your order";
			return string.Format("%1 placing a charge %2 (%3)", AIO_Server.NameOf(placer), where, how);
		}
		return "Couldn't send anyone to place the charge";
	}

	static string LayMine(array<IEntity> bodies, vector point, int playerId)
	{
		IEntity layer = null;
		IEntity mine = null;
		float best = 1000000;
		foreach (IEntity body : bodies)
		{
			if (!Available(body))
				continue;
			IEntity hisMine = FindMine(body);
			if (!hisMine)
				continue;
			float distance = vector.Distance(body.GetOrigin(), point);
			if (distance < best)
			{
				best = distance;
				layer = body;
				mine = hisMine;
			}
		}
		if (!layer)
			return "Nobody there carries a mine";
		if (best > 400)
			return "The nearest soldier with a mine is too far away";
		if (!Start(layer, KIND_MINE, mine, null, null, point, playerId))
			return "Couldn't send anyone to lay the mine";
		return string.Format("%1 laying a mine there", AIO_Server.NameOf(layer));
	}

	static string DisarmMines(array<IEntity> bodies, vector point, int playerId)
	{
		array<IEntity> mines = {};
		ArmedMinesNear(point, DISARM_RADIUS, mines);
		if (mines.IsEmpty())
			return string.Format("No armed mines found within %1 m of that spot", DISARM_RADIUS);
		IEntity sapper = null;
		float best = 1000000;
		foreach (IEntity body : bodies)
		{
			if (!Available(body))
				continue;
			float distance = vector.Distance(body.GetOrigin(), point);
			if (distance < best)
			{
				best = distance;
				sapper = body;
			}
		}
		if (!sapper)
			return "Nobody on foot to disarm them";
		// Nearest first from where he stands, then each next one from the last.
		array<IEntity> ordered = {};
		vector from = sapper.GetOrigin();
		while (!mines.IsEmpty())
		{
			int pick = 0;
			for (int i = 1; i < mines.Count(); i++)
			{
				if (vector.Distance(mines[i].GetOrigin(), from) < vector.Distance(mines[pick].GetOrigin(), from))
					pick = i;
			}
			from = mines[pick].GetOrigin();
			ordered.Insert(mines[pick]);
			mines.Remove(pick);
		}
		if (!Start(sapper, KIND_DISARM, null, null, null, ordered[0].GetOrigin(), playerId))
			return "Couldn't send anyone to disarm them";
		AIO_Demolition job = s_aJobs[s_aJobs.Count() - 1];
		job.m_aMines.Copy(ordered);
		return string.Format("%1 disarming %2 mines", AIO_Server.NameOf(sapper), ordered.Count());
	}

	protected static bool Available(IEntity body)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(body);
		return character && AIO_Server.IsAlive(body) && !character.IsInVehicle() && !IsPlacing(AIO_Tasks.AgentOf(body));
	}

	protected static ref array<IEntity> s_aQueryHits;

	protected static bool QueryHit(IEntity entity)
	{
		s_aQueryHits.Insert(entity);
		return true;
	}

	// Mines lying in the world with their trigger armed (any side: the soldier doesn't know whose).
	static void ArmedMinesNear(vector point, float radius, notnull array<IEntity> mines)
	{
		s_aQueryHits = {};
		GetGame().GetWorld().QueryEntitiesBySphere(point, radius, QueryHit, null, EQueryEntitiesFlags.ALL);
		foreach (IEntity hit : s_aQueryHits)
		{
			if (!hit || !hit.FindComponent(SCR_MineInventoryItemComponent))
				continue;
			SCR_PressureTriggerComponent trigger = SCR_PressureTriggerComponent.Cast(hit.FindComponent(SCR_PressureTriggerComponent));
			if (trigger && trigger.IsActivated() && !hit.GetParent())
				mines.Insert(hit);
		}
		s_aQueryHits = null;
	}

	// ---------------------------------------------------------------- equipment

	static IEntity FindMine(IEntity body)
	{
		SCR_InventoryStorageManagerComponent inventory = AIO_Supplies.InventoryOf(body);
		if (!inventory)
			return null;
		array<IEntity> items = {};
		inventory.GetItems(items);
		foreach (IEntity item : items)
		{
			if (item.FindComponent(SCR_MineInventoryItemComponent) && item.FindComponent(SCR_PressureTriggerComponent))
				return item;
		}
		return null;
	}

	static IEntity FindCharge(IEntity body)
	{
		SCR_InventoryStorageManagerComponent inventory = AIO_Supplies.InventoryOf(body);
		if (!inventory)
			return null;
		array<IEntity> items = {};
		inventory.GetItems(items);
		foreach (IEntity item : items)
		{
			SCR_ExplosiveChargeComponent charge = SCR_ExplosiveChargeComponent.Cast(item.FindComponent(SCR_ExplosiveChargeComponent));
			if (charge && charge.GetUsedFuzeType() == SCR_EFuzeType.NONE && item.FindComponent(SCR_PlaceableInventoryItemComponent))
				return item;
		}
		return null;
	}

	static IEntity FindDetonator(IEntity body)
	{
		SCR_InventoryStorageManagerComponent inventory = AIO_Supplies.InventoryOf(body);
		if (!inventory)
			return null;
		array<IEntity> items = {};
		inventory.GetItems(items);
		foreach (IEntity item : items)
		{
			if (item.FindComponent(SCR_DetonatorGadgetComponent))
				return item;
		}
		return null;
	}

	static bool Carries(IEntity body, IEntity item)
	{
		SCR_InventoryStorageManagerComponent inventory = AIO_Supplies.InventoryOf(body);
		if (!inventory || !item)
			return false;
		array<IEntity> items = {};
		inventory.GetItems(items);
		return items.Contains(item);
	}

	// ---------------------------------------------------------------- the errand

	protected static bool Start(IEntity body, int kind, IEntity charge, IEntity detonator, IEntity vehicle, vector point, int playerId)
	{
		AIAgent agent = AIO_Tasks.AgentOf(body);
		SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(agent);
		if (!utility)
			return false;
		AIO_Demolition job = new AIO_Demolition();
		job.m_Agent = agent;
		job.m_iKind = kind;
		job.m_Charge = charge;
		job.m_Detonator = detonator;
		job.m_Target = vehicle;
		job.m_vPoint = point;
		job.m_bRemote = kind == KIND_REMOTE;
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
		vector stand = StandSpot(body, point);
		utility.AddAction(new AIO_MoveBehavior(utility, null, stand, radius: 1.0));
		utility.AddAction(new AIO_HoldBehavior(utility, null, stand));
		s_aJobs.Insert(job);
		AIO_Tasks.Watch();
		return true;
	}

	// On the ground 1.2 m short of the point, on his side of it.
	protected static vector StandSpot(IEntity body, vector point)
	{
		vector away = body.GetOrigin() - point;
		away[1] = 0;
		if (away.Length() < 0.1)
			away = "1 0 0";
		away.Normalize();
		vector stand = point + away * 1.2;
		stand[1] = GetGame().GetWorld().GetSurfaceY(stand[0], stand[2]);
		return stand;
	}

	static bool IsPlacing(AIAgent agent)
	{
		foreach (AIO_Demolition job : s_aJobs)
		{
			if (job.m_Agent == agent && job.m_iStage != 2)
				return true;
		}
		return false;
	}

	static string Label(AIAgent agent)
	{
		foreach (AIO_Demolition job : s_aJobs)
		{
			if (job.m_Agent != agent)
				continue;
			if (job.m_iKind == KIND_DISARM)
				return "Disarming mines";
			if (job.m_iStage == 2)
				return "Taking cover";
			if (job.m_iKind == KIND_MINE)
				return "Laying mine";
			return "Placing charge";
		}
		return "";
	}

	// A new order for the man ends his errand (a charge already placed stays armed).
	static void Cancel(AIAgent agent)
	{
		for (int i = s_aJobs.Count() - 1; i >= 0; i--)
		{
			if (s_aJobs[i].m_Agent == agent && !s_aJobs[i].m_bEnding && s_aJobs[i].m_iStage != 1)
				s_aJobs.Remove(i);
		}
	}

	// Called by the AIO_Tasks watchdog every second.
	static void Tick()
	{
		int now = System.GetTickCount();
		for (int i = s_aJobs.Count() - 1; i >= 0; i--)
		{
			AIO_Demolition job = s_aJobs[i];
			IEntity body = null;
			if (job.m_Agent)
				body = job.m_Agent.GetControlledEntity();
			if (!body || !AIO_Server.IsAlive(body))
			{
				s_aJobs.Remove(i);
				continue;
			}
			if (job.m_iStage == 0)
			{
				bool equipped = job.m_iKind == KIND_DISARM || (job.m_Charge && Carries(body, job.m_Charge));
				if (!equipped || (job.m_bRemote && !Carries(body, job.m_Detonator)))
				{
					End(job, body, "no longer has what he needs for the charge");
					s_aJobs.Remove(i);
					continue;
				}
				if (now > job.m_iDeadline)
				{
					End(job, body, "couldn't reach the spot for the charge");
					s_aJobs.Remove(i);
					continue;
				}
				vector offset = body.GetOrigin() - job.m_vPoint;
				if (vector.DistanceXZ(body.GetOrigin(), job.m_vPoint) > REACH || Math.AbsFloat(offset[1]) > 2.5)
					continue;
				if (job.m_iKind == KIND_DISARM)
				{
					job.m_iStage = 3;
					job.m_iDeadline = now + DISARM_MS;
					AIO_Tasks.SetStance(job.m_Agent, ECharacterStance.CROUCH);
					continue;
				}
				job.m_iStage = 1;
				job.m_iDeadline = now + 10000;
				if (!Place(job, body))
				{
					End(job, body, "couldn't place the charge");
					s_aJobs.Remove(i);
				}
				continue;
			}
			if (job.m_iStage == 1)
			{
				if (now > job.m_iDeadline)
				{
					End(job, body, "couldn't place the charge");
					s_aJobs.Remove(i);
				}
				continue;
			}
			if (job.m_iStage == 3)
			{
				if (now < job.m_iDeadline)
					continue;
				IEntity mine = null;
				if (!job.m_aMines.IsEmpty())
				{
					mine = job.m_aMines[0];
					job.m_aMines.Remove(0);
				}
				SCR_PressureTriggerComponent trigger = null;
				if (mine)
					trigger = SCR_PressureTriggerComponent.Cast(mine.FindComponent(SCR_PressureTriggerComponent));
				if (trigger && trigger.IsActivated())
				{
					trigger.DisarmTrigger();
					job.m_iDisarmed++;
				}
				// Mines that were set off or taken meanwhile drop out of the list.
				while (!job.m_aMines.IsEmpty() && !job.m_aMines[0])
					job.m_aMines.Remove(0);
				if (job.m_aMines.IsEmpty())
				{
					AIO_Tasks.SetStance(job.m_Agent, -1);
					End(job, body, "");
					AIO_Supplies.Report(job.m_iPlayerId, string.Format("%1: %2 mines disarmed", AIO_Server.NameOf(body), job.m_iDisarmed));
					Print(string.Format("[AIO] DEMOLITION %1 disarmed %2 mines", AIO_Server.NameOf(body), job.m_iDisarmed));
					s_aJobs.Remove(i);
					continue;
				}
				job.m_vPoint = job.m_aMines[0].GetOrigin();
				job.m_iStage = 0;
				job.m_iDeadline = now + WALK_MS;
				SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(job.m_Agent);
				job.m_bEnding = true; // Release would cancel this job
				AIO_Tasks.Release(body);
				job.m_bEnding = false;
				vector next = StandSpot(body, job.m_vPoint);
				utility.AddAction(new AIO_MoveBehavior(utility, null, next, radius: 1.0));
				utility.AddAction(new AIO_HoldBehavior(utility, null, next));
				continue;
			}
			if (job.m_iStage == 2)
			{
				bool gone = job.m_iKind != KIND_MINE && (!job.m_Charge || job.m_Charge.IsDeleted());
				if (gone || now > job.m_iDeadline)
				{
					End(job, body, "");
					s_aJobs.Remove(i);
				}
			}
		}
	}

	// Takes the charge out of his inventory at the right spot; Placed() finishes the job.
	protected static bool Place(AIO_Demolition job, IEntity body)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(body);
		SCR_InventoryStorageManagerComponent inventory = AIO_Supplies.InventoryOf(body);
		SCR_PlaceableInventoryItemComponent placeable = SCR_PlaceableInventoryItemComponent.Cast(job.m_Charge.FindComponent(SCR_PlaceableInventoryItemComponent));
		RplComponent rpl = RplComponent.Cast(body.FindComponent(RplComponent));
		if (!character || !inventory || !placeable || !rpl || !placeable.GetParentSlot())
			return false;

		// Like the player's placement: trace from his eyes through the point onto the surface.
		vector eye = character.EyePosition();
		vector dir = job.m_vPoint - eye;
		float length = dir.Length();
		dir.Normalize();
		TraceParam trace = new TraceParam();
		trace.Start = eye;
		trace.End = eye + dir * (length + 0.6);
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.Exclude = body;
		trace.LayerMask = EPhysicsLayerPresets.Projectile;
		BaseWorld world = GetGame().GetWorld();
		float hit = world.TraceMove(trace, null);
		vector position = trace.Start + (trace.End - trace.Start) * hit;
		vector up = trace.TraceNorm;
		IEntity surface = trace.TraceEnt;
		if (hit >= 1)
		{
			// Nothing in the way: put it on the ground below the point.
			trace.Start = job.m_vPoint + vector.Up;
			trace.End = job.m_vPoint - vector.Up * 20;
			hit = world.TraceMove(trace, null);
			position = trace.Start + (trace.End - trace.Start) * hit;
			up = trace.TraceNorm;
			surface = trace.TraceEnt;
		}
		if (up == vector.Zero)
			up = vector.Up;
		position = position + up * 0.01;

		// Stick to the vehicle only if the trace really ended on it.
		IEntity attachTo = null;
		if (job.m_Target && surface && surface.GetRootParent() == job.m_Target.GetRootParent())
			attachTo = job.m_Target;
		job.m_Target = attachTo;

		vector side = up * dir;
		if (side.Length() < 0.01)
			side = up * "1 0 0";
		vector forward = up * side;
		side.Normalize();
		forward.Normalize();
		placeable.SetPlacementPosition(forward, up, side, position, rpl.Id());
		job.m_aPlaced[0] = forward;
		job.m_aPlaced[1] = up;
		job.m_aPlaced[2] = side;
		job.m_aPlaced[3] = position;

		AIO_ChargePlacedCallback cb = new AIO_ChargePlacedCallback();
		cb.m_Job = job;
		return inventory.TryRemoveItemFromStorage(job.m_Charge, placeable.GetParentSlot().GetStorage(), cb);
	}

	static void Placed(AIO_Demolition job, bool ok)
	{
		if (!job || !s_aJobs.Contains(job))
			return;
		IEntity body = null;
		if (job.m_Agent)
			body = job.m_Agent.GetControlledEntity();
		if (!ok || !job.m_Charge || !body)
		{
			if (body)
				End(job, body, "couldn't place the charge");
			s_aJobs.RemoveItem(job);
			return;
		}
		SCR_PlaceableInventoryItemComponent placeable = SCR_PlaceableInventoryItemComponent.Cast(job.m_Charge.FindComponent(SCR_PlaceableInventoryItemComponent));
		if (placeable)
		{
			// The placement transform may not be applied yet when the removal completes; attaching
			// to a vehicle localises the current transform, so put the item in place first.
			job.m_Charge.SetWorldTransform(job.m_aPlaced);
			job.m_Charge.Update();
			if (job.m_Target)
				placeable.PlaceItemWithParentChange(SCR_EntityHelper.EntityToRplId(job.m_Target)); // RplComponent id, as the game expects
			else
				placeable.PlaceItem();
		}
		if (job.m_iKind == KIND_MINE)
		{
			SCR_PressureTriggerComponent pressure = SCR_PressureTriggerComponent.Cast(job.m_Charge.FindComponent(SCR_PressureTriggerComponent));
			if (pressure)
			{
				pressure.SetUser(body);
				pressure.ActivateTrigger();
			}
			Print(string.Format("[AIO] DEMOLITION %1 laid a mine at %2", AIO_Server.NameOf(body), job.m_Charge.GetOrigin()));
			Retreat(job, body, 8000, 12);
			job.m_Charge = null; // nothing to wait for: back to orders once clear
			AIO_Supplies.Report(job.m_iPlayerId, string.Format("%1: mine laid and armed", AIO_Server.NameOf(body)));
			return;
		}
		SCR_ExplosiveChargeComponent charge = SCR_ExplosiveChargeComponent.Cast(job.m_Charge.FindComponent(SCR_ExplosiveChargeComponent));
		if (!charge)
		{
			End(job, body, "placed something that isn't a charge");
			s_aJobs.RemoveItem(job);
			return;
		}
		if (charge.GetTrigger())
			charge.GetTrigger().SetUser(body);
		if (job.m_bRemote)
		{
			SCR_DetonatorGadgetComponent detonator = null;
			if (job.m_Detonator)
				detonator = SCR_DetonatorGadgetComponent.Cast(job.m_Detonator.FindComponent(SCR_DetonatorGadgetComponent));
			if (detonator)
			{
				detonator.ConnectNewCharge(charge.GetRplId());
				charge.ConnectDetonator(SCR_EFuzeType.REMOTE, Replication.FindItemId(detonator));
			}
			AIO_RemoteCharge waiting = new AIO_RemoteCharge();
			waiting.m_Charge = job.m_Charge;
			waiting.m_Placer = body;
			waiting.m_Detonator = job.m_Detonator;
			waiting.m_iPlayerId = job.m_iPlayerId;
			s_aRemote.Insert(waiting);
			Print(string.Format("[AIO] DEMOLITION %1 placed a remote charge at %2 on %3", AIO_Server.NameOf(body), WorldPos(job.m_Charge), job.m_Target));
			// Out of the blast before anyone presses the button.
			Retreat(job, body, RETREAT_MS, RETREAT);
			AIO_Supplies.Report(job.m_iPlayerId, string.Format("%1: charge set, ready to detonate on your order", AIO_Server.NameOf(body)));
			return;
		}
		charge.SetFuzeTime(FUZE_SECONDS);
		charge.ArmWithTimedFuze();
		Print(string.Format("[AIO] DEMOLITION %1 placed a timed charge at %2 on %3", AIO_Server.NameOf(body), job.m_Charge.GetOrigin(), job.m_Target));
		Retreat(job, body, (FUZE_SECONDS + 8) * 1000, RETREAT);
		AIO_Supplies.Report(job.m_iPlayerId, string.Format("%1: charge set, %2 seconds!", AIO_Server.NameOf(body), FUZE_SECONDS));
	}

	// He runs some metres from the charge, away from it on his side, and waits there.
	protected static void Retreat(AIO_Demolition job, IEntity body, int waitMs, float distance)
	{
		vector from = job.m_aPlaced[3];
		if (from == vector.Zero)
			from = job.m_vPoint;
		vector away = body.GetOrigin() - from;
		away[1] = 0;
		if (away.Length() < 0.1)
			away = "1 0 0";
		away.Normalize();
		vector spot = from + away * distance;
		spot[1] = GetGame().GetWorld().GetSurfaceY(spot[0], spot[2]);
		job.m_vRetreat = spot;
		job.m_iStage = 2;
		job.m_iDeadline = System.GetTickCount() + waitMs;
		job.m_bEnding = true; // MoveTo releases him, which would cancel this job
		AIO_Tasks.MoveTo(body, spot);
		job.m_bEnding = false;
	}

	// Back to his previous orders.
	protected static void End(AIO_Demolition job, IEntity body, string problem)
	{
		job.m_bEnding = true;
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
			Print(string.Format("[AIO] DEMOLITION %1 %2", AIO_Server.NameOf(body), problem));
		}
	}

	// ---------------------------------------------------------------- detonate

	static string Detonate(int playerId)
	{
		int fired, noPlacer, outOfRange, gone;
		for (int i = s_aRemote.Count() - 1; i >= 0; i--)
		{
			AIO_RemoteCharge waiting = s_aRemote[i];
			if (waiting.m_iPlayerId != playerId)
				continue;
			SCR_ExplosiveChargeComponent charge = null;
			if (waiting.m_Charge && !waiting.m_Charge.IsDeleted())
				charge = SCR_ExplosiveChargeComponent.Cast(waiting.m_Charge.FindComponent(SCR_ExplosiveChargeComponent));
			if (!charge || charge.GetUsedFuzeType() != SCR_EFuzeType.REMOTE || !charge.GetTrigger())
			{
				gone++; // disarmed, taken or already gone off
				s_aRemote.Remove(i);
				continue;
			}
			if (!waiting.m_Placer || !AIO_Server.IsAlive(waiting.m_Placer) || !Carries(waiting.m_Placer, waiting.m_Detonator))
			{
				noPlacer++;
				continue;
			}
			if (vector.Distance(waiting.m_Placer.GetOrigin(), WorldPos(waiting.m_Charge)) > DetonatorRange(waiting.m_Detonator))
			{
				outOfRange++;
				continue;
			}
			charge.GetTrigger().SetUser(waiting.m_Placer);
			charge.GetTrigger().UseTrigger();
			fired++;
			s_aRemote.Remove(i);
		}
		if (fired == 0 && noPlacer == 0 && outOfRange == 0)
			return "No charges waiting for your order";
		string reply = string.Format("%1 charges detonated", fired);
		if (outOfRange > 0)
			reply += string.Format(", %1 out of detonator range", outOfRange);
		if (noPlacer > 0)
			reply += string.Format(", %1 whose placer is down or lost his detonator", noPlacer);
		return reply;
	}

	// GetOrigin of a charge stuck to a vehicle is relative to the vehicle.
	static vector WorldPos(IEntity entity)
	{
		vector mat[4];
		entity.GetWorldTransform(mat);
		return mat[3];
	}

	static float DetonatorRange(IEntity detonator)
	{
		if (!detonator)
			return 0;
		SCR_DetonatorGadgetComponent gadget = SCR_DetonatorGadgetComponent.Cast(detonator.FindComponent(SCR_DetonatorGadgetComponent));
		if (!gadget)
			return 0;
		SCR_DetonatorGadgetComponentClass data = SCR_DetonatorGadgetComponentClass.Cast(gadget.GetComponentData(detonator));
		if (!data || data.GetmaxDetonationRange() < 0)
			return 100000; // unlimited
		return data.GetmaxDetonationRange();
	}

	static int WaitingCharges(int playerId)
	{
		int count;
		foreach (AIO_RemoteCharge waiting : s_aRemote)
		{
			if (waiting.m_iPlayerId == playerId && waiting.m_Charge && !waiting.m_Charge.IsDeleted())
				count++;
		}
		return count;
	}
}
