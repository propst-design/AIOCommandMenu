// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). Licensed under the GNU GPL v3 or later (see LICENSE).

// More > Recruit / dismiss (server only). Take nearby AI soldiers or whole AI squads of your own side
// into your squad, let selected men go as a squad of their own, or remove soldiers from the game.
// Men are traded between players this way: one leader dismisses them, another recruits them.
// Never touches a player, nor anyone in a squad a player is part of: a slave group whose master
// holds players, a playable group, or a group with a man a player controls (Game Master, ACA command).
// Men move through the game's own group calls, the same steps as the vanilla recruit and dismiss
// commands (AddAgentFromControlledEntity / RemoveAgentFromControlledEntity plus the squad's AI member
// list), so the vanilla group menu and its joined/left notifications see them. A player's squad keeps
// the server's AI-per-squad limit (SCR_CommandingManagerComponent) unless the player is an admin.
class AIO_Recruit
{
	static const ResourceName NEW_GROUP = "{000CD338713F2B5A}Prefabs/AI/Groups/Group_Base.et";
	static const float RANGE = 100;   // aimed orders reach soldiers this close to the leader (admins: any)
	static const float NEARBY = 30;   // Recruit everyone nearby
	static const float AIM_SNAP = 4;  // aimed at the ground or a vehicle: the nearest soldier this close

	protected static ref array<IEntity> s_aFound;
	// Called by AIO_StaticInit at game start (static initializer budget: tools/check_static_init.py).
	static void AIO_InitStatics()
	{
		s_aFound = {};
	}

	static bool IsOrder(int order)
	{
		return order == AIO_EOrder.RECRUIT_MAN || order == AIO_EOrder.RECRUIT_SQUAD || order == AIO_EOrder.RECRUIT_NEARBY
			|| order == AIO_EOrder.DISMISS_SELECTED || order == AIO_EOrder.DESTROY_SELECTED
			|| order == AIO_EOrder.DESTROY_MAN || order == AIO_EOrder.DESTROY_SQUAD;
	}

	// From the menu (AIO_Server.Execute, after the caller was found alive and conscious).
	static string Order(SCR_PlayerController pc, int order, array<RplId> members, vector point, RplId target)
	{
		int playerId = pc.GetPlayerId();
		SCR_AIGroup squad = AIO_Server.Slave(playerId);
		if (!squad)
			return "Lead a squad first (More > Become leader)";
		array<IEntity> selected = {};
		if (members && !members.IsEmpty())
			AIO_Tasks.Resolve(squad, members, selected);
		return Run(squad, pc.GetControlledEntity(), AIO_Server.AdminAllowed(playerId), order, selected, point, AIO_Server.EntityOf(target));
	}

	// The order itself; server tests call it directly. squad = the leader's AI squad (slave or possessed
	// group), leader = his body, selected = his own men picked in the menu, target = what the ray hit.
	static string Run(SCR_AIGroup squad, IEntity leader, bool admin, int order, array<IEntity> selected, vector point, IEntity target)
	{
		if (!squad || !leader)
			return "No squad to command";
		selected.RemoveItem(leader);
		if (order == AIO_EOrder.DISMISS_SELECTED)
		{
			if (selected.IsEmpty())
				return "Select the men to dismiss first (0 = Select members)";
			return Dismiss(squad, selected);
		}
		if (order == AIO_EOrder.DESTROY_SELECTED)
		{
			if (selected.IsEmpty())
				return "Select the men to destroy first (0 = Select members)";
			return Destroy(selected);
		}
		if (order == AIO_EOrder.RECRUIT_NEARBY)
			return RecruitNearby(squad, leader, admin);

		IEntity man = Aimed(target, point, leader);
		if (!man)
			return "No soldier there";
		bool recruit = order == AIO_EOrder.RECRUIT_MAN || order == AIO_EOrder.RECRUIT_SQUAD;
		string refusal = Refusal(man, squad, leader, admin, recruit);
		if (!refusal.IsEmpty())
			return refusal;
		array<IEntity> men = {};
		if (order == AIO_EOrder.RECRUIT_MAN || order == AIO_EOrder.DESTROY_MAN)
			men.Insert(man);
		else
		{
			SCR_AIGroup group = GroupOf(man);
			if (group == squad)
				return "That's your own squad: select the men and use Destroy selected men";
			SquadOf(man, men);
		}
		if (recruit)
			return Recruit(squad, leader, admin, men);
		return Destroy(men);
	}

	// ---------------------------------------------------------------- who may be taken

