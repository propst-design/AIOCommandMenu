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
		if (groups)
		{
			SCR_AIGroup group = groups.GetPlayerGroup(playerId);
			if (group && group.IsPlayerLeader(playerId) && group.GetSlave())
				return group.GetSlave();
		}
		return PossessedGroup(playerId);
	}

	// Game Master "take control" of an AI soldier (usually the leader): his own AI group is the squad.
	// Never a playable group (that one holds players, not soldiers to command).
	static SCR_AIGroup PossessedGroup(int playerId)
	{
		IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		AIAgent agent = AIO_Tasks.AgentOf(body);
		if (!agent)
			return null;
		SCR_AIGroup group = SCR_AIGroup.Cast(agent.GetParentGroup());
		if (!group || group.IsPlayable() || group.IsSlave())
			return null;
		return group;
	}

	// True when the squad is a possessed AI group (no slave setup, no vanilla commanding for it).
	static bool IsPossessed(SCR_AIGroup squad)
	{
		return squad && !squad.IsSlave();
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

	static void Roster(int playerId, notnull array<RplId> ids, notnull array<string> names, notnull array<int> health, notnull array<string> tasks, notnull array<int> ammo)
	{
		array<IEntity> members = {};
		Members(Slave(playerId), members);
		members.RemoveItem(GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId)); // possessed leader
		foreach (IEntity body : members)
		{
			ids.Insert(Replication.FindItemId(body));
			names.Insert(NameOf(body));
			int percent = 100;
			SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(body);
			if (damage)
				percent = Math.Round(damage.GetHealthScaled() * 100);
			health.Insert(percent);
			tasks.Insert(AIO_Tasks.TaskLabel(body));
			ammo.Insert(AIO_Supplies.MagazineCount(body));
		}
	}

	static string OrderStatus(int playerId)
	{
		string text = AIO_Routes.Status(playerId);
		string bound = AIO_Bounding.Status(playerId);
		string treatment = AIO_Support.Status(playerId);
		if (!bound.IsEmpty())
		{
			if (!text.IsEmpty()) text += "\n";
			text += bound;
		}
		if (!treatment.IsEmpty())
		{
			if (!text.IsEmpty()) text += "\n";
			text += treatment;
		}
		return text;
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
		if ((order >= AIO_EOrder.ADMIN_HEAL_ME && order <= AIO_EOrder.ADMIN_KILL_TARGET)
			|| order == AIO_EOrder.ADMIN_TELEPORT_MAP
			|| order == AIO_EOrder.ADMIN_TELEPORT_GROUP
			|| order == AIO_EOrder.ADMIN_TELEPORT_VEHICLE)
			return Admin(pc, order, point, target);
		if (order == AIO_EOrder.TOGGLE_SQUAD_BAR || AIO_AimMarker.IsOption(order))
			return "";

		if (order == AIO_EOrder.BECOME_LEADER || order == AIO_EOrder.STEP_DOWN_LEADER)
			return AIO_Leadership.Execute(pc, order == AIO_EOrder.BECOME_LEADER);

		SCR_AIGroup slave = Slave(playerId);
		if (!slave)
			return "No soldiers under your command";
		if (order == AIO_EOrder.ROUTE_RESUME || order == AIO_EOrder.ROUTE_SKIP)
		{
			array<IEntity> routeMen = {};
			if (members && !members.IsEmpty()) AIO_Tasks.Resolve(slave, members, routeMen);
			else Members(slave, routeMen);
			routeMen.RemoveItem(pc.GetControlledEntity());
			return AIO_Routes.Recover(routeMen, playerId, order == AIO_EOrder.ROUTE_SKIP);
		}
		if (AIO_Support.IsOrder(order))
		{
			array<IEntity> supportMen = {};
			if (members && !members.IsEmpty())
				AIO_Tasks.Resolve(slave, members, supportMen);
			else
				Members(slave, supportMen);
			supportMen.RemoveItem(pc.GetControlledEntity());
			if (supportMen.IsEmpty())
				return "No soldiers selected under your command";
			return AIO_Support.Order(slave, supportMen, pc.GetControlledEntity(), order, point, EntityOf(target), playerId);
		}
		if (AIO_Explosives.IsOrder(order))
		{
			array<IEntity> demoMen = {};
			if (members && !members.IsEmpty())
				AIO_Tasks.Resolve(slave, members, demoMen);
			else
				Members(slave, demoMen);
			demoMen.RemoveItem(pc.GetControlledEntity());
			return AIO_Explosives.Order(demoMen, pc.GetControlledEntity(), order, point, EntityOf(target), playerId);
		}
		if (AIO_Grenades.IsOrder(order))
		{
			array<IEntity> throwers = {};
			if (members && !members.IsEmpty())
				AIO_Tasks.Resolve(slave, members, throwers);
			else
				Members(slave, throwers);
			throwers.RemoveItem(pc.GetControlledEntity());
			return AIO_Grenades.Order(throwers, pc.GetControlledEntity(), order, point, playerId);
		}
		if (AIO_Repair.IsOrder(order) || AIO_FireSupport.IsOrder(order) || AIO_Logistics.IsOrder(order))
		{
			array<IEntity> helpers = {};
			if (members && !members.IsEmpty())
				AIO_Tasks.Resolve(slave, members, helpers);
			else
				Members(slave, helpers);
			return Specialist(helpers, pc.GetControlledEntity(), order, point, target, playerId);
		}
		if (AIO_Behaviour.IsOrder(order))
		{
			array<IEntity> modeMen = {};
			if (members && !members.IsEmpty())
				AIO_Tasks.Resolve(slave, members, modeMen);
			else
				Members(slave, modeMen);
			modeMen.RemoveItem(pc.GetControlledEntity());
			if (modeMen.IsEmpty())
				return "No soldiers selected under your command";
			return AIO_Behaviour.Order(modeMen, order);
		}
		// Formations belong to the whole squad, also when men are selected.
		if (AIO_Formations.IsFormationOrder(order))
			return AIO_Formations.Set(slave, order);
		if (members && !members.IsEmpty())
			return ExecuteMen(slave, order, members, point, pc.GetControlledEntity(), target, playerId);
		// A possessed AI group isn't set up for the game's commanding: every man gets the order himself.
		if (IsPossessed(slave))
		{
			array<IEntity> everyone = {};
			Members(slave, everyone);
			everyone.RemoveItem(pc.GetControlledEntity());
			if (everyone.IsEmpty())
				return "No soldiers under your command";
			if (order == AIO_EOrder.FOLLOW || order == AIO_EOrder.DEF_GARRISON || order == AIO_EOrder.DEF_CLEAR)
				ClearWaypoints(slave); // a GM waypoint must not keep reissuing the previous move
			return ExecuteBodies(slave, order, everyone, point, pc.GetControlledEntity(), target, playerId);
		}
		// Whole-squad movement orders take over from any single-man tasks.
		if (order == AIO_EOrder.DEF_GARRISON || order == AIO_EOrder.DEF_CLEAR)
			ClearWaypoints(slave);
		if (order == AIO_EOrder.HALT || order == AIO_EOrder.FOLLOW || order == AIO_EOrder.MOVE_HERE || order == AIO_EOrder.HOLD_HERE)
			AIO_Tasks.ReleaseGroup(slave);
		if (order == AIO_EOrder.FOLLOW)
		{
			array<IEntity> strays = {};
			Members(slave, strays);
			foreach (IEntity stray : strays)
				AIO_Tasks.Rejoin(stray, pc.GetControlledEntity());
		}

		IEntity player = pc.GetControlledEntity();
		string moved = MoveMenu(slave, player, order, null, playerId);
		if (!moved.IsEmpty())
			return moved;
		string infantry = Infantry(slave, player, order, null);
		if (!infantry.IsEmpty())
			return infantry;
		string supply = Supply(slave, player, order, null, target, playerId);
		if (!supply.IsEmpty())
			return supply;
		string tactics = Tactics(slave, player, order, null, point, target, playerId);
		if (!tactics.IsEmpty())
			return tactics;
		if (AIO_Vehicles.IsVehicleOrder(order))
		{
			array<IEntity> squad = {};
			Members(slave, squad);
			if (player)
				squad.RemoveItem(player);
			if (squad.IsEmpty())
				return "No soldiers under your command";
			return AIO_Vehicles.Order(squad, slave, order, EntityOf(target), player, point);
		}
		// Whole-squad rules of engagement: per-man overrides end.
		if (order == AIO_EOrder.HOLD_FIRE || order == AIO_EOrder.FIRE_AT_WILL || order == AIO_EOrder.RETURN_FIRE)
		{
			array<IEntity> everyone = {};
			Members(slave, everyone);
			foreach (IEntity soldier : everyone)
			{
				AIO_Maneuvers.StopLead(AIO_Tasks.AgentOf(soldier));
				AIO_Maneuvers.SetROE(soldier, -1);
			}
		}
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
				{
					TrackWholeMove(slave, player, point, playerId);
					return "";
				}
				ClearWaypoints(slave);
				AddWaypoint(slave, WP_MOVE, point);
				TrackWholeMove(slave, player, point, playerId);
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
				return GroupROE(slave, EAIGroupCombatMode.HOLD_FIRE, "Holding fire");
			case AIO_EOrder.FIRE_AT_WILL:
				if (Vanilla(playerId, slave, "fireAtWillCommand", point))
					return "";
				return GroupROE(slave, EAIGroupCombatMode.FIRE_AT_WILL, "Fire at will");
			case AIO_EOrder.RETURN_FIRE:
				if (Vanilla(playerId, slave, "returnFireCommand", point))
					return "";
				return GroupROE(slave, EAIGroupCombatMode.RETURN_FIRE, "Return fire only");
		}
		return "Not built yet: " + typename.EnumToString(AIO_EOrder, order);
	}

	static void TrackWholeMove(SCR_AIGroup group, IEntity player, vector point, int playerId)
	{
		array<IEntity> men = {};
		Members(group, men);
		men.RemoveItem(player);
		AIO_Routes.TrackMove(group, player, men, point, playerId, true);
	}

	static const float SHIFT_DISTANCE = 50; // Advance / Stay back / Flank, as in Arma 3
	static const float FALL_BACK_DISTANCE = 40;

	// Move menu: Advance, Stay back, Flank left/right keep station on the player; Fall back goes to
	// a spot behind him and holds, with smoke between the men and the enemy when in contact.
	// men = null means the whole squad. Empty string = not a Move order.
	static string MoveMenu(SCR_AIGroup slave, IEntity player, int order, array<IEntity> men, int playerId = 0)
	{
		if (order != AIO_EOrder.ADVANCE && order != AIO_EOrder.STAY_BACK && order != AIO_EOrder.FLANK_LEFT
			&& order != AIO_EOrder.FLANK_RIGHT && order != AIO_EOrder.FALL_BACK)
			return "";
		if (!player)
			return "You need to be on foot in the world to give that order";
		array<IEntity> bodies = {};
		if (men)
			bodies.Copy(men);
		else
			Members(slave, bodies);
		bodies.RemoveItem(player);
		if (bodies.IsEmpty())
			return "No soldiers under your command";
		vector forward, right;
		AIO_Tasks.Facing(player, forward, right);
		vector offset;
		vector across = right;
		string what;
		if (order == AIO_EOrder.ADVANCE)
		{
			offset = forward * SHIFT_DISTANCE;
			what = "advancing";
		}
		else if (order == AIO_EOrder.STAY_BACK)
		{
			offset = forward * -SHIFT_DISTANCE;
			what = "staying back";
		}
		else if (order == AIO_EOrder.FLANK_LEFT)
		{
			offset = right * -SHIFT_DISTANCE;
			across = forward;
			what = "flanking left";
		}
		else if (order == AIO_EOrder.FLANK_RIGHT)
		{
			offset = right * SHIFT_DISTANCE;
			across = forward;
			what = "flanking right";
		}
		else
		{
			offset = forward * -FALL_BACK_DISTANCE;
			what = "falling back";
		}
		int count;
		for (int i = 0; i < bodies.Count(); i++)
		{
			vector slot = AIO_Tasks.LineOffset(offset, across, i, bodies.Count());
			bool ok;
			if (order == AIO_EOrder.FALL_BACK)
			{
				vector spot = player.GetOrigin() + slot;
				spot[1] = GetGame().GetWorld().GetSurfaceY(spot[0], spot[2]);
				ok = AIO_Tasks.MoveTo(bodies[i], spot);
			}
			else
			{
				ok = AIO_Tasks.KeepOffset(bodies[i], player, slot);
			}
			if (ok)
				count++;
		}
		string smoke;
		if (order == AIO_EOrder.FALL_BACK && count > 0)
			smoke = AIO_Grenades.Cover(bodies, AIO_Grenades.ThreatPoint(bodies, player.GetOrigin() + forward * SHIFT_DISTANCE), 2, playerId);
		return string.Format("%1 men %2", count, what) + smoke;
	}

	static IEntity EntityOf(RplId id)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
		if (!rpl)
			return null;
		return rpl.GetEntity();
	}

	// Group-wide rules of engagement without the game's commanding (fallback).
	static string GroupROE(SCR_AIGroup slave, EAIGroupCombatMode mode, string what)
	{
		SCR_AIGroupUtilityComponent utility = slave.GetGroupUtilityComponent();
		if (!utility)
			return "Can't change the squad's rules of engagement here";
		utility.SetCombatMode(mode);
		return what;
	}

	// Attack, Defense, speed (AIO_Maneuvers.c). men = null means the whole squad.
	static string Tactics(SCR_AIGroup slave, IEntity player, int order, array<IEntity> men, vector point, RplId target, int playerId)
	{
		bool speed = order == AIO_EOrder.SPEED_WALK || order == AIO_EOrder.SPEED_NORMAL || order == AIO_EOrder.SPEED_FAST || order == AIO_EOrder.SPEED_AUTO;
		if (!speed && !AIO_Maneuvers.IsAttackOrder(order) && !AIO_Maneuvers.IsDefenseOrder(order))
			return "";
		// Whole squad: the game's own suppress and search-and-destroy are group behaviours that work well.
		if (!men && order == AIO_EOrder.SUPPRESS)
			return "";
		if (!men && order == AIO_EOrder.ASSAULT && point != vector.Zero)
		{
			AIO_Tasks.ReleaseGroup(slave);
			if (Vanilla(playerId, slave, "waypointSearchAndDestroy", point))
				return "";
		}
		array<IEntity> bodies = {};
		if (men)
			bodies.Copy(men);
		else
			Members(slave, bodies);
		if (player)
			bodies.RemoveItem(player);
		if (bodies.IsEmpty())
			return "No soldiers under your command";
		if (speed)
		{
			int type = -1;
			string what = "choosing their own pace";
			if (order == AIO_EOrder.SPEED_WALK)
			{
				type = EMovementType.WALK;
				what = "walking";
			}
			else if (order == AIO_EOrder.SPEED_NORMAL)
			{
				type = EMovementType.RUN;
				what = "running";
			}
			else if (order == AIO_EOrder.SPEED_FAST)
			{
				type = EMovementType.SPRINT;
				what = "sprinting";
			}
			foreach (IEntity mover : bodies)
				AIO_Tasks.SetSpeed(AIO_Tasks.AgentOf(mover), type);
			return string.Format("%1 men %2", bodies.Count(), what);
		}
		if (AIO_Maneuvers.IsDefenseOrder(order))
			return AIO_Maneuvers.Defense(bodies, player, order, point, EntityOf(target));
		return AIO_Maneuvers.Attack(bodies, player, order, point, playerId);
	}

	// Rearm, Rearm from that, Take that weapon (AIO_Supplies.c). men = null means the whole squad.
	static string Supply(SCR_AIGroup slave, IEntity player, int order, array<IEntity> men, RplId target, int playerId)
	{
		if (order != AIO_EOrder.REARM && order != AIO_EOrder.REARM_FROM && order != AIO_EOrder.TAKE_WEAPON)
			return "";
		array<IEntity> bodies = {};
		if (men)
			bodies.Copy(men);
		else
			Members(slave, bodies);
		if (player)
			bodies.RemoveItem(player);
		if (bodies.IsEmpty())
			return "No soldiers under your command";
		if (order == AIO_EOrder.REARM)
			return AIO_Supplies.Rearm(bodies, null, playerId);
		IEntity looked = EntityOf(target);
		if (!looked)
			return "Look at a body, crate, vehicle or weapon";
		// Something lying loose is its own source; anything else (a weapon on a body, a wheel,
		// a crate's lid) belongs to the top entity.
		IEntity source = looked;
		if (!AIO_Supplies.IsLooseItem(looked) && looked.GetRootParent())
			source = looked.GetRootParent();
		if (order == AIO_EOrder.REARM_FROM)
			return AIO_Supplies.Rearm(bodies, source, playerId);
		return AIO_Supplies.TakeWeapon(bodies, source, looked, playerId);
	}

	// Take cover, Stance, Copy my stance, Heal up. men = null means the whole squad.
	// Empty string = not one of these orders.
	static string Infantry(SCR_AIGroup slave, IEntity player, int order, array<IEntity> men)
	{
		if (order != AIO_EOrder.TAKE_COVER && order != AIO_EOrder.HEAL_UP && order != AIO_EOrder.COPY_STANCE
			&& order != AIO_EOrder.STANCE_UP && order != AIO_EOrder.STANCE_CROUCH && order != AIO_EOrder.STANCE_PRONE && order != AIO_EOrder.STANCE_AUTO)
			return "";
		array<IEntity> bodies = {};
		if (men)
			bodies.Copy(men);
		else
			Members(slave, bodies);
		if (player)
			bodies.RemoveItem(player);
		if (bodies.IsEmpty())
			return "No soldiers under your command";

		if (order == AIO_EOrder.TAKE_COVER)
		{
			if (!player)
				return "You need to be on foot in the world to give that order";
			vector forward, right;
			AIO_Tasks.Facing(player, forward, right);
			vector threat = player.GetOrigin() + forward * 150;
			int inCover;
			foreach (IEntity coverer : bodies)
			{
				if (AIO_Tasks.TakeCover(coverer, threat))
					inCover++;
			}
			int crouching = bodies.Count() - inCover;
			if (crouching == 0)
				return string.Format("%1 men taking cover", inCover);
			return string.Format("%1 men taking cover, %2 found none and crouch in place", inCover, crouching);
		}
		if (order == AIO_EOrder.HEAL_UP)
		{
			int treating, unhurt, noKit;
			foreach (IEntity patient : bodies)
			{
				int result = AIO_Tasks.HealSelf(patient);
				if (result > 0)
					treating++;
				else if (result == 0)
					unhurt++;
				else
					noKit++;
			}
			string reply = string.Format("%1 men treating wounds", treating);
			if (noKit > 0)
				reply += string.Format(", %1 hurt with nothing to treat with", noKit);
			if (treating == 0 && noKit == 0)
				reply = "Nobody is hurt";
			return reply;
		}
		if (order == AIO_EOrder.COPY_STANCE)
		{
			if (!player)
				return "You need to be on foot in the world to give that order";
			foreach (IEntity copier : bodies)
				AIO_Tasks.CopyStance(copier, player);
			return string.Format("%1 men copying your stance", bodies.Count());
		}
		int stance = -1;
		string what = "choosing their own stance";
		if (order == AIO_EOrder.STANCE_UP)
		{
			stance = ECharacterStance.STAND;
			what = "standing";
		}
		else if (order == AIO_EOrder.STANCE_CROUCH)
		{
			stance = ECharacterStance.CROUCH;
			what = "crouching";
		}
		else if (order == AIO_EOrder.STANCE_PRONE)
		{
			stance = ECharacterStance.PRONE;
			what = "going prone";
		}
		foreach (IEntity body : bodies)
		{
			AIAgent agent = AIO_Tasks.AgentOf(body);
			AIO_Tasks.StopCopying(agent);
			AIO_Tasks.SetStance(agent, stance);
		}
		return string.Format("%1 men %2", bodies.Count(), what);
	}

	// Repair, fire missions, Give me ... (AIO_Repair.c, AIO_FireSupport.c, AIO_Logistics.c).
	static string Specialist(array<IEntity> bodies, IEntity player, int order, vector point, RplId target, int playerId)
	{
		bodies.RemoveItem(player);
		if (AIO_FireSupport.IsOrder(order))
			return AIO_FireSupport.Order(bodies, player, order, point, playerId);
		if (bodies.IsEmpty())
			return "No soldiers under your command";
		if (AIO_Repair.IsOrder(order))
			return AIO_Repair.Order(bodies, EntityOf(target), player, point, playerId);
		return AIO_Logistics.Order(bodies, player, order, playerId);
	}

	// Orders to the selected men only (AIO_Tasks.c).
	static string ExecuteMen(SCR_AIGroup slave, int order, array<RplId> ids, vector point, IEntity player, RplId target, int playerId)
	{
		array<IEntity> bodies = {};
		AIO_Tasks.Resolve(slave, ids, bodies);
		if (bodies.IsEmpty())
		{
			array<IEntity> members = {};
			Members(slave, members);
			Print(string.Format("[AIO] order %1: none of %2 ids matched the %3 men of %4 (possessed %5)", typename.EnumToString(AIO_EOrder, order), ids.Count(), members.Count(), slave, IsPossessed(slave)));
			return "Those men are no longer in your squad";
		}
		return ExecuteBodies(slave, order, bodies, point, player, target, playerId);
	}

	static string ExecuteBodies(SCR_AIGroup slave, int order, array<IEntity> bodies, vector point, IEntity player, RplId target, int playerId)
	{
		bodies.RemoveItem(player);
		if (bodies.IsEmpty())
			return "No soldiers under your command";
		if (AIO_Support.IsOrder(order))
			return AIO_Support.Order(slave, bodies, player, order, point, EntityOf(target), playerId);
		if (AIO_Behaviour.IsOrder(order))
			return AIO_Behaviour.Order(bodies, order);
		if (AIO_Explosives.IsOrder(order))
			return AIO_Explosives.Order(bodies, player, order, point, EntityOf(target), playerId);
		if (AIO_Grenades.IsOrder(order))
			return AIO_Grenades.Order(bodies, player, order, point, playerId);
		if (AIO_Repair.IsOrder(order) || AIO_FireSupport.IsOrder(order) || AIO_Logistics.IsOrder(order))
			return Specialist(bodies, player, order, point, target, playerId);
		int count;
		string moved = MoveMenu(slave, player, order, bodies, playerId);
		if (!moved.IsEmpty())
			return moved;
		string infantry = Infantry(slave, player, order, bodies);
		if (!infantry.IsEmpty())
			return infantry;
		string supply = Supply(slave, player, order, bodies, target, playerId);
		if (!supply.IsEmpty())
			return supply;
		string tactics = Tactics(slave, player, order, bodies, point, target, playerId);
		if (!tactics.IsEmpty())
			return tactics;
		if (AIO_Vehicles.IsVehicleOrder(order))
			return AIO_Vehicles.Order(bodies, slave, order, EntityOf(target), player, point);
		if (order == AIO_EOrder.MOVE_HERE)
		{
			array<IEntity> moving = {};
			for (int i = 0; i < bodies.Count(); i++)
			{
				if (AIO_Tasks.MoveTo(bodies[i], AIO_Tasks.SpreadSpot(point, i), true))
				{
					count++;
					moving.Insert(bodies[i]);
				}
			}
			AIO_Routes.TrackMove(slave, player, moving, point, playerId, false);
			return string.Format("%1 men moving", count);
		}
		if (order == AIO_EOrder.HALT || order == AIO_EOrder.HOLD_HERE)
		{
			foreach (IEntity holder : bodies)
			{
				if (AIO_Tasks.HoldHere(holder))
					count++;
			}
			return string.Format("%1 men holding", count);
		}
		if (order == AIO_EOrder.FOLLOW)
		{
			foreach (IEntity follower : bodies)
				AIO_Tasks.Rejoin(follower, player);
			return string.Format("%1 men rejoining", bodies.Count());
		}
		if (order == AIO_EOrder.HOLD_FIRE || order == AIO_EOrder.FIRE_AT_WILL || order == AIO_EOrder.RETURN_FIRE)
		{
			int mode = EAIGroupCombatMode.FIRE_AT_WILL;
			string what = "firing at will";
			if (order == AIO_EOrder.HOLD_FIRE)
			{
				mode = EAIGroupCombatMode.HOLD_FIRE;
				what = "holding fire";
			}
			else if (order == AIO_EOrder.RETURN_FIRE)
			{
				mode = EAIGroupCombatMode.RETURN_FIRE;
				what = "returning fire only";
			}
			foreach (IEntity rifleman : bodies)
			{
				AIO_Maneuvers.StopLead(AIO_Tasks.AgentOf(rifleman));
				AIO_Maneuvers.SetROE(rifleman, mode);
			}
			return string.Format("%1 men %2", bodies.Count(), what);
		}
		return "Not built yet for selected men: " + typename.EnumToString(AIO_EOrder, order);
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
		members.RemoveItem(player);
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
			case AIO_EOrder.ADMIN_TELEPORT_MAP:
				return TeleportToMapCursor(player, point);
			case AIO_EOrder.ADMIN_TELEPORT_GROUP:
				return TeleportSquad(player, members, point);
			case AIO_EOrder.ADMIN_TELEPORT_VEHICLE:
				return TeleportWithVehicle(player, point);
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

	static bool TeleportVehicleEntity(IEntity vehicle, vector point)
	{
		if (!vehicle)
			return false;
		vector transform[4];
		vehicle.GetWorldTransform(transform);
		transform[3] = point;
		SCR_TerrainHelper.OrientToTerrain(transform);
		vector mins, maxs;
		vehicle.GetBounds(mins, maxs);
		transform[3][1] = transform[3][1] - mins[1] + 0.4;
		vector previous = vehicle.GetOrigin();
		SCR_EditableEntityComponent editableVehicle = SCR_EditableEntityComponent.Cast(vehicle.FindComponent(SCR_EditableEntityComponent));
		if (editableVehicle && !editableVehicle.SetTransform(transform))
			return false;
		AIO_Vehicles.SetPose(vehicle, transform);
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if (rpl)
			rpl.ForceNodeMovement(previous);
		return true;
	}

	static bool TeleportCharacterEntity(IEntity entity, vector point, vector facing = vector.Zero)
	{
		if (!entity)
			return false;
		vector transform[4];
		entity.GetWorldTransform(transform);
		transform[3] = point;
		if (facing != vector.Zero)
		{
			facing[1] = 0;
			facing.Normalize();
			transform[2] = facing;
			transform[0] = Vector(facing[2], 0, -facing[0]);
			transform[1] = Vector(0, 1, 0);
		}
		SCR_EditableCharacterComponent editable = SCR_EditableCharacterComponent.Cast(entity.FindComponent(SCR_EditableCharacterComponent));
		if (editable)
		{
			if (editable.SetTransform(transform))
				return true;
		}
		IEntity vehicle = AIO_Vehicles.VehicleOf(entity);
		if (vehicle)
		{
			CompartmentAccessComponent access = CompartmentAccessComponent.Cast(entity.FindComponent(CompartmentAccessComponent));
			if (access && access.GetOutVehicle_NoDoor(transform, false, true))
				return true;
		}
		BaseGameEntity body = BaseGameEntity.Cast(entity);
		if (body)
		{
			body.Teleport(transform);
			Physics physics = entity.GetPhysics();
			if (physics)
			{
				physics.SetVelocity(vector.Zero);
				physics.SetAngularVelocity(vector.Zero);
			}
			return true;
		}
		return false;
	}

	// Called only after AdminAllowed in Admin. Seat authority comes from the server.
	static string TeleportToMapCursor(IEntity player, vector point)
	{
		if (!player)
			return "No body to teleport";
		point[1] = GetGame().GetWorld().GetSurfaceY(point[0], point[2]);
		if (!SCR_Global.IsPositionWithinTerrainBounds(point))
			return "Can't teleport outside the map";
		IEntity vehicle = AIO_Vehicles.VehicleOf(player);
		BaseCompartmentSlot seat = AIO_Vehicles.SeatOf(player);
		if (vehicle && seat && seat.GetType() == ECompartmentType.PILOT)
		{
			if (!TeleportVehicleEntity(vehicle, point))
				return "Can't teleport this vehicle";
			return "Teleported you and your vehicle";
		}
		if (!TeleportCharacterEntity(player, point))
			return "Can't teleport this body";
		return "Teleported to your map cursor";
	}

	static string TeleportWithVehicle(IEntity player, vector point)
	{
		if (!player)
			return "No body to teleport";
		point[1] = GetGame().GetWorld().GetSurfaceY(point[0], point[2]);
		if (!SCR_Global.IsPositionWithinTerrainBounds(point))
			return "Can't teleport outside the map";
		IEntity vehicle = AIO_Vehicles.VehicleOf(player);
		if (!vehicle)
			return "You are not in a vehicle";
		if (!TeleportVehicleEntity(vehicle, point))
			return "Can't teleport this vehicle";
		array<BaseCompartmentSlot> seats = {};
		AIO_Vehicles.Seats(vehicle, seats);
		int occupants = 0;
		foreach (BaseCompartmentSlot slot : seats)
		{
			if (slot && slot.GetOccupant())
				occupants++;
		}
		if (occupants > 1)
			return string.Format("Teleported vehicle and %1 occupants", occupants);
		return "Teleported you and your vehicle";
	}

	static string TeleportSquad(IEntity player, array<IEntity> members, vector point)
	{
		if (!player)
			return "No body to teleport";
		point[1] = GetGame().GetWorld().GetSurfaceY(point[0], point[2]);
		if (!SCR_Global.IsPositionWithinTerrainBounds(point))
			return "Can't teleport outside the map";
		vector playerOrigin = player.GetOrigin();
		vector forward, right;
		AIO_Tasks.Facing(player, forward, right);

		IEntity playerVeh = AIO_Vehicles.VehicleOf(player);
		BaseCompartmentSlot playerSeat = AIO_Vehicles.SeatOf(player);
		vector pDest = point;
		bool driverMovedVeh = false;
		if (playerVeh && playerSeat && playerSeat.GetType() == ECompartmentType.PILOT)
		{
			if (!TeleportVehicleEntity(playerVeh, point))
				return "Can't teleport this vehicle";
			driverMovedVeh = true;
			pDest = playerVeh.GetOrigin();
		}
		else
		{
			if (!TeleportCharacterEntity(player, point, forward))
				return "Can't teleport your body";
			pDest = point;
		}

		array<vector> placedPositions = {};
		placedPositions.Insert(pDest);
		int movedSquad = 0;
		int fallbackSlot = 0;

		if (members)
		{
			foreach (IEntity soldier : members)
			{
				if (!soldier || soldier == player)
					continue;

				// If driver moved vehicle and soldier was already inside it, they traveled with the vehicle.
				if (driverMovedVeh && AIO_Vehicles.VehicleOf(soldier) == playerVeh)
				{
					movedSquad++;
					continue;
				}

				vector sOrigin = soldier.GetOrigin();
				vector diff = sOrigin - playerOrigin;
				float dist = vector.DistanceXZ(sOrigin, playerOrigin);

				vector dest = vector.Zero;
				bool hasCandidate = false;

				// Preserve relative position if reasonably close, on foot, and not colliding
				if (dist >= 2.0 && dist <= 35.0 && !AIO_Vehicles.VehicleOf(soldier))
				{
					float localForward = diff[0] * forward[0] + diff[2] * forward[2];
					float localRight = diff[0] * right[0] + diff[2] * right[2];
					vector candidate = pDest + forward * localForward + right * localRight;
					candidate[1] = GetGame().GetWorld().GetSurfaceY(candidate[0], candidate[2]);

					bool collides = false;
					foreach (vector placed : placedPositions)
					{
						if (vector.DistanceXZ(candidate, placed) < 1.8)
						{
							collides = true;
							break;
						}
					}
					if (!collides)
					{
						dest = candidate;
						hasCandidate = true;
					}
				}

				// Fallback to orderly formation slot behind player if too close, stray, in vehicle or colliding
				if (!hasCandidate)
				{
					while (fallbackSlot < 100)
					{
						int row = 1 + (fallbackSlot / 2);
						float side = 1.0;
						if (fallbackSlot % 2 == 1)
							side = -1.0;
						float lateral = 2.5;
						if (row % 2 == 0)
							lateral = 3.5;
						vector slot = pDest - forward * (row * 3.5) + right * (side * lateral);
						slot[1] = GetGame().GetWorld().GetSurfaceY(slot[0], slot[2]);
						fallbackSlot++;

						bool collides = false;
						foreach (vector placed : placedPositions)
						{
							if (vector.DistanceXZ(slot, placed) < 1.8)
							{
								collides = true;
								break;
							}
						}
						if (!collides)
						{
							dest = slot;
							break;
						}
					}
				}

				if (TeleportCharacterEntity(soldier, dest, forward))
				{
					placedPositions.Insert(dest);
					movedSquad++;
				}
			}
		}

		if (movedSquad > 0)
			return string.Format("Teleported you and %1 squad members", movedSquad);
		return "Teleported you";
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
