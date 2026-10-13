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
		foreach (RplId id : ids)
		{
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
			if (!rpl)
				continue;
			IEntity body = rpl.GetEntity();
			if (body && members.Contains(body))
				bodies.Insert(body);
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
		if (stance >= 0)
			settings.AddCharacterSetting(SCR_AICharacterStanceSetting.Create(SCR_EAISettingOrigin.COMMANDING, SCR_EAIBehaviorCause.COMBAT, stance), false, true);
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

	protected static void Watch()
	{
		if (s_bWatching)
			return;
		s_bWatching = true;
		GetGame().GetCallqueue().CallLater(WatchTick, WATCH_MS, true);
	}

	// Men holding a spot who drifted away (a fight, a push) walk back to it.
	protected static void WatchTick()
	{
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
		array<ref AIActionBase> holds = {};
		foreach (AIAgent agent : agents)
		{
			SCR_AIUtilityComponent utility = UtilityOf(agent);
			if (!utility)
				continue;
			holds.Clear();
			utility.FindActionsOfType(AIO_HoldBehavior, holds);
			if (holds.IsEmpty())
				continue;
			IEntity body = agent.GetControlledEntity();
			if (!body || !AIO_Server.IsAlive(body))
				continue;
			AIO_HoldBehavior hold = AIO_HoldBehavior.Cast(holds[0]);
			if (!hold || hold.GetActionState() == EAIActionState.FAILED || hold.GetActionState() == EAIActionState.COMPLETED)
				continue;
			if (hold.m_Anchor && !AIO_Server.IsAlive(hold.m_Anchor))
			{
				Release(body); // the player he was keeping station on is gone: back to formation
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
