// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). GPL v3 or later (see LICENSE).

class AIO_TestEmergency : SCR_AIBehaviorBase
{
	void AIO_TestEmergency(SCR_AIUtilityComponent utility, SCR_AIActivityBase groupActivity)
	{
		m_sBehaviorTree = "AI/BehaviorTrees/Chimera/Soldier/Wait.bt";
		SetPriority(PRIORITY_BEHAVIOR_MOVE_FROM_DANGER);
	}
	override int GetCause() { return SCR_EAIBehaviorCause.DANGER_HIGH; }
}

// Real native perception and gunfire first; then deterministic lifecycle probes.
// Teleports below are test fixtures only, never production movement or combat.
class AIO_ContactTest
{
	protected static SCR_AIGroup s_Group;
	protected static SCR_AIGroup s_Enemy;
	protected static SCR_AIGroup s_EnemyMaster;
	protected static IEntity s_Leader;
	protected static ref array<IEntity> s_aMen = {};
	protected static ref array<IEntity> s_aEnemy = {};
	protected static ref map<IEntity, int> s_mAmmo = new map<IEntity, int>();
	protected static ref map<IEntity, BaseWeaponComponent> s_mWeapons = new map<IEntity, BaseWeaponComponent>();
	protected static ref map<IEntity, BaseMagazineComponent> s_mMagazines = new map<IEntity, BaseMagazineComponent>();
	protected static ref map<IEntity, vector> s_mPosts = new map<IEntity, vector>();
	protected static vector s_vOrigin;
	protected static vector s_vLane;
	protected static int s_iPass, s_iFail, s_iShots, s_iEnemyShots, s_iContacts;
	protected static ref AIO_Bound s_Bound;
	protected static ref AIO_TestEmergency s_Emergency;
	protected static ref AIO_HoldBehavior s_Previous;
	protected static ref AIO_HoldBehavior s_AidPost;
	protected static IEntity s_AidPatient;
	protected static bool s_bAidRan;
	protected static int s_iAidResult;
	protected static int s_iSimSteps;

	static vector Lane(vector near)
	{
		BaseWorld world = GetGame().GetWorld();
		for (int ring = 0; ring < 24; ring++)
		{
			for (int direction = 0; direction < 8; direction++)
			{
				float angle = direction * Math.PI2 / 8;
				vector center = near + Vector(Math.Cos(angle) * ring * 50, 0, Math.Sin(angle) * ring * 50);
				float low = float.MAX, high = -float.MAX;
				bool clear = true;
				for (int side = -1; side <= 1; side++)
				{
					for (int step = 0; step <= 9; step++)
					{
						vector point = center + Vector(step * 10, 0, side * 15);
						float y = world.GetSurfaceY(point[0], point[2]);
						low = Math.Min(low, y); high = Math.Max(high, y);
						if (y < world.GetOceanBaseHeight() + 1 || high - low > 2.5) clear = false;
					}
					TraceParam trace = new TraceParam();
					trace.Start = AIO_Maneuvers.Ground(center + Vector(0, 0, side * 15)) + Vector(0, 1.2, 0);
					trace.End = AIO_Maneuvers.Ground(center + Vector(90, 0, side * 15)) + Vector(0, 1.2, 0);
					trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
					if (world.TraceMove(trace, null) < 0.99) clear = false;
				}
				if (clear) { s_vLane = AIO_Maneuvers.Ground(center); return s_vLane; }
			}
		}
		return vector.Zero;
	}

	static void Check(string name, bool ok, string detail)
	{
		string verdict = "FAIL";
		if (ok) { verdict = "PASS"; s_iPass++; }
		else s_iFail++;
		Print(string.Format("[AIO] TEST %1 %2 %3", name, verdict, detail));
	}

