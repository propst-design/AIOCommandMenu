// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). Licensed under the GNU GPL v3 or later (see LICENSE).

// Orders to single soldiers (selected men, colour teams). Server only.
//
// Each man gets his own behaviours, so he stays in the player's squad (voice, formation slot):
// - AIO_MoveBehavior: native individual movement at player priority; emergency reactions win.
// - AIO_HoldBehavior: keep the order queued but yield to native firing at the post. A watchdog walks
//   him back if he drifts more than HOLD_SLACK metres, e.g. after a fight.
// Follow me / any whole-squad order removes both, and he rejoins the formation.

class AIO_MoveBehavior : SCR_AIMoveIndividuallyBehavior
{
	bool m_bTravel; // native SAFE cause lets optional AI mods distinguish travel from tactics
	// Same parameters as the game's behaviour (Enforce passes them on to the parent).
	void AIO_MoveBehavior(SCR_AIUtilityComponent utility, SCR_AIActivityBase groupActivity, vector pos, float priority = PRIORITY_BEHAVIOR_MOVE_INDIVIDUALLY, float priorityLevel = PRIORITY_LEVEL_PLAYER, IEntity ent = null, float radius = 1.0)
	{
		// Mounted movement keeps its native priority; this contact policy is for infantry.
		if (utility && utility.m_OwnerEntity && CompartmentAccessComponent.GetVehicleIn(utility.m_OwnerEntity))
			SetPriorityLevel(PRIORITY_LEVEL_NORMAL);
	}

	override int GetCause()
	{
		if (m_bTravel) return SCR_EAIBehaviorCause.SAFE;
		return SCR_EAIBehaviorCause.GROUP_GOAL;
	}

	override float CustomEvaluate()
	{
		if (AIO_Tasks.SelfAidReady(m_Utility)) return 0;
		// Keep the destination queued while a native retreat from armour takes precedence.
		if (m_Utility && m_Utility.m_CombatComponent && m_Utility.m_CombatComponent.GetRetreatTarget())
			return 0;
		return super.CustomEvaluate();
	}
}

class AIO_HoldBehavior : SCR_AIBehaviorBase
{
	vector m_vSpot;
	IEntity m_Anchor;  // when set, the spot moves with him: m_Anchor's position + m_vOffset
	bool m_bAnchored;  // it had one: a deleted anchor (player disconnected) reads as null, not as dead
	vector m_vOffset;
	bool m_bCrouched;       // Take cover set a crouch stance: undone on release
	int m_iCoverTileX = -1; // cover point reserved for him (Take cover)
	int m_iCoverTileY;
	int m_iCoverId;

	// Release frees the cover point; a man killed in it never got there, and the point stayed
	// "occupied" for every AI's cover search until the mission ended.
	void ~AIO_HoldBehavior()
	{
		if (m_iCoverTileX < 0 || !GetGame())
			return;
		ChimeraCoverManagerComponent covers = AIO_Tasks.Covers();
		if (covers)
			covers.SetOccupiedCover(m_iCoverTileX, m_iCoverTileY, m_iCoverId, false);
	}
	ref array<vector> m_aLegs; // further points of a route (flanking): walked one after another
	vector m_vLook;            // keeps looking there (360 defense, fortify); zero = his own choice
	bool m_bInterior;          // bounded movement to a building post/room
	vector m_vProgressPos;
	float m_fProgressAt;
	float m_fCheckedAt;
	float m_fTravelMs;
	int m_iPlayerId;
	int m_iVisitedRooms;
	int m_iSkippedRooms;
	bool m_bRouteComplete;
	vector m_vLastRoom;
	IEntity m_Building;
	bool m_bReachedPost;          // got to this interior post (garrison summary)
	int m_iDeadEnds;              // watchdog ticks his move had ended short of the post
	int m_iReplans;               // garrison: other posts tried after this one failed
	ref array<vector> m_aFailedSpots;
	bool m_bAllowPursuit; // assault's final position allows autonomous attack movement
	bool m_bTravel;

	// Current spot (for anchored holds: follows the anchor, on the ground).
	vector Spot()
	{
		if (!m_Anchor)
			return m_vSpot;
		vector spot = m_Anchor.GetOrigin() + m_vOffset;
		spot[1] = GetGame().GetWorld().GetSurfaceY(spot[0], spot[2]);
		return spot;
	}

	void AIO_HoldBehavior(SCR_AIUtilityComponent utility, SCR_AIActivityBase groupActivity, vector spot)
	{
		m_vSpot = spot;
		m_sBehaviorTree = "AI/BehaviorTrees/Chimera/Soldier/Wait.bt";
		SetPriority(AIO_Tasks.PRIORITY_HOLD);
		if (!utility || !utility.m_OwnerEntity || !CompartmentAccessComponent.GetVehicleIn(utility.m_OwnerEntity))
			SetPriorityLevel(PRIORITY_LEVEL_PLAYER);
		if (utility)
		{
			utility.AIO_SetOrderHold(this);
			AIO_Tasks.TrackHold(utility.GetAIAgent());
		}
	}

