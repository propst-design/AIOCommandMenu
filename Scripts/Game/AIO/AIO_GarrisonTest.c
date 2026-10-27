// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). GPL v3 or later (see LICENSE).

// -aioGarrisonTest 1: Garrison ordered from 50 m on several different building types near the
// map centre (a fresh fire team for each, full AI LOD). Per building: the order is accepted
// (posts or walk-up), every man still holding a post stands at it on the building's own floor
// (not on furniture, not the floor below), at least one man got in, and the one summary the
// player gets matches what happened. Spawning the squads is the only fixture.
class AIO_GarrisonTest
{
	static const ResourceName TEAM = "{84E5BBAB25EA23E5}Prefabs/Groups/BLUFOR/Group_US_FireTeam.et";
	static const int BUILDINGS = 3;
	static const float WAIT_MS = 120000;

	protected static ref array<IEntity> s_aBuildings = {};
	protected static ref array<string> s_aKinds = {};
	protected static SCR_AIGroup s_Group;
	protected static IEntity s_Building;
	protected static int s_iIndex = -1;
	protected static float s_fOrderedAt;
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
			s_iFail++;
		Print(string.Format("[AIO] TEST %1 %2 %3", name, verdict, detail));
	}

	static void Finish()
	{
		Print(string.Format("[AIO] TEST RESULT %1 passed, %2 failed", s_iPass, s_iFail));
		Print("[AIO] TEST COMPLETE");
	}

	static void Start()
	{
		vector mins, maxs;
		GetGame().GetWorldEntity().GetTerrain(0, 0).GetTerrainBoundBox(mins, maxs);
		vector middle = (mins + maxs) * 0.5;
		GetGame().GetWorld().QueryEntitiesBySphere(middle, 1500, Collect, null, EQueryEntitiesFlags.ALL);
		// One tall (multi-storey) building first, then the nearest other kinds.
		array<IEntity> picked = {};
		IEntity tallest;
		float tallestHeight = 0;
		foreach (IEntity building : s_aBuildings)
		{
			vector bmins, bmaxs;
			building.GetWorldBounds(bmins, bmaxs);
			if (bmaxs[1] - bmins[1] > tallestHeight)
			{
				tallestHeight = bmaxs[1] - bmins[1];
				tallest = building;
			}
		}
		if (tallest)
			picked.Insert(tallest);
		foreach (IEntity other : s_aBuildings)
		{
			if (picked.Count() >= BUILDINGS)
				break;
			if (!picked.Contains(other))
				picked.Insert(other);
		}
		s_aBuildings = picked;
		Print(string.Format("[AIO] TEST garrison: %1 building kinds chosen", s_aBuildings.Count()));
		if (s_aBuildings.IsEmpty())
		{
			Check("garrison-setup", false, "no houses near the map centre");
			Finish();
			return;
		}
		Next();
	}

	// One building per kind (the folder name), whole houses only: no ruins, sheds or factories.
	protected static bool Collect(IEntity entity)
	{
		if (!entity.GetPrefabData())
			return true;
		string name = entity.GetPrefabData().GetPrefabName();
		// Open sheds (a loft and no floor of their own) are left out: honest refusal, not a garrison.
		if ((!name.Contains("Structures/Houses") && !name.Contains("Structures/Buildings")) || name.Contains("/Dst/") || name.Contains("Ruin") || name.Contains("Shed"))
			return true;
		if (AIO_Maneuvers.BuildingOf(entity) != entity)
			return true;
		vector mins, maxs;
		entity.GetWorldBounds(mins, maxs);
		vector size = maxs - mins;
		if (size[0] < 7 || size[2] < 7 || size[0] > 30 || size[2] > 30)
			return true;
		array<string> parts = {};
		name.Split("/", parts, true);
		if (parts.Count() < 2)
			return true;
		string kind = parts[parts.Count() - 2];
		if (s_aKinds.Contains(kind))
			return true;
		s_aKinds.Insert(kind);
		s_aBuildings.Insert(entity);
		return true;
	}

	protected static void Next()
	{
		if (s_Group)
		{
			array<IEntity> old = {};
			AIO_Server.Members(s_Group, old);
			foreach (IEntity body : old)
				SCR_EntityHelper.DeleteEntityAndChildren(body);
			SCR_EntityHelper.DeleteEntityAndChildren(s_Group);
			s_Group = null;
		}
		s_iIndex++;
		if (s_iIndex >= s_aBuildings.Count())
		{
			Finish();
			return;
		}
		s_Building = s_aBuildings[s_iIndex];
		vector start = Spot(s_Building, 50);
		if (start == vector.Zero)
		{
			Check("garrison-setup-" + s_iIndex, false, "no open ground 50 m from " + s_Building.GetPrefabData().GetPrefabName());
			GetGame().GetCallqueue().CallLater(Next, 100, false);
			return;
		}
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = start;
		s_Group = SCR_AIGroup.Cast(GetGame().SpawnEntityPrefab(Resource.Load(TEAM), GetGame().GetWorld(), params));
		Print(string.Format("[AIO] TEST garrison: %1 at %2, team at %3", s_Building.GetPrefabData().GetPrefabName(), s_Building.GetOrigin(), start));
		GetGame().GetCallqueue().CallLater(Order, 10000, false);
	}

	// Flat dry ground about 'distance' from the building, outside its bounds.
	protected static vector Spot(IEntity building, float distance)
	{
		BaseWorld world = GetGame().GetWorld();
		vector mins, maxs;
		building.GetWorldBounds(mins, maxs);
		vector middle = (mins + maxs) * 0.5;
		for (int k = 0; k < 16; k++)
		{
			float angle = k * Math.PI2 / 16;
			float x = middle[0] + Math.Cos(angle) * distance;
			float z = middle[2] + Math.Sin(angle) * distance;
			float y = world.GetSurfaceY(x, z);
			if (y < world.GetOceanBaseHeight() + 2)
				continue;
			float slope = Math.AbsFloat(world.GetSurfaceY(x + 3, z) - y) + Math.AbsFloat(world.GetSurfaceY(x, z + 3) - y);
			if (slope < 1.2)
				return Vector(x, y, z);
		}
		return vector.Zero;
	}

	protected static void Men(notnull array<IEntity> men)
	{
		AIO_Server.Members(s_Group, men);
		if (s_Group.GetLeaderAgent())
			men.RemoveItem(s_Group.GetLeaderAgent().GetControlledEntity());
	}

	protected static void Order()
	{
		s_Group.PreventMaxLOD();
		s_Group.SetPermanentLOD(0);
		array<IEntity> men = {};
		Men(men);
		foreach (IEntity man : men)
		{
			AIAgent agent = AIO_Tasks.AgentOf(man);
			if (agent)
			{
				agent.PreventMaxLOD();
				agent.SetPermanentLOD(0);
			}
		}
		IEntity leader;
		if (s_Group.GetLeaderAgent())
			leader = s_Group.GetLeaderAgent().GetControlledEntity();
		AIO_Maneuvers.s_sLastGarrison = "";
		string reply = AIO_Maneuvers.Defense(men, leader, AIO_EOrder.DEF_GARRISON, vector.Zero, s_Building);
		Check("garrison-order-" + s_iIndex, reply.Contains("garrisoning") || reply.Contains("moving up"), reply);
		s_fOrderedAt = GetGame().GetWorld().GetWorldTime();
		GetGame().GetCallqueue().CallLater(Poll, 3000, false);
	}

	protected static bool Pending()
	{
		foreach (AIO_Approach job : AIO_Maneuvers.s_aApproaches)
		{
			if (job.m_Building == s_Building)
				return true;
		}
		return false;
	}

	protected static void Poll()
	{
		float waited = GetGame().GetWorld().GetWorldTime() - s_fOrderedAt;
		if (Pending() && waited < WAIT_MS)
		{
			GetGame().GetCallqueue().CallLater(Poll, 3000, false);
			return;
		}
		array<IEntity> men = {};
		Men(men);
		int holding;
		int atPost;
		foreach (IEntity man : men)
		{
			AIO_HoldBehavior hold = AIO_Tasks.ActiveHold(AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(man)));
			if (!hold || hold.m_Building != s_Building)
				continue;
			holding++;
			if (AIO_Tasks.InteriorArrived(man, hold) && AIO_Maneuvers.BuildingFloor(hold.m_vSpot, man, s_Building, true))
				atPost++;
		}
		string summary = AIO_Maneuvers.s_sLastGarrison;
		string name = s_Building.GetPrefabData().GetPrefabName();
		Check("garrison-in-" + s_iIndex, atPost >= 1 && atPost == holding && !Pending(), string.Format("%1 of %2 men at interior posts, %3 holding, after %4 s in %5", atPost, men.Count(), holding, waited / 1000, name));
		bool honest = summary.Contains(string.Format("Garrison: %1 men in position", atPost)) && (atPost == men.Count() || summary.Contains("rejoined") || summary.Contains("still"));
		Check("garrison-report-" + s_iIndex, honest, "summary: " + summary);
		foreach (IEntity man : men)
			AIO_Tasks.Release(man);
		GetGame().GetCallqueue().CallLater(Next, 1000, false);
	}
}
