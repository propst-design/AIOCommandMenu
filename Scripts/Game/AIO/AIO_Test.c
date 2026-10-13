// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). Licensed under the GNU GPL v3 or later (see LICENSE).

// Automated checks on a dedicated server (no player). Started by host options:
//   -aioTaskTest 1 : orders to single soldiers (move a man, hold, rejoin).
//   -aioMoveTest 1 : Move menu (Advance keeps station on the leader while he moves; Fall back).
//   -aioInfantryTest 1 : Take cover, stance, Heal up (nobody hurt).
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
	protected static bool s_bMoveTest;
	protected static bool s_bInfantryTest;
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
		if (!s_bMoveTest && !s_bInfantryTest && (!System.GetCLIParam("aioTaskTest", value) || value != "1"))
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
		if (s_bInfantryTest)
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
			holds.Clear();
			utility.FindActionsOfType(AIO_HoldBehavior, holds);
			if (holds.IsEmpty())
				continue;
			AIO_HoldBehavior hold = AIO_HoldBehavior.Cast(holds[0]);
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
		string labelB = AIO_Tasks.TaskLabel(s_B);
		string labelA = AIO_Tasks.TaskLabel(s_A);
		Check("squad-bar-labels", labelB == "Holding" && labelA == "", string.Format("B '%1', A '%2'", labelB, labelA));
		AIO_Tasks.Release(s_B);
		Finish();
	}
}
