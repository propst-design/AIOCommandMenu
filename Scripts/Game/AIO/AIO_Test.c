// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). Licensed under the GNU GPL v3 or later (see LICENSE).

// Automated checks on a dedicated server (no player). Started by host options:
//   -aioTaskTest 1 : orders to single soldiers (move a man, hold, rejoin).
//   -aioMoveTest 1 : Move menu (Advance keeps station on the leader while he moves; Fall back).
//   -aioInfantryTest 1 : Take cover, stance, Heal up (nobody hurt).
//   -aioSupplyTest 1 : Rearm from a fallen teammate, Take weapon from an enemy body.
//   -aioAttackTest 1 : Flank right, per-man ROE / engage / fire on my lead, Bound forward.
//   -aioDefenseTest 1 : 360 defense, Garrison and Clear a building, Fortify.
// Prints "[AIO] TEST <name> PASS|FAIL ..." lines and "[AIO] TEST COMPLETE".
class AIO_Test
{
	static const ResourceName GROUP = "{84E5BBAB25EA23E5}Prefabs/Groups/BLUFOR/Group_US_FireTeam.et";
	static const ResourceName ENEMY = "{657590C1EC9E27D3}Prefabs/Groups/OPFOR/Group_USSR_LightFireTeam.et";

	protected static SCR_AIGroup s_Group;
	protected static vector s_vStart;
	protected static vector s_vTarget;
	protected static IEntity s_A;
	protected static IEntity s_B;
	protected static IEntity s_C;
	protected static int s_iPass;
	protected static int s_iFail;
	protected static bool s_bMoveTest;
	protected static bool s_bInfantryTest;
	protected static bool s_bSupplyTest;
	protected static bool s_bAttackTest;
	protected static bool s_bDefenseTest;
	protected static IEntity s_Building;
	protected static vector s_vPoint;
	protected static SCR_AIGroup s_Enemy;
	protected static ResourceName s_sEnemyWeapon;
	protected static vector s_vForward;

	static void MaybeStart()
	{
		string value;
		string moveValue;
		System.GetCLIParam("aioMoveTest", moveValue);
		s_bMoveTest = moveValue == "1";
		string infantryValue;
		System.GetCLIParam("aioInfantryTest", infantryValue);
		s_bInfantryTest = infantryValue == "1";
		string supplyValue;
		System.GetCLIParam("aioSupplyTest", supplyValue);
		s_bSupplyTest = supplyValue == "1";
		string attackValue;
		System.GetCLIParam("aioAttackTest", attackValue);
		s_bAttackTest = attackValue == "1";
		string defenseValue;
		System.GetCLIParam("aioDefenseTest", defenseValue);
		s_bDefenseTest = defenseValue == "1";
		if (!s_bMoveTest && !s_bInfantryTest && !s_bSupplyTest && !s_bAttackTest && !s_bDefenseTest && (!System.GetCLIParam("aioTaskTest", value) || value != "1"))
			return;
		if (!Replication.IsServer())
			return;
		Print("[AIO] TEST tasks: starting in 8 s");
		GetGame().GetCallqueue().CallLater(Spawn, 8000, false);
	}

	protected static void Check(string name, bool ok, string detail)
	{
		if (ok)
			s_iPass++;
		else
			s_iFail++;
		string verdict = "FAIL";
		if (ok)
			verdict = "PASS";
		Print(string.Format("[AIO] TEST %1 %2 %3", name, verdict, detail));
	}

	protected static void Finish()
	{
		Print(string.Format("[AIO] TEST RESULT %1 passed, %2 failed", s_iPass, s_iFail));
		Print("[AIO] TEST COMPLETE");
	}

	// A dry, fairly flat spot near the middle of the map.
	protected static vector FindSpot(vector near, float minDistance)
	{
		BaseWorld world = GetGame().GetWorld();
		float sea = world.GetOceanBaseHeight();
		for (int ring = 0; ring < 40; ring++)
		{
			for (int k = 0; k < 8; k++)
			{
				float angle = k * 0.785;
				float radius = minDistance + ring * 25;
				float x = near[0] + Math.Cos(angle) * radius;
				float z = near[2] + Math.Sin(angle) * radius;
				float y = world.GetSurfaceY(x, z);
				if (y < sea + 2)
					continue;
				float slope = Math.AbsFloat(world.GetSurfaceY(x + 3, z) - y) + Math.AbsFloat(world.GetSurfaceY(x, z + 3) - y);
				if (slope > 1.2)
					continue;
				return Vector(x, y, z);
			}
		}
		return vector.Zero;
	}

