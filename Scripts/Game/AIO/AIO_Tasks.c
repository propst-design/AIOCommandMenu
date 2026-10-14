// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). Licensed under the GNU GPL v3 or later (see LICENSE).

// Orders to single soldiers (selected men, colour teams). Server only.
//
// Each man gets his own behaviours, so he stays in the player's squad (voice, formation slot):
// - AIO_MoveBehavior: walk to a spot (the game's own "move individually", priority 58, so he
//   still fights when engaged: attack is 70+).
// - AIO_HoldBehavior: stay there (priority 45, above keeping formation at 30). A watchdog walks
//   him back if he drifts more than HOLD_SLACK metres, e.g. after a fight.
// Follow me / any whole-squad order removes both, and he rejoins the formation.

class AIO_MoveBehavior : SCR_AIMoveIndividuallyBehavior
{
	// Same parameters as the game's behaviour (Enforce passes them on to the parent).
	void AIO_MoveBehavior(SCR_AIUtilityComponent utility, SCR_AIActivityBase groupActivity, vector pos, float priority = PRIORITY_BEHAVIOR_MOVE_INDIVIDUALLY, float priorityLevel = PRIORITY_LEVEL_NORMAL, IEntity ent = null, float radius = 1.0)
	{
	}
}

class AIO_HoldBehavior : SCR_AIBehaviorBase
{
	vector m_vSpot;
	IEntity m_Anchor;  // when set, the spot moves with him: m_Anchor's position + m_vOffset
	vector m_vOffset;
	bool m_bCrouched;       // Take cover set a crouch stance: undone on release
	int m_iCoverTileX = -1; // cover point reserved for him (Take cover)
	int m_iCoverTileY;
	int m_iCoverId;
	ref array<vector> m_aLegs; // further points of a route (flanking): walked one after another
	vector m_vLook;            // keeps looking there (360 defense, fortify); zero = his own choice

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
	}

	override int GetCause()
	{
		return SCR_EAIBehaviorCause.GROUP_GOAL;
	}
}

class AIO_Tasks
{
	static const float PRIORITY_HOLD = 45;
	static const float HOLD_SLACK = 6;
	static const int WATCH_MS = 1000;
	static const float LOOK_PRIORITY = 15; // below danger reactions (20) and targets (50+)