	// Why this soldier can't be recruited / destroyed by this leader ("" = he can). The leader's own men
	// can always be destroyed. Same side = same faction (as the vanilla recruit command); destroying
	// the other side is an admin's call, like More > Admin > Kill what I look at.
	static string Refusal(IEntity man, SCR_AIGroup squad, IEntity leader, bool admin, bool recruit)
	{
		if (!ChimeraCharacter.Cast(man) || !AIO_Server.IsAlive(man))
			return "No living soldier there";
		if (man == leader || AIO_Server.IsPlayerControlled(man))
			return "That's a player";
		AIAgent agent = AIO_Tasks.AgentOf(man);
		if (!agent)
			return "That soldier has no AI";
		SCR_AIGroup group = SCR_AIGroup.Cast(agent.GetParentGroup());
		if (group && group == squad)
		{
			if (recruit)
				return AIO_Server.NameOf(man) + " is already in your squad";
			return "";
		}
		if (group && PlayersSquad(group))
			return AIO_Server.NameOf(man) + " belongs to another player's squad";
		if (!SameSide(man, leader))
		{
			if (recruit)
				return "Only soldiers of your own side can join";
			if (!admin)
				return "Only admins can destroy the other side";
		}
		if (!admin && vector.Distance(man.GetOrigin(), leader.GetOrigin()) > RANGE)
			return "Too far away (100 m at most)";
		return "";
	}