	protected static void Spawn()
	{
		vector mins, maxs;
		GetGame().GetWorldEntity().GetTerrain(0, 0).GetTerrainBoundBox(mins, maxs);
		vector middle = (mins + maxs) * 0.5;
		s_vStart = FindSpot(middle, 0);
		if (s_bDefenseTest)
		{
			s_Building = FindBuilding(middle);
			if (!s_Building)
			{
				Check("setup", false, "no building found near the map centre");
				Finish();
				return;
			}
			vector bmins, bmaxs;
			s_Building.GetWorldBounds(bmins, bmaxs);
			s_vStart = FindSpot((bmins + bmaxs) * 0.5, 18);
			Print(string.Format("[AIO] TEST defense: building %1 at %2", s_Building.GetPrefabData().GetPrefabName(), s_Building.GetOrigin()));
		}
		if (s_vStart == vector.Zero)
		{
			Check("setup", false, "no dry spot found");
			Finish();
			return;
		}
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = s_vStart;
		s_Group = SCR_AIGroup.Cast(GetGame().SpawnEntityPrefab(Resource.Load(GROUP), GetGame().GetWorld(), params));
		Print(string.Format("[AIO] TEST tasks: group spawned at %1", s_vStart));
		if (s_bAttackTest)
			GetGame().GetCallqueue().CallLater(Flank, 12000, false);
		else if (s_bDefenseTest)
			GetGame().GetCallqueue().CallLater(AllRound, 12000, false);
		else if (s_bSupplyTest)
		{
			// Enemy fire team 20 m away, killed as soon as it is there: bodies to loot.
			EntitySpawnParams enemyParams = new EntitySpawnParams();
			enemyParams.TransformMode = ETransformMode.WORLD;
			enemyParams.Transform[3] = FindSpot(s_vStart, 20);
			s_Enemy = SCR_AIGroup.Cast(GetGame().SpawnEntityPrefab(Resource.Load(ENEMY), GetGame().GetWorld(), enemyParams));
			GetGame().GetCallqueue().CallLater(KillEnemies, 6000, false);
			GetGame().GetCallqueue().CallLater(Supply, 14000, false);
		}
		else if (s_bInfantryTest)
			GetGame().GetCallqueue().CallLater(Cover, 12000, false);
		else if (s_bMoveTest)
			GetGame().GetCallqueue().CallLater(Advance, 12000, false);
		else
			GetGame().GetCallqueue().CallLater(Order, 12000, false);
	}

	protected static void Order()
	{
		array<IEntity> members = {};
		AIO_Server.Members(s_Group, members);
		IEntity leader = null;
		if (s_Group && s_Group.GetLeaderAgent())
			leader = s_Group.GetLeaderAgent().GetControlledEntity();
		array<IEntity> others = {};
		foreach (IEntity member : members)
		{
			if (member != leader)
				others.Insert(member);
		}
		if (others.Count() < 3)
		{
			Check("setup", false, string.Format("only %1 men besides the leader", others.Count()));
			Finish();
			return;
		}
		s_A = others[0];
		s_B = others[1];
		s_C = others[2];
		s_vTarget = FindSpot(s_vStart, 35);
		bool a = AIO_Tasks.MoveTo(s_A, s_vTarget);
		bool b = AIO_Tasks.MoveTo(s_B, AIO_Tasks.SpreadSpot(s_vTarget, 1));
		Check("order", a && b, string.Format("two men sent %1 m away", vector.Distance(s_vStart, s_vTarget)));
		GetGame().GetCallqueue().CallLater(Arrived, 35000, false);
	}

	// ---------------------------------------------------------------- Move menu test