	override float CustomEvaluate()
	{
		if (!m_Utility || !m_Utility.m_OwnerEntity)
			return 0;
		if (AIO_Tasks.SelfAidReady(m_Utility)) return 0;
		if (m_Utility.m_CombatComponent && m_Utility.m_CombatComponent.GetRetreatTarget())
			return 0;
		// Yield to the native shooting behavior at the post. A high-priority Wait.bt
		// would otherwise stop the soldier firing. The order itself remains queued.
		if (m_Utility.m_CombatComponent && m_Utility.m_CombatComponent.GetCurrentTarget() && m_Utility.m_CombatComponent.GetCombatMode() != EAIGroupCombatMode.HOLD_FIRE)
		{
			if (m_bAllowPursuit || vector.Distance(m_Utility.m_OwnerEntity.GetOrigin(), Spot()) <= AIO_Tasks.HOLD_SLACK)
				return 0;
		}
		return GetPriority();
	}

	override int GetCause()
	{
		if (m_bTravel) return SCR_EAIBehaviorCause.SAFE;
		return SCR_EAIBehaviorCause.GROUP_GOAL;
	}
}

class AIO_Tasks
{
	static const float PRIORITY_HOLD = 45;
	static const float HOLD_SLACK = 6;
	static const int WATCH_MS = 1000;
	static const float LOOK_PRIORITY = 15; // below danger reactions (20) and targets (50+)
	static const float INTERIOR_STALL_MS = 25000;
	static const int DEAD_END_TICKS = 4;    // ~4 s standing with his move ended short of the post
	static const int GARRISON_REPLANS = 3;  // other posts tried before he rejoins the formation
	static const float ROOM_STALL_MS = 20000;
	static const float INTERIOR_RADIUS = 0.7;
	static const float INTERIOR_TRAVEL_MS = 180000;

	protected static bool s_bWatching;
	protected static IEntity s_DoorBody;
	protected static IEntity s_DoorBuilding;
	protected static bool s_bDoorOpened;

	// Native self aid (including Heal up) may temporarily interrupt a post or move.
	// Keep the destination rather than cancelling the order to permit treatment.
	static bool SelfAidReady(SCR_AIUtilityComponent utility)
	{
		if (!utility || !utility.HasActionOfType(SCR_AIHealBehavior)) return false;
		array<ref AIActionBase> actions = {};
		utility.FindActionsOfType(SCR_AIHealBehavior, actions);
		foreach (AIActionBase action : actions)
		{
			SCR_AIHealBehavior heal = SCR_AIHealBehavior.Cast(action);
			if (heal && !heal.GetRemoveAction() && heal.m_EntityToHeal == utility.m_OwnerEntity && heal.GetActionState() != EAIActionState.FAILED && heal.GetActionState() != EAIActionState.COMPLETED && heal.Evaluate() > 0) return true;
		}
		return false;
	}

	// ---------------------------------------------------------------- lookups

	static AIAgent AgentOf(IEntity body)
	{
		if (!body)
			return null;
		AIControlComponent control = AIControlComponent.Cast(body.FindComponent(AIControlComponent));
		if (!control)
			return null;
		return control.GetAIAgent();
	}

	static SCR_AIUtilityComponent UtilityOf(AIAgent agent)
	{
		if (!agent)
			return null;
		return SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
	}

	// Bodies for the given ids that really are in this group and alive.
	static void Resolve(SCR_AIGroup group, array<RplId> ids, notnull array<IEntity> bodies)
	{
		array<IEntity> members = {};
		AIO_Server.Members(group, members);
		// Same id the roster sent for each man (no round trip through the replication item).
		foreach (IEntity member : members)
		{
			RplId id = Replication.FindItemId(member);
			if (ids.Contains(id))
				bodies.Insert(member);
		}
	}

	// ---------------------------------------------------------------- orders

	// Clears our tasks: the man goes back to his formation slot.
	static void Release(IEntity body)
	{
		AIO_Routes.CancelObserver(body);
		AIAgent agent = AgentOf(body);
		SCR_AIUtilityComponent utility = UtilityOf(agent);
		if (!utility)
			return;
		utility.AIO_SetOrderHold(null);
		AIO_Supplies.Cancel(agent); // any new order ends an errand
		AIO_Explosives.Cancel(agent);
		AIO_Grenades.Cancel(agent);
		AIO_Repair.Cancel(agent);
		AIO_Logistics.Cancel(agent);
		AIO_Support.Cancel(body);
		AIO_Vehicles.CancelUnflip(body);
		AIO_Maneuvers.Leave(agent); // ...and takes him out of a bound / peel
		array<ref AIActionBase> holds = {};
		utility.FindActionsOfType(AIO_HoldBehavior, holds);
		foreach (AIActionBase action : holds)
		{
			AIO_HoldBehavior hold = AIO_HoldBehavior.Cast(action);
			if (!hold)
				continue;
			if (hold.m_bCrouched)
				SetStance(agent, -1);
			if (hold.m_iCoverTileX >= 0)
			{
				ChimeraCoverManagerComponent covers = Covers();
				if (covers)
					covers.SetOccupiedCover(hold.m_iCoverTileX, hold.m_iCoverTileY, hold.m_iCoverId, false);
				hold.m_iCoverTileX = -1;
			}
			hold.SetRemoveAction(true);
		}
		array<ref AIActionBase> moves = {};
		utility.FindActionsOfType(AIO_MoveBehavior, moves);
		foreach (AIActionBase move : moves)
			move.SetRemoveAction(true);
		utility.SetStateAllActionsOfType(AIO_MoveBehavior, EAIActionState.FAILED);
		utility.SetStateAllActionsOfType(AIO_HoldBehavior, EAIActionState.FAILED);
	}

