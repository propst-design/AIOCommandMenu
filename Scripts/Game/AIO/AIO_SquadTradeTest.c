// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). GPL v3 or later (see LICENSE).

// -aioSquadTradeTest 1: More > Recruit / dismiss through AIO_Recruit.Run, as the menu's server side.
// Two leaders' squads (US fire teams, each the slave of a playable master like a player's squad), a free
// AI fire team 30 m off, another 20 m off for Recruit nearby, a fire team with a "possessed" man (another
// player's squad), one 160 m off and a USSR team 600 m off. No players on the server: the leaders are
// the squads' AI leaders, so the "another player's squad" case is the possessed one (a master holding
// real players can't be built here).
class AIO_SquadTradeTest
{
	static const ResourceName TEAM = "{84E5BBAB25EA23E5}Prefabs/Groups/BLUFOR/Group_US_FireTeam.et";
	static const ResourceName ENEMY = "{657590C1EC9E27D3}Prefabs/Groups/OPFOR/Group_USSR_LightFireTeam.et";

	protected static ref array<IEntity> s_aRecruits;
	protected static ref array<float> s_aRecruitStart;
	// Called by AIO_StaticInit at game start (static initializer budget: tools/check_static_init.py).
	static void AIO_InitStatics()
	{
		s_aRecruits = {};
		s_aRecruitStart = {};
	}
	protected static SCR_AIGroup s_MasterA, s_MasterB;
	protected static SCR_AIGroup s_A, s_B, s_Free, s_Near, s_Possessed, s_Far, s_Enemy;
	protected static IEntity s_LeaderA, s_LeaderB;
	protected static IEntity s_Dismissed1, s_Dismissed2;
	protected static vector s_vBase;
	protected static int s_iPass, s_iFail, s_iCountA;

	static void Check(string name, bool ok, string detail)
	{
		string verdict = "FAIL";
		if (ok)
		{
			verdict = "PASS";
			s_iPass++;
		}
		else
			s_iFail++;
		Print(string.Format("[AIO] TEST %1 %2 %3", name, verdict, detail));
	}

	static void Finish()
	{
		AIO_Server.s_TestPossessed = null;
		Print(string.Format("[AIO] TEST RESULT %1 passed, %2 failed", s_iPass, s_iFail));
		Print("[AIO] TEST COMPLETE");
	}

