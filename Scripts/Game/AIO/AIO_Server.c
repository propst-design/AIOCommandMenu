// All-in-One Command Menu for Arma Reforger
// Based on Leopard20's All-In-One Command Menu for Arma 3 (GPLv3); rebuilt in Enforce Script.
// Copyright (C) 2026 Jacob (propst-design). Licensed under the GNU GPL v3 or later (see LICENSE).

// Server side: who the player commands, carrying out orders, the admin page.
// It works on the game's own squad setup (the player's group and its linked AI group),
// so it behaves the same in Conflict, Game Master and other mods that use it.
class AIO_Server
{
	static const ResourceName WP_MOVE = "{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et";

	// ---------------------------------------------------------------- squad

	// The AI group the player leads (the game's "slave" group), or null.
	static SCR_AIGroup Slave(int playerId)
	{
		SCR_GroupsManagerComponent groups = SCR_GroupsManagerComponent.GetInstance();
		if (!groups)
			return null;
		SCR_AIGroup group = groups.GetPlayerGroup(playerId);
		if (!group || !group.IsPlayerLeader(playerId))
			return null;
		return group.GetSlave();
	}

	static void Members(SCR_AIGroup slave, notnull array<IEntity> members)
	{
		if (!slave)
			return;
		array<AIAgent> agents = {};
		slave.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;
			IEntity body = agent.GetControlledEntity();
			if (body && IsAlive(body))
				members.Insert(body);
		}
	}

	static bool IsAlive(IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character)
			return false;
		CharacterControllerComponent controller = character.GetCharacterController();
		return controller && !controller.IsDead();
	}

	static void Roster(int playerId, notnull array<RplId> ids, notnull array<string> names, notnull array<int> health)
	{
		array<IEntity> members = {};
		Members(Slave(playerId), members);
		foreach (IEntity body : members)
		{
			ids.Insert(Replication.FindItemId(body));
			names.Insert(NameOf(body));
			int percent = 100;
			SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(body);
			if (damage)
				percent = Math.Round(damage.GetHealthScaled() * 100);
			health.Insert(percent);
		}
	}

	static string NameOf(IEntity body)
	{
		CharacterIdentityComponent identity = CharacterIdentityComponent.Cast(body.FindComponent(CharacterIdentityComponent));
		if (identity && identity.GetIdentity())
		{
			string surname = identity.GetIdentity().GetSurname();
			if (!surname.IsEmpty())
				return surname;
			return identity.GetIdentity().GetFullName();
		}
		return "Soldier";
	}

	// ---------------------------------------------------------------- orders

	static string Execute(SCR_PlayerController pc, int order, array<RplId> members, vector point, RplId target)
	{
		int playerId = pc.GetPlayerId();
		if (order >= AIO_EOrder.ADMIN_HEAL_ME)
			return Admin(pc, order, point, target);

		SCR_AIGroup slave = Slave(playerId);
		if (!slave)
			return "No soldiers under your command";
		if (members && !members.IsEmpty())
			return "Orders to selected men come in the next build (whole squad only for now)";

		IEntity player = pc.GetControlledEntity();
		switch (order)
		{
			case AIO_EOrder.HALT:
				if (Vanilla(playerId, slave, "halt", point))
					return "";
				ClearWaypoints(slave);
				return "Halt";
			case AIO_EOrder.FOLLOW:
				if (Vanilla(playerId, slave, "follow", point))
					return "";
				ClearWaypoints(slave);
				return "Following you";
			case AIO_EOrder.MOVE_HERE:
				if (Vanilla(playerId, slave, "waypointMove", point))
					return "";
				ClearWaypoints(slave);
				AddWaypoint(slave, WP_MOVE, point);
				return "Moving";
			case AIO_EOrder.HOLD_HERE:
				if (player && Vanilla(playerId, slave, "waypointDefend", player.GetOrigin()))
					return "";
				return "Hold here needs the game's commanding (not in this scenario)";
			case AIO_EOrder.SUPPRESS:
				if (Vanilla(playerId, slave, "waypointSuppress", point))
					return "";
				return "Suppress needs the game's commanding (not in this scenario)";
			case AIO_EOrder.HOLD_FIRE:
				if (Vanilla(playerId, slave, "holdFireCommand", point))
					return "";
				return "Hold fire needs the game's commanding (not in this scenario)";
			case AIO_EOrder.FIRE_AT_WILL:
				if (Vanilla(playerId, slave, "fireAtWillCommand", point))
					return "";
				return "Fire at will needs the game's commanding (not in this scenario)";
			case AIO_EOrder.RETURN_FIRE:
				if (Vanilla(playerId, slave, "returnFireCommand", point))
					return "";
				return "Return fire needs the game's commanding (not in this scenario)";
		}
		return "Not built yet: " + typename.EnumToString(AIO_EOrder, order);
	}

	// Runs one of the game's own commanding-menu commands for the player (voice reply,
	// gesture and all). False when the scenario has no commanding or no such command.
	static bool Vanilla(int playerId, SCR_AIGroup slave, string command, vector point)
	{
		SCR_CommandingManagerComponent commanding = SCR_CommandingManagerComponent.GetInstance();
		if (!commanding)
			return false;
		int index = commanding.FindCommandIndex(command);
		if (index < 0)
			return false;
		RplId groupId = Replication.FindItemId(slave);
		commanding.RequestCommandExecution(index, RplId.Invalid(), groupId, point, playerId, Math.RandomFloatInclusive(0, 1));
		Print(string.Format("[AIO] player %1: %2 at %3", playerId, command, point));
		return true;
	}

	static void ClearWaypoints(SCR_AIGroup group)
	{
		array<AIWaypoint> waypoints = {};
		group.GetWaypoints(waypoints);
		foreach (AIWaypoint waypoint : waypoints)
		{
			group.RemoveWaypoint(waypoint);
			if (waypoint)
				SCR_EntityHelper.DeleteEntityAndChildren(waypoint);
		}
	}

	static void AddWaypoint(SCR_AIGroup group, ResourceName prefab, vector point)
	{
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = point;
		AIWaypoint waypoint = AIWaypoint.Cast(GetGame().SpawnEntityPrefab(Resource.Load(prefab), GetGame().GetWorld(), params));
		if (waypoint)
			group.AddWaypoint(waypoint);
	}

	// ---------------------------------------------------------------- admin page

	// Single player and the host of a listen server always; on servers, logged-in admins.
	// Host option: -aioCheats all | admins (default) | off
	static bool AdminAllowed(int playerId)
	{
		string option;
		System.GetCLIParam("aioCheats", option);
		option.ToLower();
		if (option == "off")
			return false;
		if (option == "all")
			return true;
		if (RplSession.Mode() == RplMode.None)
			return true;
		if (RplSession.Mode() != RplMode.Dedicated)
		{
			PlayerController host = GetGame().GetPlayerController();
			if (host && host.GetPlayerId() == playerId)
				return true;
		}
		return SCR_Global.IsAdmin(playerId);
	}

	static string Admin(SCR_PlayerController pc, int order, vector point, RplId target)
	{
		int playerId = pc.GetPlayerId();
		if (!AdminAllowed(playerId))
			return "Admin page: not allowed";
		IEntity player = pc.GetControlledEntity();
		array<IEntity> members = {};
		Members(Slave(playerId), members);
		switch (order)
		{
			case AIO_EOrder.ADMIN_HEAL_ME:
				Heal(player);
				return "Healed";
			case AIO_EOrder.ADMIN_HEAL_SQUAD:
				Heal(player);
				foreach (IEntity member : members)
					Heal(member);
				return string.Format("Healed you and %1 men", members.Count());
			case AIO_EOrder.ADMIN_TELEPORT_ME:
				if (SCR_Global.TeleportPlayer(playerId, point))
					return "Teleported";
				return "Can't teleport there";
			case AIO_EOrder.ADMIN_TELEPORT_SQUAD:
				if (!player)
					return "No body to teleport to";
				int n;
				foreach (IEntity soldier : members)
				{
					BaseGameEntity body = BaseGameEntity.Cast(soldier);
					ChimeraCharacter character = ChimeraCharacter.Cast(soldier);
					if (!body || (character && character.IsInVehicle()))
						continue;
					vector transform[4];
					player.GetWorldTransform(transform);
					float angle = n * 0.9;
					transform[3] = transform[3] + Vector(Math.Cos(angle) * 3, 0, Math.Sin(angle) * 3);
					transform[3][1] = GetGame().GetWorld().GetSurfaceY(transform[3][0], transform[3][2]);
					body.Teleport(transform);
					n++;
				}
				return string.Format("%1 men brought to you", n);
			case AIO_EOrder.ADMIN_KILL_TARGET:
				IEntity victim = null;
				RplComponent rpl = RplComponent.Cast(Replication.FindItem(target));
				if (rpl)
					victim = rpl.GetEntity();
				if (!victim || victim == player)
					return "Nothing to kill there";
				SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(victim);
				if (!damage)
					return "That can't be killed";
				damage.Kill(Instigator.CreateInstigator(player));
				return "Killed";
		}
		return "";
	}

	static void Heal(IEntity entity)
	{
		if (!entity)
			return;
		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(entity);
		if (damage)
			damage.FullHeal();
	}
}