	// Return to his actual formation slot. A move-to-leader override crowds every man onto the
	// same point and can keep outranking formation while the leader moves.
	static void Rejoin(IEntity body, IEntity leader)
	{
		Release(body);
		SCR_AIUtilityComponent utility = UtilityOf(AgentOf(body));
		if (!utility)
			return;
		AIO_Maneuvers.SetEngage(body, -1);
		// A canceled waypoint can send its cancel message after Follow has been issued.
		// Do not reuse a formation action belonging to that old group activity.
		array<ref AIActionBase> formation = {};
		utility.FindActionsOfType(SCR_AIMoveInFormationBehavior, formation);
		foreach (AIActionBase action : formation)
		{
			if (action.GetRelatedGroupActivity())
			{
				action.SetRemoveAction(true);
				action.Fail();
			}
		}
		if (!HasFormation(utility))
			utility.AddAction(new SCR_AIMoveInFormationBehavior(utility, null));
	}

	static void ReleaseGroup(SCR_AIGroup group)
	{
		array<IEntity> members = {};
		AIO_Server.Members(group, members);
		foreach (IEntity body : members)
			Release(body);
	}

	// Walk to the spot and stay there.
	static bool MoveTo(IEntity body, vector spot, bool travel = false)
	{
		SCR_AIUtilityComponent utility = UtilityOf(AgentOf(body));
		if (!utility)
			return false;
		Release(body);
		AIO_MoveBehavior orderedMove = new AIO_MoveBehavior(utility, null, spot, radius: 2.0);
		travel = travel && AIO_Behaviour.AllowsTravel(body);
		orderedMove.m_bTravel = travel;
		AIO_HoldBehavior orderedHold = new AIO_HoldBehavior(utility, null, spot);
		orderedHold.m_bTravel = travel;
		utility.AddAction(orderedMove);
		utility.AddAction(orderedHold);
		Watch();
		return true;
	}

	// Keep a position relative to the anchor (the player): Advance, Stay back, Flank.
	static bool KeepOffset(IEntity body, IEntity anchor, vector offset)
	{
		SCR_AIUtilityComponent utility = UtilityOf(AgentOf(body));
		if (!utility || !anchor)
			return false;
		Release(body);
		AIO_HoldBehavior hold = new AIO_HoldBehavior(utility, null, anchor.GetOrigin() + offset);
		hold.m_Anchor = anchor;
		hold.m_bAnchored = anchor != null;
		hold.m_vOffset = offset;
		vector spot = hold.Spot();
		hold.m_vSpot = spot;
		utility.AddAction(new AIO_MoveBehavior(utility, null, spot, radius: 2.0));
		utility.AddAction(hold);
		Watch();
		return true;
	}

	// His current (not yet ended) hold, or null. Ended holds can linger in the queue for a moment.
	static AIO_HoldBehavior ActiveHold(SCR_AIUtilityComponent utility)
	{
		if (!utility)
			return null;
		AIO_HoldBehavior cached = utility.AIO_GetOrderHold();
		if (cached)
			return cached;
		array<ref AIActionBase> holds = {};
		utility.FindActionsOfType(AIO_HoldBehavior, holds);
		foreach (AIActionBase action : holds)
		{
			AIO_HoldBehavior hold = AIO_HoldBehavior.Cast(action);
			if (hold && !hold.GetRemoveAction() && hold.GetActionState() != EAIActionState.FAILED && hold.GetActionState() != EAIActionState.COMPLETED)
				return hold;
		}
		return null;
	}

	static bool HasFormation(SCR_AIUtilityComponent utility)
	{
		if (!utility)
			return false;
		array<ref AIActionBase> actions = {};
		utility.FindActionsOfType(SCR_AIMoveInFormationBehavior, actions);
		foreach (AIActionBase action : actions)
		{
			if (!action.GetRemoveAction() && action.GetActionState() != EAIActionState.FAILED && action.GetActionState() != EAIActionState.COMPLETED)
				return true;
		}
		return false;
	}

	static AIO_MoveBehavior ActiveMove(SCR_AIUtilityComponent utility)
	{
		if (!utility)
			return null;
		array<ref AIActionBase> moves = {};
		utility.FindActionsOfType(AIO_MoveBehavior, moves);
		foreach (AIActionBase action : moves)
		{
			AIO_MoveBehavior move = AIO_MoveBehavior.Cast(action);
			if (move && !move.GetRemoveAction() && move.GetActionState() != EAIActionState.FAILED && move.GetActionState() != EAIActionState.COMPLETED)
				return move;
		}
		return null;
	}

	static void GuardInterior(IEntity body, AIO_HoldBehavior hold, int playerId = 0)
	{
		if (!hold)
			return;
		hold.m_bInterior = true;
		hold.m_vProgressPos = body.GetOrigin();
		hold.m_fProgressAt = GetGame().GetWorld().GetWorldTime();
		hold.m_fCheckedAt = hold.m_fProgressAt;
		hold.m_fTravelMs = 0;
		hold.m_iPlayerId = playerId;
		hold.m_bReachedPost = false;
		hold.m_iDeadEnds = 0;
		AIO_MoveBehavior move = ActiveMove(UtilityOf(AgentOf(body)));
		if (move)
			move.m_Radius.m_Value = INTERIOR_RADIUS;
	}

