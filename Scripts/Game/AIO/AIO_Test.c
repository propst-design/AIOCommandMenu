// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). Licensed under the GNU GPL v3 or later (see LICENSE).

// Automated checks on a dedicated server (no player). Started by host options:
//   -aioTaskTest 1 : orders to single soldiers (move a man, hold, rejoin).
// Prints "[AIO] TEST <name> PASS|FAIL ..." lines and "[AIO] TEST COMPLETE".
class AIO_Test
{
	static const ResourceName GROUP = "{84E5BBAB25EA23E5}Prefabs/Groups/BLUFOR/Group_US_FireTeam.et";

	protected static SCR_AIGroup s_Group;
	protected static vector s_vStart;
	protected static vector s_vTarget;
	protected static IEntity s_A;
	protected static IEntity s_B;
	protected static IEntity s_C;
	protected static int s_iPass;
	protected static int s_iFail;

	static void MaybeStart()
	{
		string value;
		if (!System.GetCLIParam("aioTaskTest", value) || value != "1")
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
		AIO_Tasks.Release(s_A);
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
		AIO_Tasks.Release(s_B);
		Finish();
	}
}
