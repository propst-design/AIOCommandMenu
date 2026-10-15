// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). Licensed under the GNU GPL v3 or later (see LICENSE).

// Automated checks on a dedicated server (no player). Started by host options:
//   -aioTaskTest 1 : orders to single soldiers (move a man, hold, rejoin).
//   -aioMoveTest 1 : Move menu (Advance keeps station on the leader while he moves; Fall back).
//   -aioInfantryTest 1 : Take cover, stance, Heal up (nobody hurt).
//   -aioSupplyTest 1 : Rearm from a fallen teammate, Take weapon from an enemy body.
//   -aioAttackTest 1 : Flank right, per-man ROE / engage / fire on my lead, Bound forward.
//   -aioDefenseTest 1 : 360 defense, Garrison and Clear a building, Fortify.
//   -aioTowerTest 1 : the defense test on the airfield control tower.
//   -aioVehicleTest 1 : get in, switch seats, lights/engine, disembark, get out, eject, unflip.
//   -aioRecoveryTest 1 : GM-style rifle squad, unreachable interior posts, cancel, formation recovery.
//   -aioSupportTest 1 : watch direction, commanded medical treatment, routes, patrols and arrival reports.
//   -aioRadiusTest 1 : all-round defense radius selection, outward/crouched posts and actual movement.
//   -aioContactTest 1 : native firefight, retained orders, danger interruption and arrival-driven bounding.
//   -aioFormationTest 1 : rifle squad walks legs in Column, Staggered column, Line, Wedge; shapes measured on the move.
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
	protected static bool s_bTower;
	protected static IEntity s_Building;
	protected static vector s_vPoint;
	protected static SCR_AIGroup s_Enemy;
	protected static SCR_AIGroup s_Master; // playable group standing in for the player's group
	protected static ResourceName s_sEnemyWeapon;
	protected static vector s_vForward;
	protected static bool s_bVehicleTest;
	protected static bool s_bRecoveryTest;
	protected static bool s_bFormationTest;
	protected static bool s_bSupportTest;
	protected static bool s_bRadiusTest;
	protected static bool s_bContactTest;
	protected static ref AIO_Route s_Route;
	protected static ref AIO_Route s_Patrol;
	protected static int s_iLeg;
	protected static vector s_vLegFrom;
	static const ResourceName RIFLE_SQUAD = "{DDF3799FA1387848}Prefabs/Groups/BLUFOR/Group_US_RifleSquad.et";
	protected static IEntity s_Vehicle;
	protected static IEntity s_Gunner;
	static const ResourceName HUMVEE = "{3EA6F47D95867114}Prefabs/Vehicles/Wheeled/M998/M1025_armed_M2HB.et";
	static const ResourceName MORTAR = "{8094D99689ABE241}Prefabs/Weapons/Mortars/M252/Mortar_M252.et";
	static const ResourceName TRIPOD = "{73530808E4B455BA}Prefabs/Weapons/Tripods/Tripod_M3_M2HB.et";

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
		string towerValue;
		System.GetCLIParam("aioTowerTest", towerValue);
		s_bTower = towerValue == "1";
		if (s_bTower)
			s_bDefenseTest = true;
		string vehicleValue;
		System.GetCLIParam("aioVehicleTest", vehicleValue);
		s_bVehicleTest = vehicleValue == "1";
		string recoveryValue;
		System.GetCLIParam("aioRecoveryTest", recoveryValue);
		s_bRecoveryTest = recoveryValue == "1";
		string formationValue;
		System.GetCLIParam("aioFormationTest", formationValue);
		s_bFormationTest = formationValue == "1";
		string supportValue;
		System.GetCLIParam("aioSupportTest", supportValue);
		s_bSupportTest = supportValue == "1";
		string radiusValue;
		System.GetCLIParam("aioRadiusTest", radiusValue);
		s_bRadiusTest = radiusValue == "1";
		string contactValue;
		System.GetCLIParam("aioContactTest", contactValue);
		s_bContactTest = contactValue == "1";
		if (!s_bContactTest && !s_bRadiusTest && !s_bSupportTest && !s_bFormationTest && !s_bRecoveryTest && !s_bVehicleTest && !s_bMoveTest && !s_bInfantryTest && !s_bSupplyTest && !s_bAttackTest && !s_bDefenseTest && (!System.GetCLIParam("aioTaskTest", value) || value != "1"))
			return;
		if (!Replication.IsServer())
			return;
		Print("[AIO] TEST tasks: starting in 8 s");
		if (!s_bRecoveryTest)
			GetGame().GetCallqueue().CallLater(MakeMaster, 2000, false);
		GetGame().GetCallqueue().CallLater(Spawn, 8000, false);
	}

	// In real play the squad is the slave AI group of the player's playable group, and mods
	// such as ACA leave slave groups alone. Give the test squad the same setup, made a few
	// seconds early because CreateNewPlayableGroup finishes its own setup on a delayed call.
	protected static void MakeMaster()
	{
		SCR_GroupsManagerComponent manager = SCR_GroupsManagerComponent.GetInstance();
		SCR_FactionManager factions = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		if (!manager || !factions)
			return;
		s_Master = manager.CreateNewPlayableGroup(factions.GetFactionByKey("US"));
		if (!s_Master)
			return;
		s_Master.SetDeleteWhenEmpty(false);
		s_Master.SetCanDeleteIfNoPlayer(false);
	}

	protected static void LinkToMaster()
	{
		SCR_GroupsManagerComponent manager = SCR_GroupsManagerComponent.GetInstance();
		if (!manager || !s_Master || !s_Group)
			return;
		manager.RequestSetGroupSlave(SCR_EntityHelper.EntityToRplId(s_Master), SCR_EntityHelper.EntityToRplId(s_Group));
		Print(string.Format("[AIO] TEST tasks: squad linked as slave %1", s_Group.IsSlave()));
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
		if (s_bContactTest)
			s_vStart = AIO_ContactTest.Lane(s_vStart);
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
		ResourceName prefab = GROUP;
		if (s_bRecoveryTest || s_bFormationTest || s_bContactTest)
			prefab = RIFLE_SQUAD;
		s_Group = SCR_AIGroup.Cast(GetGame().SpawnEntityPrefab(Resource.Load(prefab), GetGame().GetWorld(), params));
		LinkToMaster();
		Print(string.Format("[AIO] TEST tasks: group spawned at %1", s_vStart));
		if (s_bContactTest)
			GetGame().GetCallqueue().CallLater(ContactStart, 12000, false);
		else if (s_bRadiusTest)
			GetGame().GetCallqueue().CallLater(RadiusStart, 12000, false);
		else if (s_bSupportTest)
			GetGame().GetCallqueue().CallLater(SupportStart, 12000, false);
		else if (s_bRecoveryTest)
			GetGame().GetCallqueue().CallLater(RecoveryStart, 12000, false);
		else if (s_bFormationTest)
			GetGame().GetCallqueue().CallLater(FormStart, 12000, false);
		else if (s_bVehicleTest)
			GetGame().GetCallqueue().CallLater(VehStart, 10000, false);
		else if (s_bAttackTest)
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

	protected static void ContactStart()
	{
		AIO_ContactTest.Start(s_Group);
	}

	protected static void RadiusStart()
	{
		array<IEntity> men = {};
		NonLeaders(men);
		if (men.Count() < 2 || !Leader())
		{
			Check("radius-setup", false, "need a leader and two soldiers");
			Finish();
			return;
		}
		AIO_Tasks.AgentOf(Leader()).DeactivateAI();
		array<int> orders = {AIO_EOrder.DEF_360, AIO_EOrder.DEF_360_5, AIO_EOrder.DEF_360_10, AIO_EOrder.DEF_360_20, AIO_EOrder.DEF_360_30, AIO_EOrder.DEF_360_50};
		array<float> expected = {5, 5, 10, 20, 30, 50}; // three non-leaders: original Auto is 5 m
		array<RplId> ids = {};
		foreach (IEntity man : men)
			ids.Insert(Replication.FindItemId(man));
		for (int i = 0; i < orders.Count(); i++)
		{
			string reply = AIO_Server.ExecuteMen(s_Group, orders[i], ids, vector.Zero, Leader(), RplId.Invalid(), 0);
			bool correct = reply.Contains("all-round");
			foreach (IEntity soldier : men)
			{
				AIO_HoldBehavior hold = AIO_Tasks.ActiveHold(AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(soldier)));
				if (!hold || Math.AbsFloat(vector.DistanceXZ(hold.m_vSpot, Leader().GetOrigin()) - expected[i]) > 0.1)
					correct = false;
				if (hold && (!hold.m_bCrouched || vector.DistanceXZ(hold.m_vLook, Leader().GetOrigin()) < expected[i]))
					correct = false;
			}
			Check(string.Format("radius-%1", i), correct, reply);
		}
		AIO_Server.ExecuteMen(s_Group, AIO_EOrder.DEF_360_20, ids, vector.Zero, Leader(), RplId.Invalid(), 0);
		GetGame().GetCallqueue().CallLater(RadiusArrived, 35000, false);
	}

	protected static void RadiusArrived()
	{
		int total;
		int crouched = CountStance(ECharacterStance.CROUCH, total);
		float worst = SlotError();
		Check("radius-walk-20", crouched == total && worst < 3, string.Format("%1 of %2 crouched, worst %3 m off ring slots", crouched, total, worst));
		array<IEntity> men = {};
		NonLeaders(men);
		AIO_HoldBehavior untouched = AIO_Tasks.ActiveHold(AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(men[1])));
		array<RplId> one = {Replication.FindItemId(men[0])};
		AIO_Server.ExecuteMen(s_Group, AIO_EOrder.DEF_360_10, one, vector.Zero, Leader(), RplId.Invalid(), 0);
		AIO_HoldBehavior selected = AIO_Tasks.ActiveHold(AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(men[0])));
		Check("radius-selected-only", selected && Math.AbsFloat(vector.DistanceXZ(selected.m_vSpot, Leader().GetOrigin()) - 10) < 0.1 && untouched == AIO_Tasks.ActiveHold(AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(men[1]))), "changing one man's radius leaves the others' orders intact");
		Finish();
	}

	protected static void SupportStart()
	{
		array<IEntity> men = {};
		NonLeaders(men);
		if (men.Count() < 3 || !Leader())
		{
			Check("support-setup", false, "need leader and three AI");
			Finish();
			return;
		}
		s_A = men[0];
		s_B = men[1];
		s_C = men[2];
		AIO_Tasks.AgentOf(Leader()).DeactivateAI();
		AIO_Tasks.AgentOf(s_B).DeactivateAI(); // only our assigned medic can treat the test wound
		AIO_Tasks.AgentOf(s_C).DeactivateAI();
		array<IEntity> selected = {s_A};
		vector look = s_A.GetOrigin() + Vector(60, 0, 0);
		AIO_Support.Order(s_Group, selected, Leader(), AIO_EOrder.WATCH_DIRECTION, look, null, 0);
		AIO_HoldBehavior hold = AIO_Tasks.ActiveHold(AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(s_A)));
		Check("watch-selected", hold && hold.m_vLook == look + Vector(0, 1.5, 0) && !AIO_Tasks.ActiveHold(AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(s_B))), "only selected man holds and watches");
		AIO_Support.Order(s_Group, selected, Leader(), AIO_EOrder.WATCH_AUTO, vector.Zero, null, 0);
		Check("watch-auto", hold.m_vLook == vector.Zero && AIO_Tasks.ActiveHold(AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(s_A))) == hold, "free look preserves hold");
		AIO_Tasks.Rejoin(s_A, Leader());
		string healthy = AIO_Support.Treat(s_Group, selected, Leader(), Leader(), 0);
		Check("treat-healthy", healthy == "That soldier needs no treatment", healthy);
		string invalid = AIO_Support.Treat(s_Group, selected, Leader(), s_Group, 0);
		Check("treat-invalid", invalid == "Look at a soldier in your squad, or use Treat me", invalid);
		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(SCR_DamageManagerComponent.GetDamageManager(Leader()));
		damage.AddParticularBleeding("RForearm");
		GetGame().GetCallqueue().CallLater(SupportTreat, 2000, false);
	}

	protected static void SupportTreat()
	{
		array<IEntity> selected = {s_A};
		Check("treat-wounded-setup", AIO_Support.Wounded(Leader()), "leader has a real bleeding wound");
		string reply = AIO_Support.Order(s_Group, selected, Leader(), AIO_EOrder.TREAT_ME, vector.Zero, null, 0);
		SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(s_A));
		Check("treat-assigned", utility.HasActionOfType(AIO_TreatBehavior), reply);
		Check("treat-duplicate", AIO_Support.Treat(s_Group, selected, Leader(), Leader(), 0) == "A soldier is already treating that casualty", "duplicate casualty is rejected");
		GetGame().GetCallqueue().CallLater(SupportTreated, 45000, false);
	}

	protected static void SupportTreated()
	{
		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(SCR_DamageManagerComponent.GetDamageManager(Leader()));
		Check("treat-real-bandage", !damage.IsBleeding(), string.Format("patient bleeding %1", damage.IsBleeding()));
		AIO_Tasks.AgentOf(s_B).ActivateAI();
		AIO_Tasks.AgentOf(s_C).ActivateAI();
		AIO_Tasks.Rejoin(s_A, Leader());
		array<ref AIActionBase> treatments = {};
		AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(s_A)).FindActionsOfType(AIO_TreatBehavior, treatments);
		bool active;
		foreach (AIActionBase action : treatments)
		{
			if (!action.GetRemoveAction() && action.GetActionState() != EAIActionState.FAILED && action.GetActionState() != EAIActionState.COMPLETED)
				active = true;
		}
		Check("treat-cancel", !active, "Follow removes commanded treatment");
		array<IEntity> men = {s_A, s_B};
		AIO_Maneuvers.SetTeam(s_A, 1);
		AIO_Maneuvers.SetTeam(s_B, 1);
		vector first = FindSpot(s_vStart, 20);
		vector second = FindSpot(first, 20);
		array<vector> points = {first, second};
		string reply = AIO_Routes.Start(s_Group, Leader(), men, points, false, 0, s_Route);
		Check("route-start", s_Route && s_Route.m_aMen.Count() == 2 && s_Route.m_sElement == "Red team (2 men)", reply);
		AIO_Tasks.AgentOf(s_B).DeactivateAI();
		GetGame().GetCallqueue().CallLater(SupportWaiting, 18000, false);
	}

	protected static void SupportWaiting()
	{
		Check("route-waits-for-element", s_Route && s_Route.m_iReached == 0, "stationary second man prevents premature arrival");
		AIO_Tasks.AgentOf(s_B).ActivateAI();
		GetGame().GetCallqueue().CallLater(SupportRouteDone, 60000, false);
	}

	protected static void SupportRouteDone()
	{
		Check("route-two-waypoints", s_Route && s_Route.m_bFinished && !s_Route.m_bBlocked && s_Route.m_iReached == 2, string.Format("reached %1, blocked %2", s_Route.m_iReached, s_Route.m_bBlocked));
		array<IEntity> men = {s_A, s_B};
		vector first = s_A.GetOrigin();
		vector second = first + Vector(20, 0, 0);
		second[1] = GetGame().GetWorld().GetSurfaceY(second[0], second[2]);
		array<vector> shortPatrol = {first, second};
		AIO_Routes.Start(s_Group, Leader(), men, shortPatrol, true, 0, s_Patrol);
		GetGame().GetCallqueue().CallLater(SupportPatrolDone, 45000, false);
	}

	protected static void SupportPatrolDone()
	{
		AIO_Route patrol = s_Patrol;
		Check("patrol-loops", patrol && !patrol.m_bFinished && patrol.m_iReached >= 3, string.Format("reached %1 legs", patrol.m_iReached));
		AIO_Tasks.Rejoin(s_A, Leader());
		AIO_Routes.Tick();
		Check("route-subset-cancel", patrol && patrol.m_aMen.Count() == 1 && patrol.m_aMen[0] == s_B, "Follow cancels only the reassigned man");
		AIO_Tasks.Rejoin(s_B, Leader());
		AIO_Routes.Tick();
		Check("route-cancel-all", patrol.m_bFinished && patrol.m_aMen.IsEmpty(), "no stale route remains");
		array<IEntity> one = {s_A};
		array<vector> impossible = {s_A.GetOrigin() + Vector(0, 25, 0)};
		AIO_Route stalled;
		AIO_Routes.Start(s_Group, Leader(), one, impossible, false, 0, stalled);
		AIO_Tasks.AgentOf(s_A).DeactivateAI();
		AIO_Routes.Tick(); // establish initial distance
		stalled.m_fProgressAt -= 61000;
		AIO_Routes.Tick();
		Check("route-blocked", stalled.m_bBlocked && stalled.m_iReached == 0, "unreachable height never counts as arrival; timeout stops route");
		AIO_Tasks.AgentOf(s_A).ActivateAI();
		AIO_Route invalid;
		array<vector> points = {s_vStart};
		array<IEntity> outsider = {Leader()};
		AIO_Routes.Start(s_Group, Leader(), outsider, points, false, 0, invalid);
		Check("route-reject-leader", !invalid, "player leader cannot be ordered by route");
		AIO_Routes.Start(s_Group, Leader(), one, points, true, 0, invalid);
		Check("patrol-reject-single-point", !invalid, "patrol needs two points");
		AIO_Tasks.HoldHere(s_A);
		AIO_Route arrival = AIO_Routes.TrackMove(s_Group, Leader(), one, s_A.GetOrigin(), 0, false);
		AIO_Routes.Tick();
		Check("single-move-arrival", arrival.m_bFinished && arrival.m_iReached == 1, "ordinary selected Move here arrival is reported once");
		AIO_Route nativeArrival = AIO_Routes.TrackMove(s_Group, Leader(), one, s_vStart, 0, true);
		AIO_Tasks.Rejoin(s_A, Leader());
		AIO_Routes.Tick();
		Check("native-move-cancel", nativeArrival.m_bFinished && nativeArrival.m_iReached == 0, "Follow invalidates native arrival observer");
		vector farPoint = s_A.GetOrigin() + Vector(1000, 0, 0);
		farPoint[1] = GetGame().GetWorld().GetSurfaceY(farPoint[0], farPoint[2]);
		array<vector> longLeg = {farPoint};
		AIO_Route longRoute;
		AIO_Routes.Start(s_Group, Leader(), one, longLeg, false, 0, longRoute);
		longRoute.m_fStarted -= 360000;
		AIO_Routes.Tick();
		Check("long-map-leg", !longRoute.m_bFinished && !longRoute.m_bBlocked, "six minutes with progress does not time out a kilometre-long leg");
		AIO_Tasks.Rejoin(s_A, Leader());
		AIO_Routes.Tick();
		Finish();
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
		array<RplId> ids = {Replication.FindItemId(s_A), Replication.FindItemId(s_B)};
		array<IEntity> resolved = {};
		AIO_Tasks.Resolve(s_Group, ids, resolved);
		Check("roster-id-resolve", resolved.Count() == 2 && resolved.Contains(s_A) && resolved.Contains(s_B) && !resolved.Contains(leader), "selected roster ids match exactly the requested men");
		string reply = AIO_Server.ExecuteMen(s_Group, AIO_EOrder.MOVE_HERE, ids, s_vTarget, leader, RplId.Invalid(), 0);
		Check("order", reply == "2 men moving", string.Format("%1; destination %2 m away", reply, vector.Distance(s_vStart, s_vTarget)));
		GetGame().GetCallqueue().CallLater(Arrived, 35000, false);
	}

	// ---------------------------------------------------------------- garrison recovery test

	protected static void RecoveryStart()
	{
		array<IEntity> men = {};
		NonLeaders(men);
		IEntity leader = Leader();
		Check("possessed-squad-setup", leader && men.Count() >= 8 && AIO_Server.IsPossessed(s_Group), string.Format("%1 men, no playable master/slave", men.Count()));
		if (!leader || men.IsEmpty())
		{
			Finish();
			return;
		}
		AIO_Tasks.AgentOf(leader).DeactivateAI(); // stand-in for a GM-controlled leader
		foreach (IEntity man : men)
		{
			// No navmesh/standing position 20 m above the man: genuinely impossible to reach.
			AIO_Maneuvers.HoldFacing(man, man.GetOrigin() + Vector(0, 20, 0), leader.GetOrigin(), false);
			AIO_Tasks.GuardInterior(man, AIO_Tasks.ActiveHold(AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(man))));
		}
		GetGame().GetCallqueue().CallLater(RecoveryTimedOut, 60000, false);
	}

	protected static void RecoveryTimedOut()
	{
		array<IEntity> men = {};
		NonLeaders(men);
		int stuck;
		foreach (IEntity man : men)
		{
			SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(man));
			if (AIO_Tasks.ActiveHold(utility) || AIO_Tasks.ActiveMove(utility))
				stuck++;
		}
		Check("unreachable-post-release", stuck == 0, string.Format("%1 men still have interior move/hold tasks after 60 s", stuck));
		s_Building = FindBuilding(s_vStart);
		if (!s_Building)
		{
			Check("approach-cancel", false, "no building found for pending approach");
			Finish();
			return;
		}
		AIO_Maneuvers.Approach(men, Leader(), AIO_EOrder.DEF_GARRISON, s_Building);
		SCR_AIGroupUtilityComponent groupUtility = SCR_AIGroupUtilityComponent.Cast(s_Group.FindComponent(SCR_AIGroupUtilityComponent));
		SCR_AIMoveActivity oldActivity = new SCR_AIMoveActivity(groupUtility, null, Leader().GetOrigin(), null, useVehicles: false);
		foreach (IEntity man : men)
		{
			AIO_Maneuvers.SetEngage(man, 0); // clear/assault's old formation override must not survive Follow
			SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(man));
			array<ref AIActionBase> formation = {};
			utility.FindActionsOfType(SCR_AIMoveInFormationBehavior, formation);
			foreach (AIActionBase action : formation)
				action.SetRelatedGroupActivity(oldActivity);
		}
		Print("[AIO] TEST recovery: " + AIO_Server.ExecuteBodies(s_Group, AIO_EOrder.FOLLOW, men, vector.Zero, Leader(), RplId.Invalid(), 0));
		SCR_AIMessage_Cancel cancel = new SCR_AIMessage_Cancel();
		cancel.m_RelatedGroupActivity = oldActivity;
		SCR_AIGoalReaction_Cancel reaction = new SCR_AIGoalReaction_Cancel();
		foreach (IEntity man : men)
			reaction.PerformReaction(AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(man)), cancel);
		GetGame().GetCallqueue().CallLater(RecoveryFollowing, 3000, false);
	}

	protected static void RecoveryFollowing()
	{
		array<IEntity> men = {};
		NonLeaders(men);
		int clean;
		foreach (IEntity man : men)
		{
			SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(man));
			if (!AIO_Tasks.ActiveHold(utility) && !AIO_Tasks.ActiveMove(utility) && utility.AIO_GetKeepFormation() == -1 && AIO_Tasks.HasFormation(utility))
				clean++;
		}
		Check("follow-cleanup", clean == men.Count() && AIO_Maneuvers.s_aApproaches.IsEmpty(), string.Format("%1 of %2 in native formation after late waypoint cancel, %3 pending approaches", clean, men.Count(), AIO_Maneuvers.s_aApproaches.Count()));
		IEntity leader = Leader();
		vector transform[4];
		leader.GetWorldTransform(transform);
		transform[3] = FindSpot(leader.GetOrigin(), 40);
		BaseGameEntity.Cast(leader).Teleport(transform);
		GetGame().GetCallqueue().CallLater(RecoveryFormed, 35000, false);
	}

	protected static void RecoveryFormed()
	{
		array<IEntity> men = {};
		NonLeaders(men);
		float farthest;
		float spread;
		int clean;
		foreach (IEntity man : men)
		{
			farthest = Math.Max(farthest, vector.DistanceXZ(man.GetOrigin(), Leader().GetOrigin()));
			foreach (IEntity other : men)
				spread = Math.Max(spread, vector.DistanceXZ(man.GetOrigin(), other.GetOrigin()));
			SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(man));
			if (!AIO_Tasks.ActiveHold(utility) && !AIO_Tasks.ActiveMove(utility) && AIO_Tasks.HasFormation(utility))
				clean++;
			Print(string.Format("[AIO] TEST recovery member %1 at %2, action %3", AIO_Server.NameOf(man), man.GetOrigin(), utility.GetExecutedAction()));
		}
		Check("follow-moving-leader", farthest < 30 && clean == men.Count(), string.Format("farthest %1 m, %2 of %3 without AIO movement overrides", farthest, clean, men.Count()));
		Check("formation-spread", spread > 8, string.Format("native formation span %1 m", spread));
		Finish();
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
		if (s_bTower)
		{
			if (name.Contains("ControlTower") && !name.Contains("furniture"))
				s_aBuildings.Insert(entity);
			return true;
		}
		if (!name.Contains("Structures/Houses") && !name.Contains("Structures/Buildings"))
			return true;
		if (AIO_Maneuvers.BuildingOf(entity) == entity)
			s_aBuildings.Insert(entity);
		return true;
	}

	protected static IEntity FindBuilding(vector near)
	{
		s_aBuildings = {};
		float radius = 700;
		if (s_bTower)
			radius = 8000;
		GetGame().GetWorld().QueryEntitiesBySphere(near, radius, CollectBuilding, null, EQueryEntitiesFlags.ALL);
		if (s_bTower)
		{
			foreach (IEntity tower : s_aBuildings)
			{
				IEntity chosen = AIO_Maneuvers.BuildingOf(tower);
				string parentName = "none";
				if (tower.GetParent() && tower.GetParent().GetPrefabData())
					parentName = tower.GetParent().GetPrefabData().GetPrefabName();
				Print(string.Format("[AIO] TEST tower candidate %1 parent %2 BuildingOf same %3", tower.GetPrefabData().GetPrefabName(), parentName, chosen == tower));
			}
			if (s_aBuildings.IsEmpty())
				return null;
			return AIO_Maneuvers.BuildingOf(s_aBuildings[0]);
		}
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
		Print(string.Format("[AIO] TEST defense: %1 interior points (%2)", inside.Count(), AIO_Maneuvers.InteriorReport(s_Building)));
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
		int clearWait = 140000;
		if (s_bTower)
			clearWait = 180000;
		GetGame().GetCallqueue().CallLater(Cleared, clearWait, false);
	}

	protected static void Cleared()
	{
		array<IEntity> men = {};
		NonLeaders(men);
		int done;
		int visited;
		int skipped;
		foreach (IEntity man : men)
		{
			AIO_HoldBehavior hold = AIO_Tasks.ActiveHold(AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(man)));
			if (hold && hold.m_bRouteComplete && hold.m_iVisitedRooms > 0 && (!hold.m_aLegs || hold.m_aLegs.IsEmpty()))
				done++;
			if (hold)
			{
				visited += hold.m_iVisitedRooms;
				skipped += hold.m_iSkippedRooms;
			}
		}
		int inside = MenInside(men);
		Check("clear", done == men.Count() && inside == men.Count(), string.Format("%1 of %2 finished reachable room routes, %3 inside; %4 positions reached, %5 inaccessible positions skipped", done, men.Count(), inside, visited, skipped));
		vector forward, right;
		AIO_Tasks.Facing(Leader(), forward, right);
		s_vPoint = AIO_Maneuvers.Ground(Leader().GetOrigin() + Vector(0, 0, -25));
		foreach (IEntity man : men)
			AIO_Tasks.Release(man);
		string reply = AIO_Maneuvers.Defense(men, Leader(), AIO_EOrder.DEF_FORTIFY, s_vPoint, null);
		Check("fortify-order", reply.Contains("fortifying"), reply);
		int fortifyWait = 35000;
		if (s_bTower)
			fortifyWait = 55000; // down the tower stairs first
		GetGame().GetCallqueue().CallLater(Fortified, fortifyWait, false);
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
		if (!s_bTower)
		{
			Finish();
			return;
		}
		// Walk-up fallback: the men go to the building first, then garrison it.
		string reply = AIO_Maneuvers.Approach(men, Leader(), AIO_EOrder.DEF_GARRISON, s_Building);
		Check("approach-order", reply.Contains("moving up"), reply);
		GetGame().GetCallqueue().CallLater(Approached, 60000, false);
	}

	protected static void Approached()
	{
		array<IEntity> men = {};
		NonLeaders(men);
		int inside = MenInside(men);
		int holding;
		foreach (IEntity man : men)
		{
			if (AIO_Tasks.ActiveHold(AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(man))))
				holding++;
			AIO_Tasks.Release(man);
		}
		Check("approach", inside == men.Count() && holding == men.Count() && AIO_Maneuvers.s_aApproaches.IsEmpty(), string.Format("%1 of %2 inside, %3 holding, %4 approaches left", inside, men.Count(), holding, AIO_Maneuvers.s_aApproaches.Count()));
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

	// ---------------------------------------------------------------- vehicle test

	protected static void Squad(notnull array<IEntity> members)
	{
		AIO_Server.Members(s_Group, members);
	}

	protected static int Aboard(out int cargo)
	{
		array<IEntity> members = {};
		Squad(members);
		int n;
		cargo = 0;
		foreach (IEntity member : members)
		{
			if (AIO_Vehicles.VehicleOf(member) != s_Vehicle)
				continue;
			n++;
			BaseCompartmentSlot seat = AIO_Vehicles.SeatOf(member);
			if (seat && seat.GetType() == ECompartmentType.CARGO)
				cargo++;
		}
		return n;
	}

	protected static IEntity InSeat(int type)
	{
		array<BaseCompartmentSlot> seats = {};
		AIO_Vehicles.Seats(s_Vehicle, seats);
		foreach (BaseCompartmentSlot seat : seats)
		{
			if (seat.GetType() == type && seat.GetOccupant())
				return seat.GetOccupant();
		}
		return null;
	}

	protected static void VehStart()
	{
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = FindSpot(s_vStart, 15) + Vector(0, 0.5, 0);
		s_Vehicle = GetGame().SpawnEntityPrefab(Resource.Load(HUMVEE), GetGame().GetWorld(), params);
		Check("vehicle-spawn", s_Vehicle != null, string.Format("M1025 at %1", params.Transform[3]));
		if (!s_Vehicle)
		{
			Finish();
			return;
		}
		GetGame().GetCallqueue().CallLater(VehBoard, 4000, false);
	}

	protected static void VehBoard()
	{
		VehTarget(MORTAR, "mortar");
		VehTarget(TRIPOD, "tripod");
		array<IEntity> members = {};
		Squad(members);
		string reply = AIO_Vehicles.GetIn(members, s_Vehicle, -1);
		Check("board-order", reply.Contains("getting in"), reply);
		GetGame().GetCallqueue().CallLater(VehBoarded, 30000, false);
	}

	protected static void VehTarget(ResourceName prefab, string name)
	{
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		vector spot = s_Vehicle.GetOrigin() + Vector(6, 0, 0);
		spot[1] = GetGame().GetWorld().GetSurfaceY(spot[0], spot[2]);
		params.Transform[3] = spot;
		IEntity weapon = GetGame().SpawnEntityPrefab(Resource.Load(prefab), GetGame().GetWorld(), params);
		Check(name + "-look-target", weapon && AIO_Vehicles.TargetVehicle(weapon, null, spot) == weapon, "looked emplacement wins over nearby HMMWV");
		Check(name + "-near-target", weapon && AIO_Vehicles.TargetVehicle(null, null, spot) == weapon, "static emplacement included in nearest search");
		if (weapon)
			SCR_EntityHelper.DeleteEntityAndChildren(weapon);
	}

	protected static void VehBoarded()
	{
		array<IEntity> members = {};
		Squad(members);
		int cargo;
		int aboard = Aboard(cargo);
		s_Gunner = InSeat(ECompartmentType.TURRET);
		IEntity driver = InSeat(ECompartmentType.PILOT);
		Check("board", aboard == members.Count() && driver != null && s_Gunner != null, string.Format("%1 of %2 aboard, driver %3, gunner %4, %5 passengers", aboard, members.Count(), driver != null, s_Gunner != null, cargo));
		if (driver)
		{
			vector start = s_Vehicle.GetOrigin() + Vector(0, 1, 0);
			vector end = start + Vector(0, -5, 60);
			vector point;
			IEntity hit;
			bool traced = AIO_Menu.TracePoint(driver, start, end, point, hit);
			Check("mounted-look-point", traced && hit != driver && AIO_Vehicles.VehicleRoot(hit) != s_Vehicle, string.Format("point %1, hit %2", point, hit));
		}
		if (!s_Gunner)
		{
			Finish();
			return;
		}
		// Use the selected-men / possessed-group route that failed in the live GM check.
		s_vTarget = FindSpot(s_Vehicle.GetOrigin(), 35);
		array<IEntity> one = {driver};
		Print("[AIO] TEST mounted move: " + AIO_Server.ExecuteBodies(s_Group, AIO_EOrder.MOVE_HERE, one, s_vTarget, null, RplId.Invalid(), 0));
		GetGame().GetCallqueue().CallLater(VehMoved, 35000, false);
	}

	protected static void VehMoved()
	{
		float distance = vector.DistanceXZ(s_Vehicle.GetOrigin(), s_vTarget);
		Check("mounted-move-here", distance < 10, string.Format("vehicle %1 m from target", distance));
		IEntity driver = InSeat(ECompartmentType.PILOT);
		if (driver)
			AIO_Tasks.Release(driver);
		array<IEntity> one = {s_Gunner};
		Print("[AIO] TEST vehicle: " + AIO_Vehicles.SwitchSeat(one, ECompartmentType.CARGO));
		GetGame().GetCallqueue().CallLater(VehSwitched, 12000, false);
	}

	protected static void VehSwitched()
	{
		BaseCompartmentSlot seat = AIO_Vehicles.SeatOf(s_Gunner);
		bool ok = seat != null && seat.GetType() == ECompartmentType.CARGO && AIO_Vehicles.VehicleOf(s_Gunner) == s_Vehicle && InSeat(ECompartmentType.TURRET) == null;
		string where = "nowhere";
		if (seat)
			where = AIO_Vehicles.SeatName(seat.GetType());
		Check("switch-to-passenger", ok, "gunner now " + where);
		array<IEntity> one = {s_Gunner};
		Print("[AIO] TEST vehicle: " + AIO_Vehicles.SwitchSeat(one, ECompartmentType.TURRET));
		GetGame().GetCallqueue().CallLater(VehSwitchedBack, 12000, false);
	}

	protected static void VehSwitchedBack()
	{
		BaseCompartmentSlot seat = AIO_Vehicles.SeatOf(s_Gunner);
		string where = "nowhere";
		if (seat)
			where = AIO_Vehicles.SeatName(seat.GetType());
		Check("switch-to-gunner", seat != null && seat.GetType() == ECompartmentType.TURRET, "now " + where + ", squad bar: " + AIO_Tasks.TaskLabel(s_Gunner));
		array<IEntity> members = {};
		Squad(members);
		Print("[AIO] TEST vehicle: " + AIO_Vehicles.Controls(members, AIO_EOrder.VEH_LIGHTS_ON));
		Print("[AIO] TEST vehicle: " + AIO_Vehicles.Controls(members, AIO_EOrder.VEH_ENGINE_ON));
		GetGame().GetCallqueue().CallLater(VehControls, 6000, false);
	}

	protected static void VehControls()
	{
		BaseLightManagerComponent lights = BaseLightManagerComponent.Cast(s_Vehicle.FindComponent(BaseLightManagerComponent));
		BaseVehicleControllerComponent controller = BaseVehicleControllerComponent.Cast(s_Vehicle.FindComponent(BaseVehicleControllerComponent));
		bool lit = lights != null && lights.GetLightsState(ELightType.Head);
		bool running = controller != null && controller.IsEngineOn();
		Check("lights-engine", lit && running, string.Format("lights %1, engine %2", lit, running));
		array<IEntity> members = {};
		Squad(members);
		Print("[AIO] TEST vehicle: " + AIO_Vehicles.Controls(members, AIO_EOrder.VEH_ENGINE_OFF));
		string reply = AIO_Vehicles.GetOut(members, s_Group, AIO_EOrder.VEH_DISEMBARK);
		Print("[AIO] TEST vehicle: " + reply);
		GetGame().GetCallqueue().CallLater(VehDisembarked, 15000, false);
	}

	protected static void VehDisembarked()
	{
		BaseVehicleControllerComponent controller = BaseVehicleControllerComponent.Cast(s_Vehicle.FindComponent(BaseVehicleControllerComponent));
		Check("engine-off", controller != null && !controller.IsEngineOn(), "");
		int cargo;
		int aboard = Aboard(cargo);
		bool crew = InSeat(ECompartmentType.PILOT) != null && InSeat(ECompartmentType.TURRET) != null;
		Check("disembark", cargo == 0 && aboard == 2 && crew, string.Format("%1 aboard, %2 passengers, crew stayed %3", aboard, cargo, crew));
		array<IEntity> members = {};
		Squad(members);
		Print("[AIO] TEST vehicle: " + AIO_Vehicles.GetOut(members, s_Group, AIO_EOrder.VEH_GET_OUT));
		GetGame().GetCallqueue().CallLater(VehOut, 20000, false);
	}

	protected static void VehOut()
	{
		int cargo;
		int aboard = Aboard(cargo);
		SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(s_Group.FindComponent(SCR_AIGroupUtilityComponent));
		bool used = utility != null && utility.IsUsableVehicle(s_Vehicle);
		Check("get-out", aboard == 0 && !used, string.Format("%1 still aboard, still the squad's vehicle %2", aboard, used));
		array<IEntity> members = {};
		Squad(members);
		array<IEntity> one = {members[members.Count() - 1]};
		s_A = one[0];
		Print("[AIO] TEST vehicle: " + AIO_Vehicles.GetIn(one, s_Vehicle, ECompartmentType.PILOT));
		GetGame().GetCallqueue().CallLater(VehEjectGo, 20000, false);
	}

	protected static void VehEjectGo()
	{
		BaseCompartmentSlot seat = AIO_Vehicles.SeatOf(s_A);
		Check("get-in-driver", seat != null && seat.GetType() == ECompartmentType.PILOT, "");
		array<IEntity> one = {s_A};
		Print("[AIO] TEST vehicle: " + AIO_Vehicles.GetOut(one, s_Group, AIO_EOrder.VEH_EJECT));
		GetGame().GetCallqueue().CallLater(VehEjected, 3000, false);
	}

	protected static int s_iFlips;

	protected static void VehEjected()
	{
		Check("eject", AIO_Vehicles.VehicleOf(s_A) == null, "out within 3 s");
		Flip();
	}

	protected static void FlipAngles()
	{
		Print(string.Format("[AIO] TEST vehicle: 1 s after the flip %1", s_Vehicle.GetYawPitchRoll()));
	}

	protected static void Flip()
	{
		s_iFlips++;
		vector angles = s_Vehicle.GetYawPitchRoll();
		vector mat[4];
		Math3D.AnglesToMatrix(Vector(angles[0], 0, 178), mat);
		mat[3] = s_Vehicle.GetOrigin() + Vector(0, 2.5, 0); // short drop: crash damage logs vanilla (E) noise
		AIO_Vehicles.SetPose(s_Vehicle, mat);
		Print(string.Format("[AIO] TEST vehicle: flipped to %1", s_Vehicle.GetYawPitchRoll()));
		GetGame().GetCallqueue().CallLater(FlipAngles, 1000, false);
		GetGame().GetCallqueue().CallLater(VehFlipGo, 5000, false);
	}

	protected static void VehFlipGo()
	{
		if (!AIO_Vehicles.IsFlipped(s_Vehicle) && s_iFlips < 3)
		{
			Print(string.Format("[AIO] TEST vehicle: flip %1 did not hold (%2), again", s_iFlips, s_Vehicle.GetYawPitchRoll()));
			Flip();
			return;
		}
		Check("flipped", AIO_Vehicles.IsFlipped(s_Vehicle), string.Format("angles %1 after %2 tries", s_Vehicle.GetYawPitchRoll(), s_iFlips));
		array<IEntity> members = {};
		Squad(members);
		Print("[AIO] TEST vehicle: " + AIO_Vehicles.Unflip(members, s_Vehicle));
		GetGame().GetCallqueue().CallLater(VehUnflipped, 35000, false);
	}

	protected static void VehUnflipped()
	{
		Check("unflip", !AIO_Vehicles.IsFlipped(s_Vehicle), string.Format("angles %1", s_Vehicle.GetYawPitchRoll()));
		Finish();
	}

	// ---------------------------------------------------------------- formation test

	static const int FORM_LEGS = 4;
	static const float FORM_LEG_LENGTH = 180;

	protected static int LegOrder(int leg)
	{
		switch (leg)
		{
			case 0: return AIO_EOrder.FORM_COLUMN;
			case 1: return AIO_EOrder.FORM_STAGGERED_COLUMN;
			case 2: return AIO_EOrder.FORM_LINE;
		}
		// Wedge after Line: from a column, the wings need longer than a leg to run up beside a
		// sprinting AI leader (vanilla movement).
		return AIO_EOrder.FORM_WEDGE;
	}

	// Average distance of the non-leader men from the nearest slot of a formation laid out on the
	// leader, facing roughly the direction he walked (best fit within 20 degrees: the tail of the
	// squad may still be on the path's last bend).
	protected static float ShapeError(string name, vector forward, bool lateralOnly = false)
	{
		float best = 9999;
		for (int step = -4; step <= 4; step++)
		{
			float angle = step * 5 * Math.DEG2RAD;
			float c = Math.Cos(angle);
			float sn = Math.Sin(angle);
			vector turned = Vector(forward[0] * c - forward[2] * sn, 0, forward[0] * sn + forward[2] * c);
			best = Math.Min(best, ShapeErrorAt(name, turned, lateralOnly));
		}
		return best;
	}

	// lateralOnly: only the sideways placement counts (a 30 m line or wedge can't keep up with a
	// running leader, but each man keeps his own lane in it).
	protected static float ShapeErrorAt(string name, vector forward, bool lateralOnly = false)
	{
		AIFormationDefinition definition = GetGame().GetAIWorld().GetFormation(name);
		IEntity leader = Leader();
		if (!definition || !leader)
			return 9999;
		vector right = Vector(forward[2], 0, -forward[0]);
		array<IEntity> men = {};
		NonLeaders(men);
		float total;
		foreach (IEntity man : men)
		{
			vector rel = man.GetOrigin() - leader.GetOrigin();
			float x = vector.Dot(rel, right);
			float z = vector.Dot(rel, forward);
			float best = 9999;
			for (int i = 1; i < 11; i++)
			{
				vector slot = definition.GetOffsetPosition(i);
				float dz = slot[2] - z;
				if (lateralOnly)
					dz = 0;
				best = Math.Min(best, Math.Sqrt((slot[0] - x) * (slot[0] - x) + dz * dz));
			}
			total += best;
		}
		if (men.IsEmpty())
			return 9999;
		return total / men.Count();
	}

	protected static void FormStart()
	{
		array<IEntity> men = {};
		NonLeaders(men);
		Check("formation-setup", Leader() && men.Count() >= 6 && s_Group.IsSlave(), string.Format("%1 men besides the leader, slave %2", men.Count(), s_Group.IsSlave()));
		string missing;
		for (int order = AIO_EOrder.FORM_COLUMN; order <= AIO_EOrder.FORM_LINE; order++)
		{
			if (!AIO_Formations.Exists(AIO_Formations.FormationOf(order)))
				missing += " " + AIO_Formations.NameOf(AIO_Formations.FormationOf(order));
		}
		Check("formation-definitions", missing.IsEmpty(), "missing:" + missing);
		if (!Leader() || men.IsEmpty())
		{
			Finish();
			return;
		}
		vector right;
		AIO_Tasks.Facing(Leader(), s_vForward, right);
		s_iLeg = 0;
		FormLeg();
	}

	protected static void FormLeg()
	{
		int order = LegOrder(s_iLeg);
		string reply = AIO_Formations.Set(s_Group, order);
		string name = AIO_Formations.NameOf(AIO_Formations.FormationOf(order));
		Check("formation-order-" + name, reply.StartsWith("Formation: ") && AIO_Formations.Current(s_Group) == name, string.Format("%1; in use: %2", reply, AIO_Formations.Current(s_Group)));
		// Walk the next leg, turning right 90 degrees each time (a square, back near the start):
		// a full about-turn would make the squad wheel through the whole formation.
		IEntity leader = Leader();
		if (s_iLeg > 0)
			s_vForward = Vector(s_vForward[2], 0, -s_vForward[0]);
		vector target = FindSpot(leader.GetOrigin() + s_vForward * FORM_LEG_LENGTH, 0);
		if (target == vector.Zero)
			target = leader.GetOrigin() + s_vForward * FORM_LEG_LENGTH;
		AIO_Server.ClearWaypoints(s_Group);
		AIO_Server.AddWaypoint(s_Group, AIO_Server.WP_MOVE, target);
		Print(string.Format("[AIO] TEST formation leg %1: leader at %2, waypoint %3 (%4 m), %5 waypoints", s_iLeg, leader.GetOrigin(), target, vector.Distance(leader.GetOrigin(), target), FormWaypoints()));
		s_iFormSample = 0;
		s_fFormBest = 9999;
		s_sFormBest = "no sample while the leader was walking";
		s_bFormBestOk = false;
		s_vLegFrom = leader.GetOrigin();
		GetGame().GetCallqueue().CallLater(FormSample, 14000, false);
	}

	// The shape is measured on the move: every 4 s from 18 s into the leg, framed on the leader's
	// last 4 s of walking; the best sample counts (corners and obstacles break any formation).
	protected static int s_iFormSample;
	protected static float s_fFormBest;
	protected static string s_sFormBest;
	protected static bool s_bFormBestOk;

	protected static int FormWaypoints()
	{
		array<AIWaypoint> waypoints = {};
		return s_Group.GetWaypoints(waypoints);
	}

	protected static void FormSample()
	{
		IEntity leader = Leader();
		vector walked = leader.GetOrigin() - s_vLegFrom;
		walked[1] = 0;
		s_vLegFrom = leader.GetOrigin();
		s_iFormSample++;
		if (s_iFormSample == 1)
			Print(string.Format("[AIO] TEST formation leg %1: leader walked %2 m in the first 14 s, %3 waypoints", s_iLeg, walked.Length(), FormWaypoints()));
		if (s_iFormSample > 1 && walked.Length() > 2)
		{
			int order = LegOrder(s_iLeg);
			string name = AIO_Formations.NameOf(AIO_Formations.FormationOf(order));
			string other = "Wedge";
			if (name == "Wedge")
				other = "Column";
			vector forward = walked.Normalized();
			// Line and Wedge: sideways placement, against Column (a column has everyone in the middle).
			// Their wings trail far behind a sprinting AI leader after a turn (vanilla movement).
			bool lateral = name == "Line" || name == "Wedge";
			if (lateral)
				other = "Column";
			float error = ShapeError(name, forward, lateral);
			float otherError = ShapeError(other, forward, lateral);
			if (error < s_fFormBest)
			{
				s_fFormBest = error;
				s_bFormBestOk = error < 5 && error < otherError * 0.5; // sprinting stretches columns a little
				s_sFormBest = string.Format("sample %1 (leader %2 m in 4 s): men avg %3 m from %4 slots (%5 m from %6 slots)", s_iFormSample, walked.Length(), error, name, otherError, other);
				if (lateral)
					s_sFormBest += " [sideways only]";
				vector right = Vector(forward[2], 0, -forward[0]);
				array<IEntity> men = {};
				NonLeaders(men);
				foreach (IEntity man : men)
				{
					vector rel = man.GetOrigin() - leader.GetOrigin();
					Print(string.Format("[AIO] TEST formation %1 sample %2 man %3 right %4 ahead %5", name, s_iFormSample, AIO_Server.NameOf(man), vector.Dot(rel, right), vector.Dot(rel, forward)));
				}
			}
		}
		if (s_iFormSample < 16)
		{
			GetGame().GetCallqueue().CallLater(FormSample, 4000, false);
			return;
		}
		FormLegDone();
	}

	protected static void FormLegDone()
	{
		int order = LegOrder(s_iLeg);
		string name = AIO_Formations.NameOf(AIO_Formations.FormationOf(order));
		Check("formation-shape-" + name, AIO_Formations.Current(s_Group) == name && s_bFormBestOk, s_sFormBest + "; in use: " + AIO_Formations.Current(s_Group));
		s_iLeg++;
		if (s_iLeg < FORM_LEGS)
		{
			// Let him arrive first so the next leg starts from a standstill.
			GetGame().GetCallqueue().CallLater(FormLeg, 12000, false);
			return;
		}
		Finish();
	}
}