	static bool InteriorArrived(IEntity body, AIO_HoldBehavior hold)
	{
		vector position = body.GetOrigin();
		if (vector.DistanceXZ(position, hold.m_vSpot) > 1.5 || Math.AbsFloat(position[1] - hold.m_vSpot[1]) > 1.5)
			return false;
		if (hold.m_Building)
		{
			vector mins, maxs;
			hold.m_Building.GetWorldBounds(mins, maxs);
			if (position[0] < mins[0] + 0.15 || position[0] > maxs[0] - 0.15 || position[2] < mins[2] + 0.15 || position[2] > maxs[2] - 0.15)
				return false;
			if (!AIO_Maneuvers.BuildingFloor(position, body, hold.m_Building))
				return false;
			TraceParam trace = new TraceParam();
			trace.Start = position + Vector(0, 1, 0);
			trace.End = hold.m_vSpot + Vector(0, 1, 0);
			trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
			trace.Exclude = body;
			if (GetGame().GetWorld().TraceMove(trace, null) < 1)
				return false; // close to the point through a wall is not arrival
		}
		return true;
	}

	protected static void StopMoves(SCR_AIUtilityComponent utility)
	{
		array<ref AIActionBase> moves = {};
		utility.FindActionsOfType(AIO_MoveBehavior, moves);
		foreach (AIActionBase move : moves)
			move.SetRemoveAction(true);
		utility.SetStateAllActionsOfType(AIO_MoveBehavior, EAIActionState.FAILED);
	}

	protected static void FinishRooms(AIO_HoldBehavior hold)
	{
		hold.m_bRouteComplete = true;
		AIO_Supplies.Report(hold.m_iPlayerId, string.Format("Clear route finished: %1 positions reached, %2 inaccessible positions skipped", hold.m_iVisitedRooms, hold.m_iSkippedRooms));
	}

	protected static bool OpenInteriorDoor(IEntity entity)
	{
		DoorComponent door = DoorComponent.Cast(entity.FindComponent(DoorComponent));
		if (!door || door.IsOpening() || Math.AbsFloat(door.GetNormalizedDoorState()) > 0.05 || Math.AbsFloat(door.GetControlValue()) > 0.05)
			return true;
		IEntity parent = entity;
		while (parent && parent != s_DoorBuilding)
			parent = parent.GetParent();
		if (!parent)
			return true;
		SCR_DoorUserAction action = SCR_AIOpenDoor.FindDoorUserAction(entity);
		if (!action || !action.CanBePerformedScript(s_DoorBody))
			return true;
		action.PerformAction(entity, s_DoorBody);
		if (!door.IsOpening() && Math.AbsFloat(door.GetNormalizedDoorState()) <= 0.05 && Math.AbsFloat(door.GetControlValue()) <= 0.05)
			return true; // locked/nonresponsive doors must not keep resetting the timeout
		s_bDoorOpened = true;
		Print("[AIO] Opening nearby door for interior movement: " + entity);
		return false;
	}