	// Average distance of the non-leader men from where Advance should put them (leader + 50 m ahead).
	protected static float AdvanceError(out float nearest)
	{
		IEntity leader = Leader();
		array<IEntity> members = {};
		AIO_Server.Members(s_Group, members);
		members.RemoveItem(leader);
		vector center = leader.GetOrigin() + s_vForward * AIO_Server.SHIFT_DISTANCE;
		float total = 0;
		nearest = 9999;
		foreach (IEntity member : members)
		{
			total += vector.DistanceXZ(member.GetOrigin(), center); // the spot ahead may be up or down a slope
			float fromLeader = vector.Distance(member.GetOrigin(), leader.GetOrigin());
			if (fromLeader < nearest)
				nearest = fromLeader;
		}
		if (members.IsEmpty())
			return 9999;
		return total / members.Count();
	}

	// Largest distance of any man from his own hold spot (what the watchdog keeps him at).
	protected static float SlotError()
	{
		array<IEntity> members = {};
		AIO_Server.Members(s_Group, members);
		float worst = 0;
		array<ref AIActionBase> holds = {};
		foreach (IEntity member : members)
		{
			SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(member));
			if (!utility)
				continue;
			AIO_HoldBehavior hold = AIO_Tasks.ActiveHold(utility);
			if (!hold)
				continue;
			float d = vector.Distance(member.GetOrigin(), hold.Spot());
			if (d > worst)
				worst = d;
		}
		return worst;
	}

	protected static void Advance()
	{
		IEntity leader = Leader();
		if (!leader)
		{
			Check("setup", false, "no leader");
			Finish();
			return;
		}
		vector right;
		AIO_Tasks.Facing(leader, s_vForward, right);
		string reply = AIO_Server.MoveMenu(s_Group, leader, AIO_EOrder.ADVANCE, null);
		Check("advance-order", reply.Contains("advancing"), reply);
		GetGame().GetCallqueue().CallLater(Advanced, 40000, false);
	}

	protected static void Advanced()
	{
		float nearest;
		float error = AdvanceError(nearest);
		Check("advance", error < 12 && nearest > 35, string.Format("men average %1 m from the spot 50 m ahead; nearest %2 m from the leader; worst man %3 m off his own slot", error, nearest, SlotError()));
		// Now the leader walks 40 m to his right: the advanced men should keep station on him.
		IEntity leader = Leader();
		vector right = Vector(s_vForward[2], 0, -s_vForward[0]);
		vector target = leader.GetOrigin() + right * 40;
		target[1] = GetGame().GetWorld().GetSurfaceY(target[0], target[2]);
		AIO_Server.AddWaypoint(s_Group, AIO_Server.WP_MOVE, target);
		GetGame().GetCallqueue().CallLater(Followed, 45000, false);
	}

	protected static void Followed()
	{
		float nearest;
		float error = AdvanceError(nearest);
		Check("station-keeping", error < 14, string.Format("after the leader moved: men average %1 m from the spot 50 m ahead of him; worst man %2 m off his own slot", error, SlotError()));
		// Fall back for everyone: a fixed spot 40 m behind the leader.
		IEntity leader = Leader();
		string reply = AIO_Server.MoveMenu(s_Group, leader, AIO_EOrder.FALL_BACK, null);
		vector forward, right;
		AIO_Tasks.Facing(leader, forward, right);
		s_vTarget = leader.GetOrigin() + forward * -AIO_Server.FALL_BACK_DISTANCE;
		Check("fallback-order", reply.Contains("falling back"), reply);
		GetGame().GetCallqueue().CallLater(FellBack, 40000, false);
	}

	protected static void FellBack()
	{
		IEntity leader = Leader();
		array<IEntity> members = {};
		AIO_Server.Members(s_Group, members);
		members.RemoveItem(leader);
		float total = 0;
		foreach (IEntity member : members)
		{
			total += vector.Distance(member.GetOrigin(), s_vTarget);
			AIO_Tasks.Release(member);
		}
		float error = 9999;
		if (!members.IsEmpty())
			error = total / members.Count();
		Check("fallback", error < 12, string.Format("men average %1 m from the fall-back spot", error));
		Finish();
	}

	// ---------------------------------------------------------------- infantry test

	protected static int CountStance(int stance, out int total)
	{
		array<IEntity> members = {};
		AIO_Server.Members(s_Group, members);
		members.RemoveItem(Leader());
		total = members.Count();
		int n;
		foreach (IEntity member : members)
		{
			ChimeraCharacter character = ChimeraCharacter.Cast(member);
			if (character && character.GetCharacterController() && character.GetCharacterController().GetStance() == stance)
				n++;
			else if (character && character.GetCharacterController())
			{
				CharacterControllerComponent controller = character.GetCharacterController();
				AIAgent agent = AIO_Tasks.AgentOf(member);
				SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(agent);
				string action = "none";
				if (utility && utility.GetCurrentAction())
					action = utility.GetCurrentAction().ClassName();
				Print(string.Format("[AIO] TEST detail: %1 stance %2 (wanted %3), speed %4, action %5", AIO_Server.NameOf(member), controller.GetStance(), stance, controller.GetVelocity().Length(), action));
			}
		}
		return n;
	}

	protected static void Cover()
	{
		string reply = AIO_Server.Infantry(s_Group, Leader(), AIO_EOrder.TAKE_COVER, null);
		Check("cover-order", reply.Contains("taking cover") || reply.Contains("crouch in place"), reply);
		GetGame().GetCallqueue().CallLater(Covered, 25000, false);
	}

	protected static void Covered()
	{
		int total;
		int crouched = CountStance(ECharacterStance.CROUCH, total);
		Check("cover", crouched == total && total > 0, string.Format("%1 of %2 men crouched; worst man %3 m off his cover spot", crouched, total, SlotError()));
		// Cover spots hug walls, where nobody fits lying down: test prone back in the open formation.
		array<IEntity> members = {};
		AIO_Server.Members(s_Group, members);
		foreach (IEntity member : members)
			AIO_Tasks.Release(member);
		GetGame().GetCallqueue().CallLater(ProneOrder, 12000, false);
	}

	protected static void ProneOrder()
	{
		string reply = AIO_Server.Infantry(s_Group, Leader(), AIO_EOrder.STANCE_PRONE, null);
		Check("prone-order", reply.Contains("prone"), reply);
		GetGame().GetCallqueue().CallLater(Proned, 10000, false);
	}

	protected static void Proned()
	{
		int total;
		int prone = CountStance(ECharacterStance.PRONE, total);
		Check("prone", prone == total && total > 0, string.Format("%1 of %2 men prone", prone, total));
		string reply = AIO_Server.Infantry(s_Group, Leader(), AIO_EOrder.HEAL_UP, null);
		Check("heal-unhurt", reply.Contains("Nobody is hurt"), reply);
		// Release: back to formation; Auto stance.
		array<IEntity> members = {};
		AIO_Server.Members(s_Group, members);
		foreach (IEntity member : members)
			AIO_Tasks.Release(member);
		reply = AIO_Server.Infantry(s_Group, Leader(), AIO_EOrder.STANCE_AUTO, null);
		Check("auto-order", reply.Contains("own stance"), reply);
		GetGame().GetCallqueue().CallLater(Autoed, 12000, false);
	}

	protected static void Autoed()
	{
		int total;
		int prone = CountStance(ECharacterStance.PRONE, total);
		Check("auto", prone < total, string.Format("%1 of %2 men still prone after Auto (calm: they should get up)", prone, total));
		Finish();
	}

	// ---------------------------------------------------------------- attack test

	protected static void NonLeaders(notnull array<IEntity> men)
	{
		AIO_Server.Members(s_Group, men);
		men.RemoveItem(Leader());
	}

	protected static void Flank()
	{
		array<IEntity> men = {};
		NonLeaders(men);
		vector center = AIO_Maneuvers.Centroid(men);
		s_vPoint = AIO_Maneuvers.Ground(center + Vector(80, 0, 0));
		string reply = AIO_Maneuvers.Attack(men, Leader(), AIO_EOrder.ATK_FLANK_RIGHT, s_vPoint, 0);
		Check("flank-order", reply.Contains("flanking right"), reply);
		// where they should end: beside the target on the right (dir +X, right = -Z)
		float distance = vector.DistanceXZ(center, s_vPoint);
		vector dir = AIO_Maneuvers.Flat(s_vPoint - center);
		vector side = Vector(dir[2], 0, -dir[0]);
		s_vTarget = s_vPoint + side * Math.Max(30, distance * 0.35);
		GetGame().GetCallqueue().CallLater(Flanked, 80000, false);
	}

	protected static void Flanked()
	{
		array<IEntity> men = {};
		NonLeaders(men);
		vector center = AIO_Maneuvers.Centroid(men);
		float off = vector.DistanceXZ(center, s_vTarget);
		vector dir = AIO_Maneuvers.Flat(s_vPoint - s_vStart);
		vector side = Vector(dir[2], 0, -dir[0]);
		float rightOf = vector.Dot(center - s_vPoint, side);
		Check("flank", off < 15 && rightOf > 15, string.Format("men %1 m from the flank position, %2 m right of the target", off, rightOf));

		// Fire control on one man.
		IEntity a = men[0];
		SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(a));
		AIO_Maneuvers.SetROE(a, EAIGroupCombatMode.HOLD_FIRE);
		bool held = utility.m_CombatComponent.GetCombatMode() == EAIGroupCombatMode.HOLD_FIRE;
		bool groupFree = s_Group.GetGroupUtilityComponent().GetCombatModeActual() != EAIGroupCombatMode.HOLD_FIRE;
		AIO_Maneuvers.SetROE(a, -1);
		bool back = utility.m_CombatComponent.GetCombatMode() == s_Group.GetGroupUtilityComponent().GetCombatModeActual();
		Check("roe-per-man", held && groupFree && back, string.Format("held %1, group still free %2, back to group %3", held, groupFree, back));
		AIO_Maneuvers.SetEngage(a, 1);
		bool keep = utility.ShouldKeepFormation();
		AIO_Maneuvers.SetEngage(a, 0);
		bool free = !utility.ShouldKeepFormation();
		AIO_Maneuvers.SetEngage(a, -1);
		Check("engage-disengage", keep && free, string.Format("disengage keeps formation %1, engage frees %2", keep, free));
		AIO_Maneuvers.FireOnMyLead(a, Leader());
		bool waiting = AIO_Maneuvers.IsWaitingForLead(AIO_Tasks.AgentOf(a)) && utility.m_CombatComponent.GetCombatMode() == EAIGroupCombatMode.HOLD_FIRE;
		AIO_Maneuvers.Fired(Leader());
		bool opened = !AIO_Maneuvers.IsWaitingForLead(AIO_Tasks.AgentOf(a)) && utility.m_CombatComponent.GetCombatMode() == EAIGroupCombatMode.FIRE_AT_WILL;
		AIO_Maneuvers.SetROE(a, -1);
		Check("fire-on-my-lead", waiting && opened, string.Format("waiting %1, opened fire %2", waiting, opened));

		// Bound forward 70 m further on.
		s_vPoint = AIO_Maneuvers.Ground(center + Vector(70, 0, 0));
		string reply = AIO_Maneuvers.Attack(men, Leader(), AIO_EOrder.BOUND, s_vPoint, 0);
		Check("bound-order", reply.Contains("bounding"), reply);
		GetGame().GetCallqueue().CallLater(Bounded, 110000, false);
	}

	protected static void Bounded()
	{
		array<IEntity> men = {};
		NonLeaders(men);
		vector center = AIO_Maneuvers.Centroid(men);
		float left = vector.DistanceXZ(center, s_vPoint);
		Check("bound", left < 15, string.Format("men %1 m from the bound target", left));
		foreach (IEntity man : men)
			AIO_Tasks.Release(man);
		Finish();
	}

	// ---------------------------------------------------------------- defense test

	protected static ref array<IEntity> s_aBuildings;
	protected static bool CollectBuilding(IEntity entity)
	{
		if (!entity.GetPrefabData())
			return true;
		string name = entity.GetPrefabData().GetPrefabName();
		if (!name.Contains("Structures/Houses") && !name.Contains("Structures/Buildings"))
			return true;
		if (AIO_Maneuvers.BuildingOf(entity) == entity)
			s_aBuildings.Insert(entity);
		return true;
	}

	protected static IEntity FindBuilding(vector near)
	{
		s_aBuildings = {};
		GetGame().GetWorld().QueryEntitiesBySphere(near, 700, CollectBuilding, null, EQueryEntitiesFlags.ALL);
		IEntity best = null;
		float bestDistance = float.MAX;
		foreach (IEntity building : s_aBuildings)
		{
			vector mins, maxs;
			building.GetWorldBounds(mins, maxs);
			vector size = maxs - mins;
			if (size[0] < 7 || size[2] < 7 || size[0] > 25 || size[2] > 25)
				continue; // a house, not a shed or a factory
			float distance = vector.DistanceXZ(building.GetOrigin(), near);
			if (distance < bestDistance)
			{
				best = building;
				bestDistance = distance;
			}
		}
		Print(string.Format("[AIO] TEST defense: %1 buildings looked at", s_aBuildings.Count()));
		return best;
	}

	protected static void AllRound()
	{
		array<IEntity> men = {};
		NonLeaders(men);
		string reply = AIO_Maneuvers.Defense(men, Leader(), AIO_EOrder.DEF_360, vector.Zero, null);
		Check("360-order", reply.Contains("all-round"), reply);
		GetGame().GetCallqueue().CallLater(AllRounded, 25000, false);
	}

	protected static void AllRounded()
	{
		array<IEntity> men = {};
		NonLeaders(men);
		int total;
		int crouched = CountStance(ECharacterStance.CROUCH, total);
		float worst = SlotError();
		Check("360", crouched == total && worst < 3, string.Format("%1 of %2 crouched, worst %3 m off his spot", crouched, total, worst));
		array<vector> inside = {};
		AIO_Maneuvers.InteriorPoints(s_Building, men[0], inside);
		Print(string.Format("[AIO] TEST defense: %1 interior points", inside.Count()));
		string reply = AIO_Maneuvers.Defense(men, Leader(), AIO_EOrder.DEF_GARRISON, vector.Zero, s_Building);
		Check("garrison-order", reply.Contains("garrisoning"), reply);
		GetGame().GetCallqueue().CallLater(Garrisoned, 45000, false);
	}

	protected static int MenInside(array<IEntity> men)
	{
		vector mins, maxs;
		s_Building.GetWorldBounds(mins, maxs);
		int n;
		foreach (IEntity man : men)
		{
			vector p = man.GetOrigin();
			if (p[0] > mins[0] - 0.5 && p[0] < maxs[0] + 0.5 && p[2] > mins[2] - 0.5 && p[2] < maxs[2] + 0.5)
				n++;
		}
		return n;
	}

	protected static void Garrisoned()
	{
		array<IEntity> men = {};
		NonLeaders(men);
		int inside = MenInside(men);
		Check("garrison", inside == men.Count(), string.Format("%1 of %2 men inside the building", inside, men.Count()));
		foreach (IEntity man : men)
			AIO_Tasks.Release(man);
		string reply = AIO_Maneuvers.Defense(men, Leader(), AIO_EOrder.DEF_CLEAR, vector.Zero, s_Building);
		Check("clear-order", reply.Contains("clearing"), reply);
		GetGame().GetCallqueue().CallLater(Cleared, 60000, false);
	}

	protected static void Cleared()
	{
		array<IEntity> men = {};
		NonLeaders(men);
		int done;
		foreach (IEntity man : men)
		{
			AIO_HoldBehavior hold = AIO_Tasks.ActiveHold(AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(man)));
			if (hold && (!hold.m_aLegs || hold.m_aLegs.IsEmpty()))
				done++;
		}
		int inside = MenInside(men);
		Check("clear", done == men.Count() && inside == men.Count(), string.Format("%1 of %2 men walked every room, %3 inside", done, men.Count(), inside));
		vector forward, right;
		AIO_Tasks.Facing(Leader(), forward, right);
		s_vPoint = AIO_Maneuvers.Ground(Leader().GetOrigin() + Vector(0, 0, -25));
		foreach (IEntity man : men)
			AIO_Tasks.Release(man);
		string reply = AIO_Maneuvers.Defense(men, Leader(), AIO_EOrder.DEF_FORTIFY, s_vPoint, null);
		Check("fortify-order", reply.Contains("fortifying"), reply);
		GetGame().GetCallqueue().CallLater(Fortified, 35000, false);
	}

	protected static void Fortified()
	{
		array<IEntity> men = {};
		NonLeaders(men);
		int total;
		int crouched = CountStance(ECharacterStance.CROUCH, total);
		float farthest = 0;
		foreach (IEntity man : men)
		{
			float d = vector.DistanceXZ(man.GetOrigin(), s_vPoint);
			if (d > farthest)
				farthest = d;
			AIO_Tasks.Release(man);
		}
		Check("fortify", crouched == total && farthest < 22, string.Format("%1 of %2 crouched, farthest %3 m from the point", crouched, total, farthest));
		Finish();
	}

	// ---------------------------------------------------------------- supply test

	protected static void KillEnemies()
	{
		array<IEntity> enemies = {};
		AIO_Server.Members(s_Enemy, enemies);
		foreach (IEntity enemy : enemies)
		{
			SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(enemy);
			if (damage)
				damage.Kill(Instigator.CreateInstigator(enemy));
		}
		Print(string.Format("[AIO] TEST supply: %1 enemies killed for loot", enemies.Count()));
	}

	// Magazines for the current muzzle of a man's first rifle-type weapon.
	protected static int Mags(IEntity body)
	{
		return AIO_Supplies.MagazineCount(body);
	}

	protected static void Supply()
	{
		IEntity leader = Leader();
		array<IEntity> members = {};
		AIO_Server.Members(s_Group, members);
		members.RemoveItem(leader);
		if (members.Count() < 3)
		{
			Check("setup", false, "not enough men");
			Finish();
			return;
		}
		s_A = members[0];
		s_B = members[1];
		s_C = members[2];
		// C falls: his body is the friendly source. A loses all his spare magazines.
		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(s_C);
		damage.Kill(Instigator.CreateInstigator(s_C));
		SCR_InventoryStorageManagerComponent inventory = AIO_Supplies.InventoryOf(s_A);
		array<IEntity> items = {};
		inventory.GetItems(items);
		int deleted;
		foreach (IEntity item : items)
		{
			if (item.FindComponent(MagazineComponent) && inventory.TryDeleteItem(item))
				deleted++;
		}
		GetGame().GetCallqueue().CallLater(SupplyOrders, 3000, false);
		Print(string.Format("[AIO] TEST supply: A dropped %1 magazines", deleted));
	}

	protected static void SupplyDebug()
	{
		InventoryStorageManagerComponent storage = InventoryStorageManagerComponent.Cast(s_C.FindComponent(InventoryStorageManagerComponent));
		array<IEntity> items = {};
		if (storage)
			storage.GetItems(items);
		string list = "";
		foreach (IEntity item : items)
		{
			string name = "?";
			if (item.GetPrefabData())
				name = item.GetPrefabData().GetPrefabName();
			list += string.Format(" [%1 kind %2 loose %3]", FilePath.StripPath(name), AIO_Supplies.Kind(item), AIO_Supplies.IsLooseItem(item));
		}
		Print(string.Format("[AIO] TEST detail: C dead %1, storage %2, %3 items:%4", AIO_Supplies.IsDeadCharacter(s_C), storage, items.Count(), list));
		array<ref AIO_Need> needs = {};
		AIO_Supplies.Needs(s_A, needs);
		foreach (AIO_Need need : needs)
			Print(string.Format("[AIO] TEST detail: A needs kind %1 well %2 x%3 prefab %4", need.m_iKind, need.m_tWell, need.m_iMissing, need.m_sPrefab));
		Print(string.Format("[AIO] TEST detail: usefulness of C for A = %1", AIO_Supplies.Usefulness(s_C, s_A, needs)));
		s_aDead = {};
		s_aLoose = {};
		GetGame().GetWorld().QueryEntitiesBySphere(s_vStart, 60, CollectDead, null, EQueryEntitiesFlags.ALL);
		foreach (IEntity loose : s_aLoose)
		{
			string looseName = "?";
			if (loose.GetPrefabData())
				looseName = loose.GetPrefabData().GetPrefabName();
			Print(string.Format("[AIO] TEST detail: loose %1 at %2 m", FilePath.StripPath(looseName), vector.Distance(loose.GetOrigin(), s_vStart)));
		}
	}

	protected static ref array<IEntity> s_aLoose;

	protected static void SupplyOrders()
	{
		SupplyDebug();
		int before = Mags(s_A);
		array<IEntity> men = {s_A};
		string reply = AIO_Supplies.Rearm(men, null, 0);
		Check("rearm-order", reply.Contains("1 men going"), string.Format("%1 (A had %2 magazines)", reply, before));
		// B takes the best weapon off an enemy body.
		// Enemy bodies: dead characters near the enemy spawn.
		s_aDead = {};
		GetGame().GetWorld().QueryEntitiesBySphere(s_vStart, 60, CollectDead, null, EQueryEntitiesFlags.ALL);
		IEntity enemyBody = null;
		foreach (IEntity dead : s_aDead)
		{
			if (dead == s_C)
				continue;
			enemyBody = dead;
			break;
		}
		if (!enemyBody)
		{
			Check("take-order", false, "no enemy body found");
			GetGame().GetCallqueue().CallLater(Supplied, 30000, false);
			return;
		}
		IEntity weapon = AIO_Supplies.WeaponAt(enemyBody);
		if (weapon && weapon.GetPrefabData())
			s_sEnemyWeapon = weapon.GetPrefabData().GetPrefabName();
		array<IEntity> taker = {s_B};
		reply = AIO_Supplies.TakeWeapon(taker, enemyBody, enemyBody, 0);
		Check("take-order", reply.Contains("going to take"), string.Format("%1 (%2)", reply, s_sEnemyWeapon));
		GetGame().GetCallqueue().CallLater(Supplied, 35000, false);
	}

	protected static ref array<IEntity> s_aDead;
	protected static bool CollectDead(IEntity entity)
	{
		if (AIO_Supplies.IsDeadCharacter(entity))
			s_aDead.Insert(entity);
		if (s_aLoose && entity.FindComponent(BaseWeaponComponent) && AIO_Supplies.IsLooseItem(entity))
			s_aLoose.Insert(entity);
		return true;
	}

	protected static void Supplied()
	{
		int after = Mags(s_A);
		Check("rearm", after >= 2, string.Format("A now has %1 magazines", after));
		bool hasWeapon = false;
		BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(s_B.FindComponent(BaseWeaponManagerComponent));
		array<IEntity> list = {};
		if (weapons)
			weapons.GetWeaponsList(list);
		foreach (IEntity weaponEntity : list)
		{
			if (weaponEntity.GetPrefabData() && weaponEntity.GetPrefabData().GetPrefabName() == s_sEnemyWeapon)
				hasWeapon = true;
		}
		Check("take-weapon", hasWeapon, string.Format("B carries %1: %2", s_sEnemyWeapon, hasWeapon));
		array<ref AIO_Need> needs = {};
		AIO_Supplies.Needs(s_B, needs);
		int magNeeds;
		foreach (AIO_Need need : needs)
		{
			if (need.m_iKind == AIO_Supplies.KIND_MAGAZINE)
				magNeeds++;
		}
		Check("take-weapon-ammo", hasWeapon && Mags(s_B) > 0, string.Format("B has %1 magazines for his current weapon", Mags(s_B)));
		Finish();
	}

	protected static IEntity Leader()
	{
		if (s_Group && s_Group.GetLeaderAgent())
			return s_Group.GetLeaderAgent().GetControlledEntity();
		return null;
	}

	protected static void Arrived()
	{
		float da = vector.Distance(s_A.GetOrigin(), s_vTarget);
		float db = vector.Distance(s_B.GetOrigin(), AIO_Tasks.SpreadSpot(s_vTarget, 1));
		Check("move", da <= 5 && db <= 6, string.Format("A %1 m, B %2 m from their spots", da, db));
		IEntity leader = Leader();
		float dc = -1;
		if (leader)
			dc = vector.Distance(s_C.GetOrigin(), leader.GetOrigin());
		Check("others-stay", dc >= 0 && dc < 15, string.Format("C %1 m from his leader", dc));
		// A rejoins (Follow me for one man); B keeps holding.
		AIO_Tasks.Rejoin(s_A, Leader());
		GetGame().GetCallqueue().CallLater(Rejoined, 30000, false);
	}

	protected static void Rejoined()
	{
		IEntity leader = Leader();
		float da = -1;
		if (leader)
			da = vector.Distance(s_A.GetOrigin(), leader.GetOrigin());
		Check("rejoin", da >= 0 && da < 15, string.Format("A %1 m from his leader", da));
		float db = vector.Distance(s_B.GetOrigin(), AIO_Tasks.SpreadSpot(s_vTarget, 1));
		Check("hold", db <= 6, string.Format("B still %1 m from his spot", db));
		string labelB = AIO_Tasks.TaskLabel(s_B);
		string labelA = AIO_Tasks.TaskLabel(s_A);
		Check("squad-bar-labels", labelB == "Holding" && labelA == "", string.Format("B '%1', A '%2'", labelB, labelA));
		AIO_Tasks.Release(s_B);
		Finish();
	}
}