	protected static bool s_bWatching;

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
		AIAgent agent = AgentOf(body);
		SCR_AIUtilityComponent utility = UtilityOf(agent);
		if (!utility)
			return;
		AIO_Supplies.Cancel(agent); // any new order ends an errand
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
			}
		}
		utility.SetStateAllActionsOfType(AIO_MoveBehavior, EAIActionState.FAILED);
		utility.SetStateAllActionsOfType(AIO_HoldBehavior, EAIActionState.FAILED);
	}

	// Back to the formation (Follow me): release, and if he is far off, walk him to his leader first
	// (the game's formation following can be slow to pick a far-off man up again).
	static void Rejoin(IEntity body, IEntity leader)
	{
		Release(body);
		if (!leader || vector.DistanceXZ(body.GetOrigin(), leader.GetOrigin()) < 12)
			return;
		SCR_AIUtilityComponent utility = UtilityOf(AgentOf(body));
		if (utility)
			utility.AddAction(new AIO_MoveBehavior(utility, null, leader.GetOrigin(), ent: leader, radius: 6.0));
	}

	static void ReleaseGroup(SCR_AIGroup group)
	{
		array<IEntity> members = {};
		AIO_Server.Members(group, members);
		foreach (IEntity body : members)
			Release(body);
	}

	// Walk to the spot and stay there.
	static bool MoveTo(IEntity body, vector spot)
	{
		SCR_AIUtilityComponent utility = UtilityOf(AgentOf(body));
		if (!utility)
			return false;
		Release(body);
		utility.AddAction(new AIO_MoveBehavior(utility, null, spot, radius: 2.0));
		utility.AddAction(new AIO_HoldBehavior(utility, null, spot));
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
		array<ref AIActionBase> holds = {};
		utility.FindActionsOfType(AIO_HoldBehavior, holds);
		foreach (AIActionBase action : holds)
		{
			AIO_HoldBehavior hold = AIO_HoldBehavior.Cast(action);
			if (hold && hold.GetActionState() != EAIActionState.FAILED && hold.GetActionState() != EAIActionState.COMPLETED)
				return hold;
		}
		return null;
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
			if (utility.HasActionOfType(AIO_MoveBehavior))
				label = "Moving";
		}
		if (utility.HasActionOfType(SCR_AIHealBehavior))
			label = "Healing";
		if (AIO_Supplies.IsFetching(agent))
			label = "Rearming";
		if (AIO_Maneuvers.IsBounding(agent))
			label = "Bounding";
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
		if (!utility.HasActionOfType(SCR_AIHealBehavior))
			utility.AddAction(new SCR_AIHealBehavior(utility, null, body, true));
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
		float ring = 4 * (1 + (index - 1) / 6);
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

	// Men holding a spot who drifted away (a fight, a push) walk back to it.
	protected static void WatchTick()
	{
		KeepStances();
		AIO_Supplies.Tick();
		AIO_Maneuvers.Tick();
		AIO_Vehicles.Tick();
		for (int c = s_aCopyAgents.Count() - 1; c >= 0; c--)
		{
			AIAgent copier = s_aCopyAgents[c];
			IEntity leader = s_aCopyLeaders[c];
			if (!copier || !copier.GetControlledEntity() || !AIO_Server.IsAlive(copier.GetControlledEntity()) || !leader || !AIO_Server.IsAlive(leader))
			{
				if (copier)
					SetStance(copier, -1);
				s_aCopyAgents.RemoveOrdered(c);
				s_aCopyLeaders.RemoveOrdered(c);
				continue;
			}
			ApplyCopiedStance(copier, leader);
		}
		array<AIAgent> agents = {};
		AIWorld world = GetGame().GetAIWorld();
		if (!world)
			return;
		world.GetAIAgents(agents);
		foreach (AIAgent agent : agents)
		{
			SCR_AIUtilityComponent utility = UtilityOf(agent);
			if (!utility)
				continue;
			AIO_HoldBehavior hold = ActiveHold(utility);
			if (!hold)
				continue;
			IEntity body = agent.GetControlledEntity();
			if (!body || !AIO_Server.IsAlive(body))
				continue;
			if (hold.m_Anchor && !AIO_Server.IsAlive(hold.m_Anchor))
			{
				Release(body); // the player he was keeping station on is gone: back to formation
				continue;
			}
			// Keep facing out (360 defense, fortify); enemies and danger still win (higher priority).
			if (hold.m_vLook != vector.Zero)
				utility.m_LookAction.LookAt(hold.m_vLook, LOOK_PRIORITY, 2.0);
			// Route: next leg once he's near the current point.
			if (hold.m_aLegs && !hold.m_aLegs.IsEmpty() && vector.DistanceXZ(body.GetOrigin(), hold.m_vSpot) <= 5)
			{
				hold.m_vSpot = hold.m_aLegs[0];
				hold.m_aLegs.RemoveOrdered(0);
				utility.SetStateAllActionsOfType(AIO_MoveBehavior, EAIActionState.FAILED);
				utility.AddAction(new AIO_MoveBehavior(utility, null, hold.m_vSpot, radius: 2.0));
				continue;
			}
			vector spot = hold.Spot();
			bool spotMoved = vector.Distance(spot, hold.m_vSpot) > HOLD_SLACK;
			if (!spotMoved && vector.Distance(body.GetOrigin(), spot) <= HOLD_SLACK)
				continue;
			if (utility.HasActionOfType(AIO_MoveBehavior))
			{
				if (!spotMoved)
					continue;
				// The anchor moved on: drop the walk to the old spot.
				utility.SetStateAllActionsOfType(AIO_MoveBehavior, EAIActionState.FAILED);
			}
			hold.m_vSpot = spot;
			utility.AddAction(new AIO_MoveBehavior(utility, null, spot, radius: 2.0));
		}
	}
}