	// Pause the budget during combat; stop an inaccessible room from consuming the man forever.
	protected static bool CheckInterior(IEntity body, SCR_AIUtilityComponent utility, AIO_HoldBehavior hold)
	{
		float now = GetGame().GetWorld().GetWorldTime();
		float elapsed = Math.Max(0, now - hold.m_fCheckedAt);
		hold.m_fCheckedAt = now;
		// Clear: rooms another man already found cut off are skipped without walking to them.
		bool knownDead;
		if (hold.m_Building && hold.m_aLegs && !hold.m_bRouteComplete)
		{
			while (!hold.m_aLegs.IsEmpty() && AIO_Maneuvers.CutOff(hold.m_Building, hold.m_aLegs[0]))
			{
				hold.m_aLegs.RemoveOrdered(0);
				hold.m_iSkippedRooms++;
			}
			knownDead = AIO_Maneuvers.CutOff(hold.m_Building, hold.m_vSpot);
		}
		if (!knownDead && InteriorArrived(body, hold))
		{
			hold.m_bReachedPost = true;
			hold.m_iDeadEnds = 0;
			if (hold.m_aLegs && !hold.m_bRouteComplete)
			{
				hold.m_iVisitedRooms++;
				hold.m_vLastRoom = body.GetOrigin();
				if (hold.m_aLegs.IsEmpty())
					FinishRooms(hold);
			}
			hold.m_fProgressAt = now;
			hold.m_fTravelMs = 0;
			hold.m_vProgressPos = body.GetOrigin();
			return true;
		}
		SCR_AIBehaviorBase current = SCR_AIBehaviorBase.Cast(utility.GetExecutedAction());
		if (current && current.GetCause() >= SCR_EAIBehaviorCause.COMBAT)
		{
			hold.m_fProgressAt += elapsed;
			return true;
		}
		hold.m_fTravelMs += elapsed;
		if (vector.Distance(body.GetOrigin(), hold.m_vProgressPos) >= 1)
		{
			hold.m_vProgressPos = body.GetOrigin();
			hold.m_fProgressAt = now;
			hold.m_iDeadEnds = 0;
		}
		// The native pathfinder ends his move at the nearest point it can reach when the post
		// can't be reached (a floor with no stairs on the navmesh, a closed-off room). The
		// watchdog re-issues it and he stands there: a few of those in a row is a dead end,
		// no need to wait out the whole stall time.
		if (!ActiveMove(utility) && now - hold.m_fProgressAt >= 2000)
			hold.m_iDeadEnds++;
		if (hold.m_Building && now - hold.m_fProgressAt >= 3000)
		{
			s_DoorBody = body;
			s_DoorBuilding = hold.m_Building;
			s_bDoorOpened = false;
			GetGame().GetWorld().QueryEntitiesBySphere(body.GetOrigin() + Vector(0, 1, 0), 2.5, OpenInteriorDoor, null, EQueryEntitiesFlags.ALL);
			s_DoorBody = null;
			s_DoorBuilding = null;
			if (s_bDoorOpened)
			{
				hold.m_fProgressAt = now;
				StopMoves(utility);
				utility.AddAction(new AIO_MoveBehavior(utility, null, hold.m_vSpot, radius: INTERIOR_RADIUS));
				return true;
			}
		}
		float stall = INTERIOR_STALL_MS;
		if (hold.m_aLegs)
			stall = ROOM_STALL_MS;
		bool deadEnd = hold.m_iDeadEnds >= DEAD_END_TICKS || knownDead;
		if (!deadEnd && now - hold.m_fProgressAt < stall && hold.m_fTravelMs < INTERIOR_TRAVEL_MS)
			return true;
		int playerId = hold.m_iPlayerId;
		Print(string.Format("[AIO] %1 cannot reach interior post %2 from %3 (action %4, dead end %5); abandoning that position", AIO_Server.NameOf(body), hold.m_vSpot, body.GetOrigin(), utility.GetExecutedAction(), deadEnd));
		AIAgent agent = AgentOf(body);
		AIO_Approach garrison = AIO_Maneuvers.GarrisonOf(agent);
		if (hold.m_Building)
			AIO_Maneuvers.DeadEnd(hold.m_Building, body, hold.m_vSpot, hold.m_iDeadEnds >= DEAD_END_TICKS);
		// Garrison: try the nearest free post he may be able to reach before giving up.
		if (!hold.m_aLegs && hold.m_Building && hold.m_iReplans < GARRISON_REPLANS)
		{
			if (!hold.m_aFailedSpots)
				hold.m_aFailedSpots = {};
			hold.m_aFailedSpots.Insert(hold.m_vSpot);
			hold.m_iReplans++;
			vector look;
			vector next = AIO_Maneuvers.AlternatePost(body, hold, look);
			if (next != vector.Zero)
			{
				Print(string.Format("[AIO] %1 takes post %2 instead (try %3)", AIO_Server.NameOf(body), next, hold.m_iReplans));
				hold.m_vSpot = next;
				hold.m_vLook = next + look * 60;
				GuardInterior(body, hold, playerId);
				StopMoves(utility);
				utility.AddAction(new AIO_MoveBehavior(utility, null, next, radius: INTERIOR_RADIUS));
				return true;
			}
		}
		if (hold.m_aLegs && !hold.m_bRouteComplete)
		{
			hold.m_iSkippedRooms++;
			StopMoves(utility);
			if (!hold.m_aLegs.IsEmpty())
			{
				hold.m_vSpot = hold.m_aLegs[0];
				hold.m_aLegs.RemoveOrdered(0);
				GuardInterior(body, hold, playerId);
				utility.AddAction(new AIO_MoveBehavior(utility, null, hold.m_vSpot, radius: INTERIOR_RADIUS));
				return true;
			}
			if (hold.m_iVisitedRooms > 0)
			{
				hold.m_vSpot = hold.m_vLastRoom;
				GuardInterior(body, hold, playerId);
				FinishRooms(hold);
				utility.AddAction(new AIO_MoveBehavior(utility, null, hold.m_vSpot, radius: INTERIOR_RADIUS));
				return true;
			}
			// None of his rooms could be reached: one he can reach instead, so he still ends inside.
			if (hold.m_Building && hold.m_iReplans < GARRISON_REPLANS)
			{
				hold.m_iReplans++;
				vector roomLook;
				vector room = AIO_Maneuvers.AlternatePost(body, hold, roomLook);
				if (room != vector.Zero)
				{
					Print(string.Format("[AIO] %1 clears %2 instead (try %3)", AIO_Server.NameOf(body), room, hold.m_iReplans));
					hold.m_vSpot = room;
					GuardInterior(body, hold, playerId);
					utility.AddAction(new AIO_MoveBehavior(utility, null, room, radius: INTERIOR_RADIUS));
					return true;
				}
			}
		}
		if (garrison)
			AIO_Maneuvers.GaveUp(agent); // the garrison summary reports him
		Rejoin(body, null);
		if (!garrison)
			AIO_Supplies.Report(playerId, "A soldier cannot reach his building position; returning to formation");
		return false;
	}

	// Short label of what the man is doing for the squad bar ("" = nothing special).
	static string TaskLabel(IEntity body)
	{
		AIAgent agent = AgentOf(body);
		SCR_AIUtilityComponent utility = UtilityOf(agent);
		if (!utility)
			return "";
		string label = "";
		AIO_HoldBehavior hold = ActiveHold(utility);
		if (hold)
		{
			if (hold.m_bCrouched)
				label = "Cover";
			else if (hold.m_Anchor)
				label = "Station";
			else
				label = "Holding";
			if (ActiveMove(utility))
				label = "Moving";
		}
		string routeLabel = AIO_Routes.Label(body);
		if (!routeLabel.IsEmpty()) label = routeLabel;
		if (utility.HasActionOfType(SCR_AIHealBehavior))
			label = "Healing";
		if (utility.HasActionOfType(AIO_TreatBehavior))
			label = "Treating casualty";
		if (hold && hold.m_vLook != vector.Zero)
			label += ", Watching";
		if (AIO_Supplies.IsFetching(agent))
			label = "Rearming";
		string demolition = AIO_Explosives.Label(agent);
		if (!demolition.IsEmpty())
			label = demolition;
		string grenade = AIO_Grenades.Label(agent);
		if (!grenade.IsEmpty())
			label = grenade;
		string specialist = AIO_Repair.Label(agent) + AIO_FireSupport.Label(agent) + AIO_Logistics.Label(agent);
		if (!specialist.IsEmpty())
			label = specialist;
		if (AIO_Maneuvers.IsBounding(agent))
			label = AIO_Bounding.Label(agent);
		if (AIO_Vehicles.IsPushing(body))
			label = "Unflipping";
		string seat = AIO_Vehicles.Label(body);
		if (!seat.IsEmpty())
		{
			if (!label.IsEmpty())
				label = seat + ", " + label;
			else
				label = seat;
		}
		if (AIO_Maneuvers.IsWaitingForLead(agent))
		{
			if (!label.IsEmpty())
				label += ", ";
			label += "Wait to fire";
		}
		if (s_aCopyAgents.Contains(agent))
		{
			if (!label.IsEmpty())
				label += ", ";
			label += "Copying";
		}
		string mode = AIO_Behaviour.Label(agent);
		if (!mode.IsEmpty())
		{
			if (!label.IsEmpty())
				label += ", ";
			label += mode;
		}
		return label;
	}