	// A dry, fairly flat spot near the middle of the map.
	protected static vector Spot(vector near)
	{
		BaseWorld world = GetGame().GetWorld();
		float sea = world.GetOceanBaseHeight();
		for (int ring = 0; ring < 40; ring++)
		{
			for (int k = 0; k < 8; k++)
			{
				float angle = 0.785 * k;
				float x = near[0] + Math.Cos(angle) * ring * 25;
				float z = near[2] + Math.Sin(angle) * ring * 25;
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

	protected static SCR_AIGroup SpawnTeam(ResourceName prefab, vector offset)
	{
		vector at = s_vBase + offset;
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = at;
		return SCR_AIGroup.Cast(GetGame().SpawnEntityPrefab(Resource.Load(prefab), GetGame().GetWorld(), params));
	}

	protected static SCR_AIGroup MakeMaster()
	{
		SCR_GroupsManagerComponent manager = SCR_GroupsManagerComponent.GetInstance();
		SCR_FactionManager factions = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		if (!manager || !factions)
			return null;
		SCR_AIGroup master = manager.CreateNewPlayableGroup(factions.GetFactionByKey("US"));
		if (!master)
			return null;
		master.SetDeleteWhenEmpty(false);
		master.SetCanDeleteIfNoPlayer(false);
		return master;
	}

	static void Start()
	{
		vector mins, maxs;
		GetGame().GetWorldEntity().GetTerrain(0, 0).GetTerrainBoundBox(mins, maxs);
		s_vBase = Spot((mins + maxs) * 0.5);
		if (s_vBase == vector.Zero)
		{
			Check("setup", false, "no dry spot found");
			Finish();
			return;
		}
		// CreateNewPlayableGroup finishes on a delayed call: masters first, squads linked later.
		s_MasterA = MakeMaster();
		s_MasterB = MakeMaster();
		s_A = SpawnTeam(TEAM, vector.Zero);
		s_B = SpawnTeam(TEAM, Vector(0, 0, -60));
		s_Free = SpawnTeam(TEAM, Vector(30, 0, 0));
		s_Near = SpawnTeam(TEAM, Vector(0, 0, 20));
		s_Possessed = SpawnTeam(TEAM, Vector(-25, 0, 0));
		s_Far = SpawnTeam(TEAM, Vector(160, 0, 0));
		s_Enemy = SpawnTeam(ENEMY, Vector(600, 0, 0));
		Print(string.Format("[AIO] TEST squadtrade: teams spawned around %1", s_vBase));
		GetGame().GetCallqueue().CallLater(Link, 4000, false);
	}

	protected static void Link()
	{
		SCR_GroupsManagerComponent manager = SCR_GroupsManagerComponent.GetInstance();
		if (manager && s_MasterA && s_A)
			manager.RequestSetGroupSlave(SCR_EntityHelper.EntityToRplId(s_MasterA), SCR_EntityHelper.EntityToRplId(s_A));
		if (manager && s_MasterB && s_B)
			manager.RequestSetGroupSlave(SCR_EntityHelper.EntityToRplId(s_MasterB), SCR_EntityHelper.EntityToRplId(s_B));
		GetGame().GetCallqueue().CallLater(Refusals, 8000, false);
	}

	protected static IEntity Man(SCR_AIGroup group, int index)
	{
		array<IEntity> men = {};
		AIO_Server.Members(group, men);
		IEntity leader;
		if (group)
			leader = group.GetLeaderEntity();
		men.RemoveItem(leader);
		if (index < men.Count())
			return men[index];
		return null;
	}

	protected static int Count(SCR_AIGroup group)
	{
		array<IEntity> men = {};
		AIO_Server.Members(group, men);
		return men.Count();
	}

	protected static SCR_AIGroup GroupOf(IEntity man)
	{
		AIAgent agent = AIO_Tasks.AgentOf(man);
		if (!agent)
			return null;
		return SCR_AIGroup.Cast(agent.GetParentGroup());
	}

	protected static string Order(SCR_AIGroup squad, IEntity leader, bool admin, int order, IEntity target, array<IEntity> selected = null)
	{
		array<IEntity> picked = {};
		if (selected)
			picked.Copy(selected);
		vector point = vector.Zero;
		if (target)
			point = target.GetOrigin();
		string reply = AIO_Recruit.Run(squad, leader, admin, order, picked, point, target);
		Print(string.Format("[AIO] TEST squadtrade: order %1 -> %2", typename.EnumToString(AIO_EOrder, order), reply));
		return reply;
	}

	// Refusals first, while the free teams still stand where they spawned.
	protected static void Refusals()
	{
		if (s_A)
			s_LeaderA = s_A.GetLeaderEntity();
		if (s_B)
			s_LeaderB = s_B.GetLeaderEntity();
		bool ready = s_A && s_B && s_A.IsSlave() && s_B.IsSlave() && s_LeaderA && s_LeaderB
			&& Count(s_A) >= 3 && Count(s_Free) >= 3 && Count(s_Near) >= 2 && Count(s_Possessed) >= 3 && Count(s_Far) >= 1 && Count(s_Enemy) >= 1;
		Check("setup", ready, string.Format("A %1 (slave %2), free %3, near %4, possessed %5, far %6, enemy %7",
			Count(s_A), s_A && s_A.IsSlave(), Count(s_Free), Count(s_Near), Count(s_Possessed), Count(s_Far), Count(s_Enemy)));
		if (!ready)
		{
			Finish();
			return;
		}
		AIO_Server.s_TestPossessed = s_Possessed.GetLeaderEntity();

		IEntity enemy = Man(s_Enemy, 0);
		if (!enemy)
			enemy = s_Enemy.GetLeaderEntity();
		string reply = Order(s_A, s_LeaderA, false, AIO_EOrder.RECRUIT_MAN, enemy);
		Check("enemy-recruit-refused", reply.Contains("own side") && GroupOf(enemy) == s_Enemy, reply);
		reply = Order(s_A, s_LeaderA, false, AIO_EOrder.DESTROY_MAN, enemy);
		Check("enemy-destroy-not-admin", reply.Contains("admins") && enemy && !enemy.IsDeleted(), reply);
		reply = Order(s_A, s_LeaderA, true, AIO_EOrder.DESTROY_SQUAD, enemy);
		Check("enemy-destroy-admin", reply.Contains("destroyed") && !reply.Contains("Nobody"), reply);

		IEntity far = Man(s_Far, 0);
		reply = Order(s_A, s_LeaderA, false, AIO_EOrder.RECRUIT_MAN, far);
		Check("far-refused", reply.Contains("Too far") && GroupOf(far) == s_Far, reply);

		IEntity other = Man(s_Possessed, 0);
		reply = Order(s_A, s_LeaderA, false, AIO_EOrder.RECRUIT_SQUAD, other);
		Check("other-player-refused", reply.Contains("another player's squad") && GroupOf(other) == s_Possessed, reply);
		reply = Order(s_A, s_LeaderA, true, AIO_EOrder.DESTROY_MAN, other);
		Check("other-player-destroy-refused", reply.Contains("another player's squad") && other && !other.IsDeleted(), reply);
		reply = Order(s_A, s_LeaderA, false, AIO_EOrder.RECRUIT_MAN, AIO_Server.s_TestPossessed);
		Check("player-refused", reply.Contains("player"), reply);

		// Recruit one man, then the rest of his squad.
		int before = Count(s_A);
		int freeCount = Count(s_Free);
		array<IEntity> freeMen = {};
		AIO_Server.Members(s_Free, freeMen);
		IEntity first = Man(s_Free, 0);
		reply = Order(s_A, s_LeaderA, false, AIO_EOrder.RECRUIT_MAN, first);
		Check("recruit-man", reply.Contains("joined") && GroupOf(first) == s_A && Count(s_A) == before + 1, reply);
		reply = Order(s_A, s_LeaderA, false, AIO_EOrder.RECRUIT_MAN, first);
		Check("recruit-already", reply.Contains("already in your squad"), reply);
		IEntity second = s_Free.GetLeaderEntity();
		reply = Order(s_A, s_LeaderA, false, AIO_EOrder.RECRUIT_SQUAD, second);
		Check("recruit-squad", reply.Contains("joined") && Count(s_A) == before + freeCount && (!s_Free || Count(s_Free) == 0), reply + string.Format(" (A %1, was %2 + %3)", Count(s_A), before, freeCount));

		// The vanilla group menu's list of the squad's AI has them.
		int listed;
		foreach (IEntity man : freeMen)
		{
			s_aRecruits.Insert(man);
			s_aRecruitStart.Insert(vector.Distance(man.GetOrigin(), s_LeaderA.GetOrigin()));
			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(man);
			if (character && s_A.GetAIMembers() && s_A.GetAIMembers().Contains(character))
				listed++;
		}
		Check("recruits-listed", listed == freeMen.Count() && listed > 0, string.Format("%1 of %2 recruits on the squad's AI member list", listed, freeMen.Count()));
		GetGame().GetCallqueue().CallLater(Formation, 20000, false);
	}

	// The recruits walked over into the squad's formation; Recruit nearby; dismiss; trade; destroy.
	protected static void Formation()
	{
		int closer;
		string distances;
		foreach (int i, IEntity recruit : s_aRecruits)
		{
			if (!recruit)
				continue;
			float now = vector.Distance(recruit.GetOrigin(), s_LeaderA.GetOrigin());
			distances += string.Format(" %1->%2", Math.Round(s_aRecruitStart[i]), Math.Round(now));
			if (now < 15 && now < s_aRecruitStart[i])
				closer++;
		}
		Check("recruits-in-formation", closer == s_aRecruits.Count() && closer > 0, "metres from the leader:" + distances);

		// Recruit nearby: the near team joins; the possessed team (25 m) is refused, nobody else in 30 m.
		int before = Count(s_A);
		int nearCount = Count(s_Near);
		string reply = Order(s_A, s_LeaderA, true, AIO_EOrder.RECRUIT_NEARBY, null);
		Check("recruit-nearby", reply.Contains("joined") && Count(s_A) == before + nearCount && Count(s_Possessed) >= 3, reply + string.Format(" (A %1, was %2 + %3)", Count(s_A), before, nearCount));

		// The server's AI-per-squad limit holds for a player who isn't an admin.
		SCR_CommandingManagerComponent commanding = SCR_CommandingManagerComponent.GetInstance();
		IEntity spare = Man(s_B, 0); // the other leader's squad has no player on this server: fair game
		if (commanding)
		{
			int limit = commanding.GetMaxAIPerGroup();
			commanding.SetMaxAIPerGroup(s_A.GetServerAgentsCount());
			reply = Order(s_A, s_LeaderA, false, AIO_EOrder.RECRUIT_MAN, spare);
			commanding.SetMaxAIPerGroup(limit);
			Check("squad-limit", reply.Contains("full") && GroupOf(spare) == s_B, reply);
		}
		else
			Check("squad-limit", true, "no commanding manager on this world: no limit to keep");

		// Dismiss two men, one in Stealth with his own fire rule: they leave clean, as a squad of their side.
		s_Dismissed1 = Man(s_A, 0);
		s_Dismissed2 = Man(s_A, 1);
		AIO_Behaviour.Set(AIO_Tasks.AgentOf(s_Dismissed1), AIO_EBehaviour.STEALTH);
		AIO_Maneuvers.SetROE(s_Dismissed1, EAIGroupCombatMode.HOLD_FIRE);
		reply = Order(s_A, s_LeaderA, false, AIO_EOrder.DISMISS_SELECTED, null);
		Check("dismiss-needs-selection", reply.Contains("Select"), reply);
		array<IEntity> selected = {};
		selected.Insert(s_Dismissed1);
		selected.Insert(s_Dismissed2);
		selected.Insert(s_LeaderA); // the leader himself is never dismissed
		before = Count(s_A);
		reply = Order(s_A, s_LeaderA, false, AIO_EOrder.DISMISS_SELECTED, null, selected);
		SCR_AIGroup group = GroupOf(s_Dismissed1);
		string faction = "";
		if (group && group.GetFaction())
			faction = group.GetFaction().GetFactionKey();
		bool together = group && group == GroupOf(s_Dismissed2) && group != s_A && !group.IsSlave() && !group.IsPlayable();
		Check("dismiss", reply.Contains("2 men dismissed") && together && faction == "US" && Count(s_A) == before - 2 && GroupOf(s_LeaderA) == s_A,
			reply + string.Format(" (faction %1, A %2 was %3)", faction, Count(s_A), before));
		SCR_ChimeraCharacter gone = SCR_ChimeraCharacter.Cast(s_Dismissed1);
		bool unlisted = !gone || !s_A.GetAIMembers() || !s_A.GetAIMembers().Contains(gone);
		Check("dismiss-clean", AIO_Behaviour.ModeOf(AIO_Tasks.AgentOf(s_Dismissed1)) == AIO_EBehaviour.AWARE && AIO_Maneuvers.ROEOf(s_Dismissed1) == -1 && unlisted,
			string.Format("mode %1, own ROE %2, still listed %3", AIO_Behaviour.ModeOf(AIO_Tasks.AgentOf(s_Dismissed1)), AIO_Maneuvers.ROEOf(s_Dismissed1), !unlisted));
		GetGame().GetCallqueue().CallLater(Trade, 3000, false);
	}

	// The other leader recruits the dismissed pair: a trade.
	protected static void Trade()
	{
		int before = Count(s_B);
		string reply = Order(s_B, s_LeaderB, false, AIO_EOrder.RECRUIT_SQUAD, s_Dismissed2);
		Check("trade", reply.Contains("2 men joined") && GroupOf(s_Dismissed1) == s_B && GroupOf(s_Dismissed2) == s_B && Count(s_B) == before + 2, reply);

		// Destroy: own selected man, a man of another AI squad, but not the whole of one's own squad.
		IEntity own = Man(s_A, 0);
		array<IEntity> selected = {};
		selected.Insert(own);
		s_iCountA = Count(s_A);
		reply = Order(s_A, s_LeaderA, false, AIO_EOrder.DESTROY_SELECTED, null, selected);
		Check("destroy-selected", reply.Contains("destroyed") && !reply.Contains("Nobody"), reply);
		reply = Order(s_A, s_LeaderA, false, AIO_EOrder.DESTROY_SQUAD, Man(s_A, 1));
		Check("destroy-own-squad-refused", reply.Contains("your own squad"), reply);
		IEntity theirs = Man(s_B, 0);
		reply = Order(s_A, s_LeaderA, false, AIO_EOrder.DESTROY_MAN, theirs);
		Check("destroy-man", reply.Contains("destroyed") && !reply.Contains("Nobody"), reply);
		s_aRecruits.Clear();
		s_aRecruits.Insert(own);
		s_aRecruits.Insert(theirs);
		GetGame().GetCallqueue().CallLater(Gone, 2000, false);
	}

	protected static void Gone()
	{
		bool gone = true;
		foreach (IEntity man : s_aRecruits)
		{
			if (man && !man.IsDeleted())
				gone = false;
		}
		Check("destroyed-gone", gone && Count(s_A) == s_iCountA - 1, string.Format("A %1, was %2", Count(s_A), s_iCountA));
		Finish();
	}
}