	static void Start(SCR_AIGroup group)
	{
		s_Group = group;
		TimeAndWeatherManagerEntity weather = ChimeraWorld.CastFrom(GetGame().GetWorld()).GetTimeAndWeatherManager();
		Print(string.Format("[AIO] TEST time %1", weather.GetTimeOfTheDay()));
		weather.SetHoursMinutesSeconds(12, 0, 0);
		group.PreventMaxLOD();
		group.SetPermanentLOD(0);
		s_Leader = group.GetLeaderAgent().GetControlledEntity();
		AIO_Server.Members(group, s_aMen);
		s_aMen.RemoveItem(s_Leader);
		if (s_aMen.Count() < 4) { Check("contact-setup", false, "need four subordinates"); Finish(); return; }
		s_vOrigin = s_vLane;
		ObserversSystem observers = ObserversSystem.Cast(GetGame().GetWorld().FindSystem(ObserversSystem));
		if (observers) observers.InsertObserverSP(22103, s_vOrigin[0], s_vOrigin[2], null);
		AIO_Tasks.AgentOf(s_Leader).DeactivateAI();
		for (int i = 0; i < s_aMen.Count(); i++)
		{
			IEntity man = s_aMen[i];
			AIO_Tasks.AgentOf(man).PreventMaxLOD();
			AIO_Tasks.AgentOf(man).SetPermanentLOD(0);
			man.SetYawPitchRoll(Vector(90, 0, 0));
			vector pose[4];
			man.GetWorldTransform(pose);
			pose[3] = AIO_Maneuvers.Ground(s_vOrigin + Vector(0, 0, (i - 3.5) * 3));
			BaseGameEntity.Cast(man).Teleport(pose);
			AIO_Tasks.MoveTo(man, AIO_Maneuvers.Ground(s_vOrigin + Vector(0, 0, (i - 3.5) * 3)));
			s_mAmmo.Set(man, AIO_Maneuvers.PlayerAmmo(man));
		}
		SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(s_aMen[0]));
		AIO_MoveBehavior move = AIO_Tasks.ActiveMove(utility);
		Check("player-move-priority", move && move.EvaluatePriorityLevel() == SCR_AIActionBase.PRIORITY_LEVEL_PLAYER && move.Evaluate() < SCR_AIActionBase.PRIORITY_BEHAVIOR_MOVE_FROM_DANGER, "explicit movement outranks ordinary attacks, below native danger reactions");
		SCR_GroupsManagerComponent groups = SCR_GroupsManagerComponent.GetInstance();
		SCR_FactionManager factions = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		s_EnemyMaster = groups.CreateNewPlayableGroup(factions.GetFactionByKey("USSR"));
		if (s_EnemyMaster) { s_EnemyMaster.SetDeleteWhenEmpty(false); s_EnemyMaster.SetCanDeleteIfNoPlayer(false); }
		GetGame().GetCallqueue().CallLater(EnemySpawn, 5000, false);
	}

	protected static void EnemySpawn()
	{
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = AIO_Maneuvers.Ground(s_vOrigin + Vector(45, 0, 4));
		s_Enemy = SCR_AIGroup.Cast(GetGame().SpawnEntityPrefab(Resource.Load(AIO_Test.ENEMY), GetGame().GetWorld(), params));
		if (s_Enemy && s_EnemyMaster) SCR_GroupsManagerComponent.GetInstance().RequestSetGroupSlave(Replication.FindItemId(s_EnemyMaster), Replication.FindItemId(s_Enemy));
		GetGame().GetCallqueue().CallLater(OpenContact, 5000, false);
	}

	protected static void OpenContact()
	{
		AIO_Server.Members(s_Enemy, s_aEnemy);
		s_Enemy.PreventMaxLOD();
		s_Enemy.SetPermanentLOD(0);
		Faction ownFaction = FactionAffiliationComponent.Cast(s_aMen[0].FindComponent(FactionAffiliationComponent)).GetAffiliatedFaction();
		Faction enemyFaction = FactionAffiliationComponent.Cast(s_aEnemy[0].FindComponent(FactionAffiliationComponent)).GetAffiliatedFaction();
		Print(string.Format("[AIO] TEST factions %1/%2 hostile %3", ownFaction.GetFactionKey(), enemyFaction.GetFactionKey(), ownFaction.IsFactionEnemy(enemyFaction)));
		Check("enemy-setup", s_aEnemy.Count() > 0, string.Format("%1 live opponents", s_aEnemy.Count()));
		foreach (IEntity enemy : s_aEnemy)
		{
			AIO_Maneuvers.SetROE(enemy, EAIGroupCombatMode.FIRE_AT_WILL);
			AIO_Tasks.SetStance(AIO_Tasks.AgentOf(enemy), ECharacterStance.STAND);
			AIO_Tasks.AgentOf(enemy).PreventMaxLOD();
			AIO_Tasks.AgentOf(enemy).SetPermanentLOD(0);
			enemy.SetYawPitchRoll(Vector(-90, 0, 0));
			AIO_Tasks.AgentOf(enemy).ActivateAI();
			s_mAmmo.Set(enemy, AIO_Maneuvers.PlayerAmmo(enemy));
		}
		foreach (IEntity man : s_aMen)
		{
			AIO_Maneuvers.SetROE(man, EAIGroupCombatMode.FIRE_AT_WILL);
			AIO_Tasks.SetStance(AIO_Tasks.AgentOf(man), ECharacterStance.STAND);
			AIO_Tasks.HoldHere(man);
			AIO_Tasks.AgentOf(man).ActivateAI();
			AIO_Tasks.ActiveHold(AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(man))).m_vLook = s_vOrigin + Vector(80, 1.5, 0);
			s_mPosts.Set(man, man.GetOrigin());
		}
		GetGame().GetCallqueue().CallLater(Sample, 500, true);
		GetGame().GetCallqueue().CallLater(HoldContactResult, 18000, false);
	}

	protected static void Sample()
	{
		array<IEntity> all = {};
		all.Copy(s_aMen);
		all.InsertAll(s_aEnemy);
		foreach (IEntity body : all)
		{
			if (!AIO_Server.IsAlive(body)) continue;
			BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(body.FindComponent(BaseWeaponManagerComponent));
			BaseWeaponComponent weapon;
			if (weapons) weapon = weapons.GetCurrentWeapon();
			BaseMagazineComponent magazine;
			if (weapon) magazine = weapon.GetCurrentMagazine();
			int rounds = -1;
			if (magazine) rounds = magazine.GetAmmoCount();
			int before;
			BaseWeaponComponent oldWeapon;
			BaseMagazineComponent oldMagazine;
			if (s_mWeapons.Find(body, oldWeapon) && oldWeapon == weapon && s_mMagazines.Find(body, oldMagazine) && oldMagazine == magazine && s_mAmmo.Find(body, before) && rounds >= 0 && before > rounds)
			{
				if (s_aMen.Contains(body)) s_iShots += before - rounds;
				else s_iEnemyShots += before - rounds;
			}
			s_mAmmo.Set(body, rounds);
			s_mWeapons.Set(body, weapon);
			s_mMagazines.Set(body, magazine);
			SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(body));
			if (body == s_AidPatient && utility && SCR_AIHealBehavior.Cast(utility.GetCurrentBehavior())) s_bAidRan = true;
			if (s_aMen.Contains(body) && utility && utility.m_CombatComponent && utility.m_CombatComponent.GetCurrentTarget()) s_iContacts++;
		}
	}

	protected static void HoldContactResult()
	{
		SCR_AIUtilityComponent diagnostic = AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(s_aMen[0]));
		array<BaseTarget> detected = {};
		diagnostic.m_PerceptionComponent.GetTargetsList(detected, ETargetCategory.ENEMY);
		Print(string.Format("[AIO] TEST perception %1 behavior %2 combat mode %3 ammo %4", detected.Count(), diagnostic.GetCurrentBehavior(), diagnostic.m_CombatComponent.GetCombatMode(), AIO_Maneuvers.PlayerAmmo(s_aMen[0])));
		Print(string.Format("[AIO] TEST positions %1/%2 LOD %3", s_aMen[0].GetOrigin(), s_aEnemy[0].GetOrigin(), AIO_Tasks.AgentOf(s_aMen[0]).GetLOD()));
		Check("real-contact", s_iContacts > 0 && s_iEnemyShots > 0, string.Format("native target samples %1, enemy shots %2", s_iContacts, s_iEnemyShots));
		Check("hold-allows-firing", s_iShots > 0, string.Format("%1 shots by holders", s_iShots));
		float farthest;
		array<IEntity> live = {};
		foreach (IEntity body : s_aMen)
		{
			if (!AIO_Server.IsAlive(body)) continue;
			live.Insert(body);
			vector post;
			if (s_mPosts.Find(body, post)) farthest = Math.Max(farthest, vector.Distance(body.GetOrigin(), post));
		}
		Check("hold-post-contact", live.Count() > 0 && farthest < 12, string.Format("%1 survivors, farthest %2 m from ordered post", live.Count(), farthest));
		vector destination = AIO_Maneuvers.Ground(s_vOrigin + Vector(0, 0, 45));
		string reply = AIO_Bounding.Start(live, Vector(0, 0, 1), destination, false, 0, s_Leader, s_Bound);
		Check("bound-contact-start", s_Bound != null, reply);
		GetGame().GetCallqueue().CallLater(BoundContactResult, 18000, false);
	}

	protected static void BoundContactResult()
	{
		Check("bound-contact-honesty", s_Bound && (!s_Bound.m_bFinished || s_Bound.m_bFailed), "contact may pause or stop the maneuver, never fabricate arrival");
		if (s_Bound && !s_Bound.m_bFinished)
		{
			bool oneSide = true;
			foreach (AIO_BoundMember member : s_Bound.m_aMembers)
			{
				if (member.m_bMoving && member.m_bA != s_Bound.m_bAMoving) oneSide = false;
			}
			Check("bound-one-moving-element", oneSide, "covering element has no movement assignment");
		}
		else Check("bound-one-moving-element", false, "fixture lost an element before role sampling");
		if (s_Enemy) s_Enemy.DeactivateAI();
		if (s_EnemyMaster) s_EnemyMaster.DeactivateAI();
		foreach (IEntity enemy : s_aEnemy) if (enemy) SCR_EntityHelper.DeleteEntityAndChildren(enemy);
		GetGame().GetCallqueue().CallLater(Recovery, 5000, false);
	}

	protected static void Recovery()
	{
		// Fresh soldiers isolate lifecycle checks from live-fire deaths, bleeding and
		// retained suppression. The firefight's losses remain recorded above.
		SCR_AIGroup master = s_Group.GetMaster();
		s_Group.DeactivateAI();
		foreach (IEntity old : s_aMen)
		{
			if (!old) continue;
			AIO_Tasks.Release(old);
			SCR_EntityHelper.DeleteEntityAndChildren(old);
		}
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = s_vLane;
		s_Group = SCR_AIGroup.Cast(GetGame().SpawnEntityPrefab(Resource.Load(AIO_Test.RIFLE_SQUAD), GetGame().GetWorld(), params));
		if (master && s_Group) SCR_GroupsManagerComponent.GetInstance().RequestSetGroupSlave(Replication.FindItemId(master), Replication.FindItemId(s_Group));
		GetGame().GetCallqueue().CallLater(RecoveryReady, 10000, false);
	}

	protected static void RecoveryReady()
	{
		s_Group.PreventMaxLOD();
		s_Group.SetPermanentLOD(0);
		s_aMen.Clear();
		AIO_Server.Members(s_Group, s_aMen);
		s_Leader = s_Group.GetLeaderAgent().GetControlledEntity();
		AIO_Tasks.AgentOf(s_Leader).DeactivateAI();
		s_aMen.RemoveItem(s_Leader);
		// End the live-fire phase and prove a temporary high-priority interruption
		// leaves the exact destination/hold object available to resume afterwards.
		array<IEntity> live = {};
		foreach (IEntity body : s_aMen)
		{
			if (AIO_Bounding.Conscious(body)) live.Insert(body);
		}
		if (live.Count() < 4) { Check("recovery-fixture", false, "too few conscious survivors"); Finish(); return; }
		s_aMen.Copy(live);
		for (int i = 0; i < s_aMen.Count(); i++)
		{
			AIO_Tasks.AgentOf(s_aMen[i]).PreventMaxLOD();
			AIO_Tasks.AgentOf(s_aMen[i]).SetPermanentLOD(0);
			vector pose[4];
			s_aMen[i].GetWorldTransform(pose);
			pose[3] = AIO_Maneuvers.Ground(s_vLane + Vector(0, 0, (i - 3.5) * 3));
			BaseGameEntity.Cast(s_aMen[i]).Teleport(pose);
		}
		foreach (IEntity healed : s_aMen) AIO_Server.Heal(healed); // fixture reset, not production healing
		IEntity man = s_aMen[0];
		AIO_Tasks.MoveTo(man, AIO_Maneuvers.Ground(s_vLane + Vector(12, 0, -10.5)));
		SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(man));
		s_Previous = AIO_Tasks.ActiveHold(utility);
		s_Emergency = new AIO_TestEmergency(utility, null);
		utility.AddAction(s_Emergency);
		IEntity patient = s_aMen[1];
		s_AidPatient = patient;
		AIO_Tasks.HoldHere(patient);
		s_AidPost = AIO_Tasks.ActiveHold(AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(patient)));
		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(SCR_DamageManagerComponent.GetDamageManager(patient));
		damage.AddParticularBleeding("RForearm");
		s_iAidResult = AIO_Tasks.HealSelf(patient);
		GetGame().GetCallqueue().CallLater(EmergencyResult, 3000, false);
	}

	protected static void EmergencyResult()
	{
		SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(s_aMen[0]));
		Check("danger-interrupts", utility.GetExecutedAction() == s_Emergency && AIO_Tasks.ActiveHold(utility) == s_Previous, "emergency wins while the original order survives");
		s_Emergency.SetRemoveAction(true); s_Emergency.Fail();
		GetGame().GetCallqueue().CallLater(RecoveryResult, 25000, false);
	}

	protected static void RecoveryResult()
	{
		SCR_AIUtilityComponent aidUtility = AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(s_AidPatient));
		Check("self-aid-preserves-post", s_iAidResult == 1 && s_bAidRan && AIO_Tasks.ActiveHold(aidUtility) == s_AidPost, string.Format("native heal result %1, ran %2, behavior %3; original post retained", s_iAidResult, s_bAidRan, aidUtility.GetCurrentBehavior()));
		Check("order-resumes", AIO_Tasks.ActiveHold(AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(s_aMen[0]))) == s_Previous && AIO_Bounding.At(s_aMen[0], s_Previous.m_vSpot), "same order reached after interruption");
		AIO_Server.Heal(s_aMen[1]); // reset the self-aid fixture before maneuver checks
		foreach (IEntity body : s_aMen) AIO_Tasks.Rejoin(body, s_Leader);
		vector destination = AIO_Maneuvers.Ground(s_vLane + Vector(45, 0, 0));
		AIO_Bounding.Start(s_aMen, Vector(1, 0, 0), destination, false, 0, s_Leader, s_Bound);
		GetGame().GetCallqueue().CallLater(BoundArrived, 65000, false);
	}

	protected static void BoundArrived()
	{
		Check("bound-real-arrival", s_Bound && s_Bound.m_bFinished && !s_Bound.m_bFailed, string.Format("legs reached %1, paused %2", s_Bound.m_iLegsReached, s_Bound.m_bPaused));
		if (!s_Bound.m_bFinished)
		{
			foreach (AIO_BoundMember stuck : s_Bound.m_aMembers)
				Print(string.Format("[AIO] TEST bound position %1 leg %2 side %3 behavior %4", stuck.m_Agent.GetControlledEntity().GetOrigin(), stuck.m_vLeg, stuck.m_bA, AIO_Tasks.UtilityOf(stuck.m_Agent).GetCurrentBehavior()));
		}
		foreach (IEntity man : s_aMen) AIO_Tasks.Rejoin(man, s_Leader);
		vector destination = AIO_Maneuvers.Ground(s_aMen[0].GetOrigin() + Vector(400, 0, 0));
		AIO_Bounding.Start(s_aMen, Vector(1, 0, 0), destination, false, 0, s_Leader, s_Bound);
		// Simulate successful arrival leg by leg to exercise a long destination
		// without turning a state-machine check into a ten-minute walking test.
		s_iSimSteps = 0;
		GetGame().GetCallqueue().CallLater(SimulateLeg, 100, true);
	}

	protected static void SimulateLeg()
	{
		if (++s_iSimSteps < 100 && !s_Bound.m_bFinished)
		{
			s_Bound.m_fReadySince = GetGame().GetWorld().GetWorldTime() - 3000;
			AIO_Bounding.Tick();
			foreach (AIO_BoundMember member : s_Bound.m_aMembers)
			{
				if (!member.m_bMoving) continue;
				vector transform[4];
				member.m_Agent.GetControlledEntity().GetWorldTransform(transform);
				transform[3] = member.m_vLeg;
				BaseGameEntity.Cast(member.m_Agent.GetControlledEntity()).Teleport(transform);
			}
			AIO_Bounding.Tick();
			return;
		}
		GetGame().GetCallqueue().Remove(SimulateLeg);
		Check("bound-no-twelve-step-cap", s_Bound.m_bFinished && !s_Bound.m_bFailed && s_Bound.m_iLegsReached > 12, string.Format("%1 simulated arrived legs", s_Bound.m_iLegsReached));
		foreach (IEntity soldier : s_aMen) AIO_Tasks.Rejoin(soldier, s_Leader);
		vector destination = AIO_Maneuvers.Ground(s_aMen[0].GetOrigin() + Vector(40, 0, 0));
		AIO_Bounding.Start(s_aMen, Vector(1, 0, 0), destination, false, 0, s_Leader, s_Bound);
		s_Bound.m_fReadySince = GetGame().GetWorld().GetWorldTime() - 3000;
		AIO_Bounding.Tick();
		foreach (AIO_BoundMember blocked : s_Bound.m_aMembers) blocked.m_fProgressAt -= 46000;
		bool side = s_Bound.m_bAMoving;
		AIO_Bounding.Tick();
		Check("bound-stall-pauses", s_Bound.m_bPaused && s_Bound.m_bManualPause && s_Bound.m_bAMoving == side && !s_Bound.m_bFinished, "blocked movement cannot switch elements or announce completion");
		AIO_Bounding.Resume(s_aMen, 0);
		s_Bound.m_fReadySince = GetGame().GetWorld().GetWorldTime() - 3000;
		AIO_Bounding.Tick();
		Check("bound-explicit-resume", !s_Bound.m_bManualPause && !s_Bound.m_bPaused && s_Bound.m_bAMoving == side, "resume retries the same leg");
		AIO_BoundMember survivor = s_Bound.m_aMembers[1];
		vector assigned = survivor.m_vFinal;
		int before = s_Bound.m_aMembers.Count();
		SCR_EntityHelper.DeleteEntityAndChildren(s_Bound.m_aMembers[0].m_Agent.GetControlledEntity());
		AIO_Bounding.Tick();
		Check("bound-casualty-keeps-assignments", !s_Bound.m_bFinished && s_Bound.m_aMembers.Count() == before - 1 && survivor.m_vFinal == assigned, "a casualty never reindexes surviving destinations");
		s_Bound.m_bPaused = true; // already waiting for automatic cover readiness
		s_Bound.m_bManualPause = false;
		for (int loss = 0; loss < 3; loss++)
			SCR_EntityHelper.DeleteEntityAndChildren(s_Bound.m_aMembers[loss].m_Agent.GetControlledEntity());
		GetGame().GetCallqueue().CallLater(HeavyLossesResult, 200, false);
	}

	protected static void HeavyLossesResult()
	{
		AIO_Bounding.Tick();
		Check("bound-heavy-losses-pause", s_Bound.m_bPaused && s_Bound.m_bManualPause && !s_Bound.m_bFinished, string.Format("%1 of %2 remain, pause reason %3", s_Bound.m_aMembers.Count(), s_Bound.m_iInitialCount, s_Bound.m_sPauseReason));
		foreach (IEntity cancelled : s_aMen) AIO_Tasks.Rejoin(cancelled, s_Leader);
		AIO_Bounding.Tick();
		Check("bound-follow-cancels", s_Bound.m_bFinished && s_Bound.m_bFailed, "Follow cancels rather than reporting destination reached");
		Finish();
	}

	protected static void Finish()
	{
		ObserversSystem observers = ObserversSystem.Cast(GetGame().GetWorld().FindSystem(ObserversSystem));
		if (observers) observers.RemoveObserverSP(22103);
		GetGame().GetCallqueue().Remove(Sample);
		Print(string.Format("[AIO] TEST RESULT %1 passed, %2 failed", s_iPass, s_iFail));
		Print("[AIO] TEST COMPLETE");
	}
}
