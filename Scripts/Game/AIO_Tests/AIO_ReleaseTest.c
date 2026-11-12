// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). Licensed under the GNU GPL v3 or later (see LICENSE).

// -aioReleaseTest 1: orders end when no player commands the squad any more (a leader stepped down or
// left, a Game Master or another mod's commander let go of the leader). A fire team linked to a playable
// group gets a held spot, prone, hold fire and Combat mode on one man and a crouch on another. While the
// test counts the squad as commanded (AIO_Server.s_bTestCommandAll, the stand-in for its player) they
// keep them; once it stops, the watchdog must let both men go within a few seconds. Also: a leader no
// player controls commands nobody, and an errand gives a hold back only while someone commands the man.
class AIO_ReleaseTest
{
	static const ResourceName TEAM = "{84E5BBAB25EA23E5}Prefabs/Groups/BLUFOR/Group_US_FireTeam.et";

	protected static SCR_AIGroup s_Master, s_Squad;
	protected static IEntity s_A, s_B;
	protected static vector s_vBase;
	protected static int s_iPass, s_iFail;

	static void Check(string name, bool ok, string detail)
	{
		string verdict = "FAIL";
		if (ok)
		{
			verdict = "PASS";
			s_iPass++;
		}
		else
		{
			s_iFail++;
		}
		Print(string.Format("[AIO] TEST %1 %2 %3", name, verdict, detail));
	}

	protected static void Finish()
	{
		AIO_Server.s_bTestCommandAll = true;
		Print(string.Format("[AIO] TEST RESULT %1 passed, %2 failed", s_iPass, s_iFail));
		AIO_TestVerdict.Report(s_iPass, s_iFail);
		Print("[AIO] TEST COMPLETE");
	}

	// A dry, fairly flat spot near the middle of the map (as AIO_SquadTradeTest).
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

	static void Start()
	{
		vector mins, maxs;
		GetGame().GetWorldEntity().GetTerrain(0, 0).GetTerrainBoundBox(mins, maxs);
		s_vBase = Spot((mins + maxs) * 0.5);
		SCR_GroupsManagerComponent manager = SCR_GroupsManagerComponent.GetInstance();
		SCR_FactionManager factions = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		if (s_vBase == vector.Zero || !manager || !factions)
		{
			Check("setup", false, "no dry spot, groups manager or faction manager");
			Finish();
			return;
		}
		s_Master = manager.CreateNewPlayableGroup(factions.GetFactionByKey("US"));
		if (s_Master)
		{
			s_Master.SetDeleteWhenEmpty(false);
			s_Master.SetCanDeleteIfNoPlayer(false);
		}
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = s_vBase;
		s_Squad = SCR_AIGroup.Cast(GetGame().SpawnEntityPrefab(Resource.Load(TEAM), GetGame().GetWorld(), params));
		Print(string.Format("[AIO] TEST release: team spawned at %1", s_vBase));
		GetGame().GetCallqueue().CallLater(Link, 4000, false);
	}

	protected static void Link()
	{
		SCR_GroupsManagerComponent manager = SCR_GroupsManagerComponent.GetInstance();
		if (manager && s_Master && s_Squad)
			manager.RequestSetGroupSlave(SCR_EntityHelper.EntityToRplId(s_Master), SCR_EntityHelper.EntityToRplId(s_Squad));
		GetGame().GetCallqueue().CallLater(Give, 5000, false);
	}

	protected static IEntity Man(int index)
	{
		array<IEntity> members = {};
		AIO_Server.Members(s_Squad, members);
		if (index < members.Count())
			return members[index];
		return null;
	}

	protected static string State(IEntity man)
	{
		if (!man)
			return "(gone)";
		AIAgent agent = AIO_Tasks.AgentOf(man);
		bool held = AIO_Tasks.ActiveHold(AIO_Tasks.UtilityOf(agent)) != null;
		return string.Format("hold=%1 stance=%2 roe=%3 mode=%4", held, AIO_Tasks.StanceOrderOf(agent),
			AIO_Maneuvers.ROEOf(man), typename.EnumToString(AIO_EBehaviour, AIO_Behaviour.ModeOf(agent)));
	}

