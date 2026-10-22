// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). Licensed under the GNU GPL v3 or later (see LICENSE).

// Leadership orders use only the caller's own group, never the aimed or selected soldiers.
class AIO_Leadership
{
	static string Execute(SCR_PlayerController pc, bool become)
	{
		IEntity body = pc.GetControlledEntity();
		if (!AIO_Server.IsAlive(body))
			return "You must be alive to change leadership";

		// GM possession: change the real AI formation leader within this soldier's group.
		SCR_AIGroup possessed = AIO_Server.PossessedGroup(pc.GetPlayerId());
		if (possessed)
			return ChangeAILeader(possessed, body, become);

		SCR_GroupsManagerComponent groups = SCR_GroupsManagerComponent.GetInstance();
		if (!groups)
			return "No squad available";
		SCR_AIGroup group = groups.GetPlayerGroup(pc.GetPlayerId());
		if (!group || !group.IsPlayerInGroup(pc.GetPlayerId()))
			return "Join a squad first";
		int playerId = pc.GetPlayerId();
		if (become)
		{
			if (group.IsPlayerLeader(playerId))
				return "You are already the squad leader";
			if (group.GetLeaderID() > 0)
				return "Another player leads this squad; ask them to step down";
			group.SetGroupLeader(playerId);
			return "You are now the squad leader";
		}
		if (!group.IsPlayerLeader(playerId))
			return "You are not the squad leader";
		foreach (int successor : group.GetPlayerIDs())
		{
			if (successor <= 0 || successor == playerId)
				continue;
			group.SetGroupLeader(successor);
			return "Stepped down; leadership passed to another squad member";
		}
		group.AIO_ClearPlayerLeader(playerId);
		return "Stepped down; squad leadership is vacant";
	}

	// Server only; separate from player-group leadership, which belongs to a player ID.
	static string ChangeAILeader(SCR_AIGroup group, IEntity body, bool become)
	{
		AIAgent agent = AIO_Tasks.AgentOf(body);
		if (!Replication.IsServer() || !group || group.IsPlayable() || group.IsSlave() || !agent || agent.GetParentGroup() != group || !AIO_Server.IsAlive(body))
			return "No independent AI squad available";
		AIAgent leader = group.GetLeaderAgent();
		if (become)
		{
			if (leader == agent)
				return "You are already the squad leader";
			if (leader && GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(leader.GetControlledEntity()) > 0)
				return "Another player leads this squad; ask them to step down";
			group.SetNewLeader(agent);
			if (group.GetLeaderAgent() != agent)
				return "The squad could not change leader";
			return "You are now the squad leader";
		}
		if (leader != agent)
			return "You are not the squad leader";
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent successor : agents)
		{
			if (!successor || successor == agent)
				continue;
			IEntity member = successor.GetControlledEntity();
			if (!AIO_Server.IsAlive(member) || GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(member) > 0)
				continue;
			group.SetNewLeader(successor);
			if (group.GetLeaderAgent() != successor)
				return "The squad could not change leader";
			return "Stepped down; an AI squadmate now leads";
		}
		return "No living AI squadmate can take over";
	}
}

modded class SCR_AIGroup
{
	// Native SetGroupLeader always announces a promoted player; clearing needs no promotion notice.
	// Match its replicated leader update and group-attribute reset without announcing player -1.
	void AIO_ClearPlayerLeader(int playerId)
	{
		if (!Replication.IsServer() || !IsPlayerLeader(playerId))
			return;
		SetCustomName("", 0);
		SetCustomDescription("", 0);
		RPC_SetLeaderID(-1);
		Rpc(RPC_SetLeaderID, -1);
	}
}