	// A squad a player is part of: hands off. Playable groups hold players; a slave group belongs to the
	// players of its master; a group with a man a player controls is that player's squad.
	static bool PlayersSquad(SCR_AIGroup group)
	{
		if (group.IsPlayable())
			return true;
		SCR_AIGroup master = group.GetMaster();
		if (master && master.GetPlayerCount() > 0)
			return true;
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (agent && AIO_Server.IsPlayerControlled(agent.GetControlledEntity()))
				return true;
		}
		return false;
	}

	protected static bool SameSide(IEntity man, IEntity leader)
	{
		SCR_ChimeraCharacter one = SCR_ChimeraCharacter.Cast(man);
		SCR_ChimeraCharacter other = SCR_ChimeraCharacter.Cast(leader);
		if (!one || !other || !one.GetFaction() || !other.GetFaction())
			return true; // no faction to compare (some Game Master units): nothing to tell them apart by
		return one.GetFaction().GetFactionKey() == other.GetFaction().GetFactionKey();
	}

	protected static SCR_AIGroup GroupOf(IEntity man)
	{
		AIAgent agent = AIO_Tasks.AgentOf(man);
		if (!agent)
			return null;
		return SCR_AIGroup.Cast(agent.GetParentGroup());
	}

	// The man and every other living AI soldier of his group.
	protected static void SquadOf(IEntity man, notnull array<IEntity> men)
	{
		SCR_AIGroup group = GroupOf(man);
		if (group)
			AIO_Server.Members(group, men);
		if (!men.Contains(man))
			men.Insert(man);
	}

	// The soldier the player means: the one the ray hit, else the nearest living soldier a few metres
	// from the point (aimed at his feet, at the vehicle he sits in, or through the map cursor).
	protected static IEntity Aimed(IEntity target, vector point, IEntity leader)
	{
		if (target != leader && ChimeraCharacter.Cast(target) && AIO_Server.IsAlive(target))
			return target;
		Find(point, AIM_SNAP);
		IEntity best;
		float bestDistance = AIM_SNAP + 1;
		foreach (IEntity found : s_aFound)
		{
			if (found == leader)
				continue;
			float distance = vector.Distance(found.GetOrigin(), point);
			if (distance < bestDistance)
			{
				best = found;
				bestDistance = distance;
			}
		}
		s_aFound.Clear();
		return best;
	}

	// Living soldiers within radius of the point, nearest first, into s_aFound.
	protected static void Find(vector point, float radius)
	{
		s_aFound.Clear();
		GetGame().GetWorld().QueryEntitiesBySphere(point, radius, FoundHit, null, EQueryEntitiesFlags.ALL);
		// Nearest first, so a squad limit takes the closest men.
		for (int i = 1; i < s_aFound.Count(); i++)
		{
			IEntity entity = s_aFound[i];
			float distance = vector.DistanceSq(entity.GetOrigin(), point);
			int j = i - 1;
			while (j >= 0 && vector.DistanceSq(s_aFound[j].GetOrigin(), point) > distance)
			{
				s_aFound[j + 1] = s_aFound[j];
				j--;
			}
			s_aFound[j + 1] = entity;
		}
	}

	protected static bool FoundHit(IEntity entity)
	{
		if (ChimeraCharacter.Cast(entity) && AIO_Server.IsAlive(entity) && !s_aFound.Contains(entity))
			s_aFound.Insert(entity);
		return true;
	}

	// ---------------------------------------------------------------- recruit

	protected static string RecruitNearby(SCR_AIGroup squad, IEntity leader, bool admin)
	{
		Find(leader.GetOrigin(), NEARBY);
		array<IEntity> men = {};
		int players;
		foreach (IEntity found : s_aFound)
		{
			if (found == leader || GroupOf(found) == squad)
				continue;
			string refusal = Refusal(found, squad, leader, admin, true);
			if (refusal.IsEmpty())
				men.Insert(found);
			else if (refusal.Contains("player"))
				players++;
		}
		s_aFound.Clear();
		if (men.IsEmpty())
		{
			if (players > 0)
				return "Nobody free within 30 m: the soldiers near you belong to players";
			return "No soldiers of your side within 30 m";
		}
		return Recruit(squad, leader, admin, men);
	}

	protected static string Recruit(SCR_AIGroup squad, IEntity leader, bool admin, array<IEntity> men)
	{
		int room = Room(squad, admin);
		if (room == 0)
			return "Your squad is full (server limit for AI in a squad)";
		int taken;
		int failed;
		string name;
		foreach (IEntity man : men)
		{
			if (room > 0 && taken >= room)
				break;
			if (Take(squad, leader, man))
			{
				taken++;
				if (name.IsEmpty())
					name = AIO_Server.NameOf(man);
			}
			else
				failed++;
		}
		if (taken == 0)
			return "Nobody could join your squad";
		string text = string.Format("%1 men joined your squad", taken);
		if (taken == 1)
			text = name + " joined your squad";
		int full = men.Count() - taken - failed;
		if (full > 0)
			text += string.Format("; %1 left out, squad full", full);
		if (failed > 0)
			text += string.Format("; %1 could not join", failed);
		return text;
	}

	// How many more AI a player's squad may take: the server's commanding limit, which the vanilla
	// recruit command keeps too. Admins and possessed AI squads have none. -1 = no limit.
	protected static int Room(SCR_AIGroup squad, bool admin)
	{
		if (admin || !squad.IsSlave())
			return -1;
		SCR_CommandingManagerComponent commanding = SCR_CommandingManagerComponent.GetInstance();
		if (!commanding || commanding.GetMaxAIPerGroup() < 0)
			return -1;
		int room = commanding.GetMaxAIPerGroup() - squad.GetServerAgentsCount();
		if (room < 0)
			return 0;
		return room;
	}

	// Moves one soldier out of his group into the squad and its formation. False if the game refused him.
	static bool Take(SCR_AIGroup squad, IEntity leader, IEntity man)
	{
		AIAgent agent = AIO_Tasks.AgentOf(man);
		if (!agent)
			return false;
		SCR_AIGroup old = SCR_AIGroup.Cast(agent.GetParentGroup());
		if (old == squad)
			return true;
		if (old)
			Leave(old, man);
		if (!squad.IsAIActivated())
			squad.ActivateAI();
		squad.AddAgentFromControlledEntity(man);
		if (agent.GetParentGroup() != squad)
		{
			if (old && !agent.GetParentGroup())
				old.AddAgent(agent); // back where he was rather than in no group at all
			return false;
		}
		if (squad.IsSlave())
			Listed(squad, man, true);
		DropOldActivity(agent);
		AIO_Tasks.Rejoin(man, leader);
		return true;
	}

	// ---------------------------------------------------------------- dismiss

	// The men leave together as one new AI squad of their side, where they stand, with no orders: they
	// stay put around their new leader until another leader recruits them (or a mission/mod commands
	// every free AI squad, as ACA does).
	protected static string Dismiss(SCR_AIGroup squad, array<IEntity> men)
	{
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = men[0].GetOrigin();
		SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().SpawnEntityPrefab(Resource.Load(NEW_GROUP), GetGame().GetWorld(), params));
		if (!group)
			return "Could not form a new squad for them";
		SCR_ChimeraCharacter first = SCR_ChimeraCharacter.Cast(men[0]);
		if (first && first.GetFaction())
			group.SetFaction(first.GetFaction());
		int moved;
		string name;
		foreach (IEntity man : men)
		{
			AIAgent agent = AIO_Tasks.AgentOf(man);
			if (!agent || agent.GetParentGroup() != squad)
				continue;
			Clean(man);
			Leave(squad, man);
			group.AddAgent(agent);
			if (agent.GetParentGroup() != group)
			{
				squad.AddAgentFromControlledEntity(man);
				if (squad.IsSlave())
					Listed(squad, man, true);
				continue;
			}
			DropOldActivity(agent);
			AIO_Tasks.Rejoin(man, null);
			moved++;
			if (name.IsEmpty())
				name = AIO_Server.NameOf(man);
		}
		if (moved == 0)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(group);
			return "Nobody could be dismissed";
		}
		if (moved == 1)
			return name + " dismissed; he waits there on his own";
		return string.Format("%1 men dismissed; they wait there as their own squad", moved);
	}

	// Ends everything AiO has a man of ours doing or set on him (tasks, per-man fire rules, behaviour
	// mode, stance and pace), so he leaves the squad or the game with nothing of ours left behind.
	static void Clean(IEntity man)
	{
		AIAgent agent = AIO_Tasks.AgentOf(man);
		if (!agent)
			return;
		AIO_Tasks.Release(man);
		AIO_Maneuvers.StopLead(agent);
		AIO_Maneuvers.SetROE(man, -1);
		AIO_Maneuvers.SetEngage(man, -1);
		if (AIO_Behaviour.ModeOf(agent) != AIO_EBehaviour.AWARE)
			AIO_Behaviour.Set(agent, AIO_EBehaviour.AWARE);
		AIO_Tasks.SetStance(agent, -1);
		AIO_Tasks.SetSpeed(agent, -1);
	}

	// ---------------------------------------------------------------- destroy

	// Removed from the game (no body left), the way the Game Master deletes a soldier. A soldier the
	// mission marks as not deletable stays.
	protected static string Destroy(array<IEntity> men)
	{
		int removed;
		int kept;
		string name;
		foreach (IEntity man : men)
		{
			if (!man || man.IsDeleted())
				continue;
			string who = AIO_Server.NameOf(man);
			SCR_EditableEntityComponent editable = SCR_EditableEntityComponent.Cast(man.FindComponent(SCR_EditableEntityComponent));
			if (editable && editable.HasEntityFlag(EEditableEntityFlag.NON_DELETABLE))
			{
				kept++;
				continue;
			}
			SCR_AIGroup old = GroupOf(man);
			Clean(man);
			if (old)
				Leave(old, man);
			if (editable)
				editable.Delete(false, false);
			else
			{
				SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(man.FindComponent(SCR_CompartmentAccessComponent));
				if (access && access.GetCompartment())
					access.GetCompartment().SetCompartmentAccessible(true);
				RplComponent.DeleteRplEntity(man, false);
			}
			removed++;
			if (name.IsEmpty())
				name = who;
		}
		string text = string.Format("%1 men destroyed", removed);
		if (removed == 1)
			text = name + " destroyed";
		if (removed == 0)
			text = "Nobody destroyed";
		if (kept > 0)
			text += string.Format("; %1 can't be removed in this mission", kept);
		return text;
	}

	// ---------------------------------------------------------------- group bookkeeping

	protected static void Leave(SCR_AIGroup group, IEntity man)
	{
		group.RemoveAgentFromControlledEntity(man);
		if (group.IsSlave())
			Listed(group, man, false);
	}

	// A player's squad keeps a replicated list of its AI members (group menu, "recruited" mark); the
	// vanilla recruit and dismiss commands update it through the groups manager, and so do we.
	protected static void Listed(SCR_AIGroup slave, IEntity man, bool add)
	{
		SCR_GroupsManagerComponent groups = SCR_GroupsManagerComponent.GetInstance();
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(man);
		if (!groups || !character)
			return;
		bool listed = slave.GetAIMembers() && slave.GetAIMembers().Contains(character);
		if (add == listed)
			return;
		RplId groupId = SCR_EntityHelper.EntityToRplId(slave);
		RplId manId = SCR_EntityHelper.EntityToRplId(man);
		if (add)
			groups.AskAddAiMemberToGroup(groupId, manId);
		else
			groups.AskRemoveAiMemberFromGroup(groupId, manId);
	}

	// Actions a man still runs for his old group's waypoint (defend, patrol, a move) end: his new group
	// gives its own. (AIO_Tasks.Rejoin does the same for formation actions only.)
	protected static void DropOldActivity(AIAgent agent)
	{
		SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(agent);
		if (!utility)
			return;
		array<ref AIActionBase> actions = {};
		utility.GetActions(actions);
		foreach (AIActionBase action : actions)
		{
			if (action && action.GetRelatedGroupActivity())
			{
				action.SetRemoveAction(true);
				action.Fail();
			}
		}
	}
}