	protected static void Give()
	{
		s_A = Man(1);
		s_B = Man(2);
		if (!s_A || !s_B || s_Master.GetSlave() != s_Squad)
		{
			Check("setup", false, "team not linked or too small");
			Finish();
			return;
		}
		AIAgent a = AIO_Tasks.AgentOf(s_A);
		AIO_Tasks.MoveTo(s_A, s_A.GetOrigin() + Vector(6, 0, 0));
		AIO_Behaviour.Set(a, AIO_EBehaviour.COMBAT);
		AIO_Tasks.SetStance(a, ECharacterStance.PRONE);
		AIO_Maneuvers.SetROE(s_A, EAIGroupCombatMode.HOLD_FIRE);
		AIO_Tasks.SetStance(AIO_Tasks.AgentOf(s_B), ECharacterStance.CROUCH);
		Check("orders given", Ordered(s_A, s_B), "A: " + State(s_A) + " | B: " + State(s_B));
		GetGame().GetCallqueue().CallLater(StillCommanded, 4000, false);
	}

	protected static bool Ordered(IEntity a, IEntity b)
	{
		AIAgent agentA = AIO_Tasks.AgentOf(a);
		return AIO_Tasks.ActiveHold(AIO_Tasks.UtilityOf(agentA)) != null && AIO_Tasks.StanceOrderOf(agentA) == ECharacterStance.PRONE
			&& AIO_Maneuvers.ROEOf(a) == EAIGroupCombatMode.HOLD_FIRE && AIO_Behaviour.ModeOf(agentA) == AIO_EBehaviour.COMBAT
			&& AIO_Tasks.StanceOrderOf(AIO_Tasks.AgentOf(b)) == ECharacterStance.CROUCH;
	}

	protected static void StillCommanded()
	{
		Check("kept while commanded", Ordered(s_A, s_B), "4 s later, A: " + State(s_A) + " | B: " + State(s_B));
		// The leader body is an AI: with the test's stand-in off, he commands nobody.
		IEntity leader = Man(0);
		bool withStandIn = AIO_Server.StillCommands(leader, s_A);
		AIO_Server.s_bTestCommandAll = false;
		bool without = AIO_Server.StillCommands(leader, s_A);
		Check("non-player leader commands nobody", withStandIn && !without, string.Format("stand-in=%1 none=%2", withStandIn, without));
		GetGame().GetCallqueue().CallLater(Released, 5500, false);
	}

	protected static void Released()
	{
		AIAgent a = AIO_Tasks.AgentOf(s_A);
		bool freeA = AIO_Tasks.ActiveHold(AIO_Tasks.UtilityOf(a)) == null && AIO_Tasks.StanceOrderOf(a) < 0
			&& AIO_Maneuvers.ROEOf(s_A) < 0 && AIO_Behaviour.ModeOf(a) == AIO_EBehaviour.AWARE;
		bool freeB = AIO_Tasks.StanceOrderOf(AIO_Tasks.AgentOf(s_B)) < 0;
		Check("released when no player commands", freeA && freeB, "A: " + State(s_A) + " | B: " + State(s_B));

		// An errand ending now must not post him again; under command it gives his spot back.
		AIO_SavedHold saved = new AIO_SavedHold();
		saved.m_bHad = true;
		saved.m_vSpot = s_A.GetOrigin() + Vector(0, 0, 5);
		saved.Restore(s_A);
		bool noPost = AIO_Tasks.ActiveHold(AIO_Tasks.UtilityOf(a)) == null;
		AIO_Server.s_bTestCommandAll = true;
		saved.Restore(s_A);
		bool post = AIO_Tasks.ActiveHold(AIO_Tasks.UtilityOf(a)) != null;
		Check("errand hold back only under command", noPost && post, string.Format("uncommanded=%1 commanded=%2", !noPost, post));
		Finish();
	}
}