	// ---------------------------------------------------------------- speed

	// Movement speed for his moves (EMovementType), or -1 = his own choice. Combat dashes aren't limited.
	static void SetSpeed(AIAgent agent, int speed)
	{
		if (!agent)
			return;
		SCR_AICharacterSettingsComponent settings = SCR_AICharacterSettingsComponent.Cast(agent.FindComponent(SCR_AICharacterSettingsComponent));
		if (!settings)
			return;
		settings.RemoveSettingsOfTypeAndOrigin(SCR_AICharacterMovementSpeedSettingBase, SCR_EAISettingOrigin.COMMANDING);
		if (speed >= 0)
			settings.AddCharacterSetting(SCR_AICharacterMovementSpeedSetting.Create(SCR_EAISettingOrigin.COMMANDING, SCR_EAIBehaviorCause.GROUP_GOAL, speed), false, true);
	}

	// ---------------------------------------------------------------- stance

	// Fixed stance for the man (ECharacterStance), or -1 = his own choice again.
	static void SetStance(AIAgent agent, int stance)
	{
		if (!agent)
			return;
		SCR_AICharacterSettingsComponent settings = SCR_AICharacterSettingsComponent.Cast(agent.FindComponent(SCR_AICharacterSettingsComponent));
		if (!settings)
			return;
		settings.RemoveSettingsOfTypeAndOrigin(SCR_AICharacterStanceSettingBase, SCR_EAISettingOrigin.COMMANDING);
		int at = s_aStanceAgents.Find(agent);
		if (at >= 0)
		{
			s_aStanceAgents.RemoveOrdered(at);
			s_aStances.RemoveOrdered(at);
		}
		if (stance < 0)
		{
			// Auto: stand up now; the man's own behaviours pick his stance from here on.
			ChimeraCharacter freed = ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (freed && freed.GetCharacterController() && freed.GetCharacterController().GetStance() == ECharacterStance.PRONE)
				SCR_AIStanceHandling.SetStance(freed.GetCharacterController(), ECharacterStance.CROUCH);
			AIO_Behaviour.RestoreStance(agent, settings);
			return;
		}
		s_aStanceAgents.Insert(agent);
		s_aStances.Insert(stance);
		Watch();
		settings.AddCharacterSetting(SCR_AICharacterStanceSetting.Create(SCR_EAISettingOrigin.COMMANDING, SCR_EAIBehaviorCause.COMBAT, stance), false, true);
		// The setting is read when a behaviour sets a stance; a man standing idle (holding) would not
		// change until then, so change it now as well.
		ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
		if (character && character.GetCharacterController())
			SCR_AIStanceHandling.SetStance(character.GetCharacterController(), stance);
	}

	// Men with an ordered stance: the watchdog re-applies it when something else changed it
	// (idle men re-read the stance setting only when a behaviour sets a stance).
	protected static ref array<AIAgent> s_aStanceAgents = {};
	protected static ref array<int> s_aStances = {};

	protected static void KeepStances()
	{
		for (int i = s_aStanceAgents.Count() - 1; i >= 0; i--)
		{
			AIAgent agent = s_aStanceAgents[i];
			ChimeraCharacter character = null;
			if (agent)
				character = ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (!character || !AIO_Server.IsAlive(character) || !character.GetCharacterController())
			{
				s_aStanceAgents.RemoveOrdered(i);
				s_aStances.RemoveOrdered(i);
				continue;
			}
			CharacterControllerComponent controller = character.GetCharacterController();
			if (controller.GetStance() != s_aStances[i])
				SCR_AIStanceHandling.SetStance(controller, s_aStances[i]);
		}
	}

	protected static ref array<AIAgent> s_aCopyAgents = {};
	// Men given a hold: the watchdog checks these, not every AI in the world each second.
	protected static ref array<AIAgent> s_aHoldAgents = {};

	static void TrackHold(AIAgent agent)
	{
		if (agent && !s_aHoldAgents.Contains(agent))
			s_aHoldAgents.Insert(agent);
	}
	protected static ref array<IEntity> s_aCopyLeaders = {};

	// The man copies the leader's stance until told otherwise (Stance > Auto or another stance).
	static void CopyStance(IEntity body, IEntity leader)
	{
		AIAgent agent = AgentOf(body);
		if (!agent || !leader)
			return;
		StopCopying(agent);
		s_aCopyAgents.Insert(agent);
		s_aCopyLeaders.Insert(leader);
		ApplyCopiedStance(agent, leader);
		Watch();
	}

	static void StopCopying(AIAgent agent)
	{
		int at = s_aCopyAgents.Find(agent);
		if (at < 0)
			return;
		s_aCopyAgents.RemoveOrdered(at);
		s_aCopyLeaders.RemoveOrdered(at);
	}

	protected static void ApplyCopiedStance(AIAgent agent, IEntity leader)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(leader);
		if (!character || !character.GetCharacterController())
			return;
		SetStance(agent, character.GetCharacterController().GetStance());
	}

	// ---------------------------------------------------------------- cover

	static ChimeraCoverManagerComponent Covers()
	{
		AIWorld world = GetGame().GetAIWorld();
		if (!world)
			return null;
		return ChimeraCoverManagerComponent.Cast(world.FindComponent(ChimeraCoverManagerComponent));
	}

	// Nearest free cover point within 25 m that faces the threat; he moves there, crouches and holds.
	// Without a cover point he crouches where he is. Returns true when a cover point was found.
	static bool TakeCover(IEntity body, vector threat)
	{
		AIAgent agent = AgentOf(body);
		SCR_AIUtilityComponent utility = UtilityOf(agent);
		if (!utility)
			return false;
		Release(body);
		ChimeraCoverManagerComponent covers = Covers();
		AIPathfindingComponent pathfinding = AIPathfindingComponent.Cast(body.FindComponent(AIPathfindingComponent));
		bool found;
		vector coverPos, tallest;
		int tileX, tileY, coverId;
		if (covers && pathfinding)
		{
			CoverQueryProperties query = new CoverQueryProperties();
			query.m_vNearestPolyHalfExtend = SCR_AIFindCover.NEAREST_POLY_HALF_EXTEND;
			query.m_fNmAreaCostScale = SCR_AIFindCover.NAVMESH_AREA_COST_SCALE;
			query.m_vSectorPos = body.GetOrigin();
			query.m_vSectorDir = vector.Zero;
			query.m_vThreatPos = threat;
			query.m_fQuerySectorAngleCosMin = -1.0;
			query.m_fSectorDistMin = 0;
			query.m_fSectorDistMax = 25;
			query.m_fCoverToThreatAngleCosMin = 0.3; // the cover is between him and the threat
			query.m_fScoreWeightDirection = 0;
			query.m_fScoreWeightDistance = 1.0;
			query.m_bCheckVisibility = false;
			query.m_bSelectHighestScore = false;
			query.m_iMaxCoversToCheck = 25;
			found = covers.GetBestCover("Soldiers", pathfinding, query, coverPos, tallest, tileX, tileY, coverId);
		}
		vector spot = body.GetOrigin();
		if (found)
		{
			spot = coverPos;
			covers.SetOccupiedCover(tileX, tileY, coverId, true);
			utility.AddAction(new AIO_MoveBehavior(utility, null, spot, radius: 1.0));
		}
		AIO_HoldBehavior hold = new AIO_HoldBehavior(utility, null, spot);
		hold.m_bCrouched = true;
		if (found)
		{
			hold.m_iCoverTileX = tileX;
			hold.m_iCoverTileY = tileY;
			hold.m_iCoverId = coverId;
		}
		utility.AddAction(hold);
		SetStance(agent, ECharacterStance.CROUCH);
		Watch();
		return found;
	}

	// ---------------------------------------------------------------- healing

	// Wounded man with something to heal himself with: treats himself (the game's own heal behaviour).
	// Returns 1 = treating, 0 = not hurt, -1 = hurt but nothing to treat with.
	static int HealSelf(IEntity body)
	{
		SCR_AIUtilityComponent utility = UtilityOf(AgentOf(body));
		if (!utility)
			return 0;
		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(SCR_DamageManagerComponent.GetDamageManager(body));
		if (!damage)
			return 0;
		if (damage.GetHealthScaled() >= 0.99 && !damage.IsBleeding())
			return 0;
		if (!utility.m_AIInfo || !utility.m_AIInfo.HasRole(EUnitRole.MEDIC))
			return -1;
		SCR_AIHealBehavior heal = SCR_AIHealBehavior.Cast(utility.FindActionOfType(SCR_AIHealBehavior));
		if (!heal || heal.GetRemoveAction() || heal.GetActionState() == EAIActionState.FAILED || heal.GetActionState() == EAIActionState.COMPLETED)
			utility.AddAction(new SCR_AIHealBehavior(utility, null, body, true, priorityLevel: SCR_AIActionBase.PRIORITY_LEVEL_PLAYER));
		else
			heal.SetPriorityLevel(SCR_AIActionBase.PRIORITY_LEVEL_PLAYER);
		return 1;
	}

	// Flat forward and right directions of an entity (yaw only).
	static void Facing(IEntity entity, out vector forward, out vector right)
	{
		vector transform[4];
		entity.GetWorldTransform(transform);
		forward = transform[2];
		forward[1] = 0;
		forward.Normalize();
		right = Vector(forward[2], 0, -forward[0]);
	}

	// Men side by side across a direction, 5 m apart, centred on the offset.
	static vector LineOffset(vector offset, vector across, int index, int count)
	{
		float slot = index - (count - 1) * 0.5;
		return offset + across * (slot * 5);
	}

	// Stay where he is.
	static bool HoldHere(IEntity body)
	{
		SCR_AIUtilityComponent utility = UtilityOf(AgentOf(body));
		if (!utility)
			return false;
		Release(body);
		utility.AddAction(new AIO_HoldBehavior(utility, null, body.GetOrigin()));
		Watch();
		return true;
	}

	// Spots around a point for several men: the first at the point, the rest on rings 4 m apart.
	static vector SpreadSpot(vector center, int index)
	{
		if (index == 0)
			return center;
		float ring = 4 * (1 + (index - 1) / 6); // int-math ok: whole rings of 6
		float angle = (index - 1) * 1.047; // 60 degrees
		vector spot = center + Vector(Math.Cos(angle) * ring, 0, Math.Sin(angle) * ring);
		spot[1] = GetGame().GetWorld().GetSurfaceY(spot[0], spot[2]);
		return spot;
	}

	// ---------------------------------------------------------------- watchdog

	static void Watch()
	{
		if (s_bWatching)
			return;
		s_bWatching = true;
		GetGame().GetCallqueue().CallLater(WatchTick, WATCH_MS, true);
	}

	// Mission end (AIO_Game.c): stop the watchdog and forget the men, so the next mission's
	// first order starts it again (a stale s_bWatching would leave every hold unwatched).
	static void Cleanup()
	{
		GetGame().GetCallqueue().Remove(WatchTick);
		s_bWatching = false;
		s_aStanceAgents.Clear();
		s_aStances.Clear();
		s_aCopyAgents.Clear();
		s_aCopyLeaders.Clear();
		s_aHoldAgents.Clear();
		s_DoorBody = null;
		s_DoorBuilding = null;
	}

	// Men holding a spot who drifted away (a fight, a push) walk back to it.
	protected static void WatchTick()
	{
		KeepStances();
		AIO_Behaviour.Tick();
		AIO_Explosives.Tick();
		AIO_Grenades.Tick();
		AIO_Repair.Tick();
		AIO_FireSupport.Tick();
		AIO_Logistics.Tick();
		AIO_Supplies.Tick();
		AIO_Maneuvers.Tick();
		AIO_Vehicles.Tick();
		AIO_Support.Tick();
		AIO_Routes.Tick();
		for (int c = s_aCopyAgents.Count() - 1; c >= 0; c--)
		{
			AIAgent copier = s_aCopyAgents[c];
			IEntity leader = s_aCopyLeaders[c];
			if (!copier || !copier.GetControlledEntity() || !AIO_Server.IsAlive(copier.GetControlledEntity()) || !leader || !AIO_Server.IsAlive(leader)
				|| !AIO_Server.StillCommands(leader, copier.GetControlledEntity()))
			{
				if (copier)
					SetStance(copier, -1);
				s_aCopyAgents.RemoveOrdered(c);
				s_aCopyLeaders.RemoveOrdered(c);
				continue;
			}
			ApplyCopiedStance(copier, leader);
		}
		// Copy: a release below can give a man a new hold (tracked again at the end).
		array<AIAgent> agents = {};
		agents.Copy(s_aHoldAgents);
		foreach (AIAgent agent : agents)
		{
			SCR_AIUtilityComponent utility = UtilityOf(agent);
			AIO_HoldBehavior hold;
			if (utility)
				hold = ActiveHold(utility);
			if (!hold)
			{
				s_aHoldAgents.RemoveItem(agent); // no hold any more (or gone): stop checking him
				continue;
			}
			IEntity body = agent.GetControlledEntity();
			if (!body || !AIO_Server.IsAlive(body))
				continue;
			if (hold.m_bInterior && !CheckInterior(body, utility, hold))
				continue;
			if (hold.m_bAnchored && (!hold.m_Anchor || !AIO_Server.IsAlive(hold.m_Anchor)))
			{
				Release(body); // the player he was keeping station on is gone: back to formation
				continue;
			}
			// Keep facing out (360 defense, fortify); enemies and danger still win (higher priority).
			if (hold.m_vLook != vector.Zero)
				utility.m_LookAction.LookAt(hold.m_vLook, LOOK_PRIORITY, 2.0);
			// Route: next leg once he's near the current point.
			bool arrived = vector.DistanceXZ(body.GetOrigin(), hold.m_vSpot) <= 5;
			if (hold.m_bInterior)
				arrived = InteriorArrived(body, hold);
			float radius = 2;
			if (hold.m_bInterior)
				radius = INTERIOR_RADIUS;
			if (hold.m_aLegs && !hold.m_aLegs.IsEmpty() && arrived)
			{
				hold.m_vSpot = hold.m_aLegs[0];
				hold.m_aLegs.RemoveOrdered(0);
				if (hold.m_bInterior)
					GuardInterior(body, hold, hold.m_iPlayerId);
				StopMoves(utility);
				utility.AddAction(new AIO_MoveBehavior(utility, null, hold.m_vSpot, radius: radius));
				continue;
			}
			vector spot = hold.Spot();
			bool spotMoved = vector.Distance(spot, hold.m_vSpot) > HOLD_SLACK;
			bool settled = vector.Distance(body.GetOrigin(), spot) <= HOLD_SLACK;
			if (hold.m_bInterior)
				settled = InteriorArrived(body, hold);
			if (!spotMoved && settled)
				continue;
			if (ActiveMove(utility))
			{
				if (!spotMoved)
					continue;
				// The anchor moved on: drop the walk to the old spot.
				StopMoves(utility);
			}
			hold.m_vSpot = spot;
			AIO_MoveBehavior retry = new AIO_MoveBehavior(utility, null, spot, radius: radius);
			retry.m_bTravel = hold.m_bTravel;
			utility.AddAction(retry);
		}
	}
}
