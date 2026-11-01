// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). Licensed under the GNU GPL v3 or later (see LICENSE).

// Defense > Grenades & smoke: men carrying hand grenades throw them at the point (map cursor when
// the map is open). Uses the game's own throw (SCR_AIThrowGrenadeToBehavior, the behaviour its smoke
// cover for medics uses) at player priority: it outranks AiO moves and holds, so a man keeps his
// post and throws from it, while native danger, melee and high-priority attack still come first.
// The game's throw goes wherever he faces if its aim hasn't caught up (an ACA range test threw
// 90 degrees off, 8 m out, into the squad), so he turns to the point first; a frag is called off
// if he can't face it within TURN_MS, smoke is thrown anyway.
// The throw tree gives up beyond 40 m, so a man farther than THROW_RANGE walks up to STAND_OFF
// first and returns to his previous orders afterwards. A throw counts only once the grenade has
// left his inventory. Frags are refused (and held back at the last moment) when a friendly
// soldier is within DANGER_CLOSE of the point. Server only.
// - Frag there / Smoke there: the nearest man carrying one.
// - Smoke screen there: up to three men, a wall across the player's line to the point.
// Cover(): Fall back, Peel back and Attack > Flank add a smoke screen between the squad and the
// threat when the squad is in contact (not AIO_Behaviour.Quiet), thrown by carriers among the men
// on top of their new movement; quiet moves throw nothing.

class AIO_Throw
{
	AIAgent m_Agent;
	EWeaponType m_eType;
	vector m_vPoint;
	float m_fDelay;
	int m_iPlayerId;
	int m_iStage;              // 0 moving up, 2 turning to face the point, 1 throwing
	int m_iDeadline;           // tick count
	int m_iGiveUp;             // tick count: self-aid pauses the deadline up to here
	int m_iCarried;            // grenades of that type when the throw began
	ref array<IEntity> m_aHeld = {}; // ...and which ones
	ref SCR_AIThrowGrenadeToBehavior m_Behavior;
	// walked up: what he was doing before (restored afterwards)
	bool m_bMoved;
	bool m_bHadHold;
	vector m_vSpot;
	IEntity m_Anchor;
	vector m_vOffset;
	bool m_bEnding;
	bool m_bCover;             // smoke added to a maneuver
}

// A thrown frag in flight: where it bursts, for the log.
class AIO_Burst
{
	IEntity m_Grenade;
	vector m_vPoint;
	vector m_vFrom;
	vector m_vLast;
	string m_sWho;
	int m_iDeadline;
}

class AIO_Grenades
{
	static const float THROW_RANGE = 32;  // the game's throw tree refuses beyond 40 m
	static const float STAND_OFF = 25;    // where a man out of range walks up to
	static const float WALK_LIMIT = 120;  // farther than this from the point: refused
	static const float DANGER_CLOSE = 15; // frag: no friendly soldier this close to the point (M67 casualty radius)
	static const float SCREEN_GAP = 6;
	static const int WALK_MS = 60000;
	static const int THROW_MS = 15000;
	static const int GIVE_UP_MS = 180000;
	static const int TURN_MS = 6000;
	static const float TURN_LOOK_PRIORITY = 60; // above targets (50+): face the ordered point, briefly
	static const float FACING_DEG = 25;

	protected static ref array<ref AIO_Throw> s_aThrows = {};
	protected static ref array<IEntity> s_aQueryHits;
	protected static ref array<ref AIO_Burst> s_aBursts = {};

	// Mission end (AIO_Game.c): the jobs belong to the world being torn down.
	static void Cleanup()
	{
		s_aThrows.Clear();
		s_aBursts.Clear();
	}
	static int s_iThrown; // grenades that left a thrower's inventory (tests)
	static float s_fLastMiss = -1; // last frag burst's distance from its point (tests)
	static vector s_vLastCover;    // centre of the last cover screen (tests)

	static bool IsOrder(int order)
	{
		return order == AIO_EOrder.GREN_FRAG || order == AIO_EOrder.GREN_SMOKE || order == AIO_EOrder.GREN_SMOKE_SCREEN;
	}

	static string Order(array<IEntity> bodies, IEntity player, int order, vector point, int playerId)
	{
		if (point == vector.Zero)
			return "Point at the spot, or open the map and put the cursor on it";
		EWeaponType type = EWeaponType.WT_SMOKEGRENADE;
		string what = "smoke";
		if (order == AIO_EOrder.GREN_FRAG)
		{
			type = EWeaponType.WT_FRAGGRENADE;
			what = "frag";
		}
		array<vector> spots = {point};
		if (order == AIO_EOrder.GREN_SMOKE_SCREEN)
		{
			// A wall across the line from the player (or the squad) to the point.
			vector from = point;
			if (player)
				from = player.GetOrigin();
			else if (!bodies.IsEmpty())
				from = bodies[0].GetOrigin();
			vector line = point - from;
			line[1] = 0;
			if (line.Length() < 0.1)
				line = "0 0 1";
			line.Normalize();
			vector across = Vector(-line[2], 0, line[0]);
			spots.Insert(Ground(point + across * SCREEN_GAP));
			spots.Insert(Ground(point - across * SCREEN_GAP));
		}
		if (type == EWeaponType.WT_FRAGGRENADE)
		{
			string close = DangerClose(point, player);
			if (!close.IsEmpty())
				return close;
		}

		array<IEntity> free = {};
		foreach (IEntity body : bodies)
		{
			if (body != player && Ready(body) && Count(body, type) > 0)
				free.Insert(body);
		}
		if (free.IsEmpty())
			return string.Format("Nobody on foot carries a %1 grenade", what);

		array<string> names = {};
		bool walking = false;
		foreach (int i, vector spot : spots)
		{
			float best;
			string blocked;
			IEntity thrower = Pick(free, spot, type == EWeaponType.WT_FRAGGRENADE, best, blocked);
			if (!thrower || best > WALK_LIMIT)
			{
				if (i > 0)
					break;
				if (!thrower)
					return blocked;
				return string.Format("Too far to throw: the nearest man with a %1 grenade is %2 m from there", what, Math.Round(best));
			}
			free.RemoveItem(thrower);
			if (!Start(thrower, type, spot, i * 0.8, playerId))
				continue;
			names.Insert(AIO_Server.NameOf(thrower));
			if (best > THROW_RANGE)
				walking = true;
			if (free.IsEmpty())
				break;
		}
		if (names.IsEmpty())
			return "Couldn't get anyone to throw";
		Print(string.Format("[AIO] GRENADES %1 x %2 at %3", what, names.Count(), point));
		string who = names[0];
		for (int n = 1; n < names.Count(); n++)
			who += ", " + names[n];
		string reply;
		if (order == AIO_EOrder.GREN_SMOKE_SCREEN)
			reply = string.Format("Smoke screen: %1", who);
		else
			reply = string.Format("%1 throwing %2 there", who, what);
		if (walking)
			reply += " (moving up to throw)";
		return reply;
	}

	// Smoke screen for a maneuver just ordered: up to count smokes across the line from the men to the
	// threat, 8-20 m out from them. Only in contact, only carriers already within throwing range.
	// Returns a reply suffix, empty when nothing is thrown.
	static string Cover(array<IEntity> bodies, vector threat, int count, int playerId)
	{
		if (bodies.IsEmpty() || !InContact(bodies))
			return "";
		vector from = AIO_Maneuvers.Centroid(bodies);
		vector line = threat - from;
		line[1] = 0;
		float distance = line.Length();
		if (distance < 1)
			return "";
		line.Normalize();
		vector across = Vector(-line[2], 0, line[0]);
		vector centre = from + line * Math.Clamp(distance * 0.5, 8, 20);
		s_vLastCover = centre;
		array<vector> spots = {Ground(centre), Ground(centre + across * SCREEN_GAP), Ground(centre - across * SCREEN_GAP)};
		array<IEntity> free = {};
		foreach (IEntity body : bodies)
		{
			if (Ready(body) && Count(body, EWeaponType.WT_SMOKEGRENADE) > 0)
				free.Insert(body);
		}
		if (free.IsEmpty())
			return "; no smoke to cover it";
		array<string> names = {};
		for (int i = 0; i < count && i < spots.Count() && !free.IsEmpty(); i++)
		{
			IEntity thrower = null;
			float best = THROW_RANGE;
			foreach (IEntity body : free)
			{
				float d = vector.Distance(body.GetOrigin(), spots[i]);
				if (d <= best)
				{
					best = d;
					thrower = body;
				}
			}
			if (!thrower)
				break;
			free.RemoveItem(thrower);
			if (!Start(thrower, EWeaponType.WT_SMOKEGRENADE, spots[i], i * 0.8, playerId))
				continue;
			s_aThrows[s_aThrows.Count() - 1].m_bCover = true;
			names.Insert(AIO_Server.NameOf(thrower));
		}
		if (names.IsEmpty())
			return "";
		Print(string.Format("[AIO] GRENADES cover smoke x %1 toward %2", names.Count(), threat));
		string who = names[0];
		for (int n = 1; n < names.Count(); n++)
			who += ", " + names[n];
		return "; smoke: " + who;
	}

	// Someone among the men is aware of a threat (the opposite of the Safe mode's quiet).
	static bool InContact(array<IEntity> bodies)
	{
		foreach (IEntity body : bodies)
		{
			AIAgent agent = AIO_Tasks.AgentOf(body);
			if (agent && AIO_Server.IsAlive(body) && !AIO_Behaviour.Quiet(agent))
				return true;
		}
		return false;
	}

	// Centre of the enemies the men have detected in the last 30 s, or fallback.
	static vector ThreatPoint(array<IEntity> bodies, vector fallback)
	{
		vector sum = vector.Zero;
		int found;
		foreach (IEntity body : bodies)
		{
			SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(body));
			if (!utility || !utility.m_PerceptionComponent)
				continue;
			array<BaseTarget> targets = {};
			utility.m_PerceptionComponent.GetTargetsList(targets, ETargetCategory.ENEMY);
			foreach (BaseTarget target : targets)
			{
				float since = target.GetTimeSinceDetected();
				if (since < 0 || since > 30)
					continue;
				sum += target.GetLastDetectedPosition();
				found++;
			}
		}
		if (found == 0)
			return fallback;
		return sum * (1.0 / found);
	}

	// The nearest carrier to the spot; for a frag within throwing range, one whose lob isn't blocked
	// close in front of him (it would bounce back into the squad).
	protected static IEntity Pick(array<IEntity> free, vector spot, bool frag, out float best, out string blocked)
	{
		array<IEntity> left = {};
		left.Copy(free);
		while (!left.IsEmpty())
		{
			IEntity pick = null;
			best = float.MAX;
			foreach (IEntity body : left)
			{
				float distance = vector.Distance(body.GetOrigin(), spot);
				if (distance < best)
				{
					best = distance;
					pick = body;
				}
			}
			if (!frag || best > THROW_RANGE)
				return pick;
			float wall = Blocked(pick, spot);
			if (wall < 0 || wall >= DANGER_CLOSE)
				return pick;
			blocked = string.Format("No clear throw: %1's throw is blocked %2 m in front of him", AIO_Server.NameOf(pick), Math.Round(wall));
			left.RemoveItem(pick);
		}
		best = float.MAX;
		return null;
	}

	// How far from him a lob to the point first hits something (projectile collision), -1 if clear.
	// Two legs: hand to an apex above the midpoint, apex to just above the point.
	static float Blocked(IEntity body, vector point)
	{
		vector hand = body.GetOrigin() + "0 1.7 0";
		float distance = vector.DistanceXZ(hand, point);
		vector apex = (hand + point) * 0.5 + Vector(0, Math.Clamp(distance * 0.3, 1.5, 9), 0);
		array<vector> legs = {hand, apex, point + "0 0.5 0"};
		for (int leg = 0; leg < 2; leg++)
		{
			TraceParam trace = new TraceParam();
			trace.Start = legs[leg];
			trace.End = legs[leg + 1];
			trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
			trace.LayerMask = EPhysicsLayerPresets.Projectile;
			trace.Exclude = body;
			float fraction = GetGame().GetWorld().TraceMove(trace, null);
			if (fraction >= 1)
				continue;
			vector hit = legs[leg] + (legs[leg + 1] - legs[leg]) * fraction;
			float along = vector.DistanceXZ(hand, hit);
			string what = "terrain";
			if (trace.TraceEnt)
				what = trace.TraceEnt.ToString();
			if (trace.TraceEnt && trace.TraceEnt.GetPrefabData())
				what = trace.TraceEnt.GetPrefabData().GetPrefabName();
			Print(string.Format("[AIO] GRENADES %1's throw to %2 m is blocked %3 m out by %4", AIO_Server.NameOf(body), Math.Round(distance), Math.Round(along), what));
			return along;
		}
		return -1;
	}

	// Name of a friendly soldier within DANGER_CLOSE of the point, as a refusal; empty if clear.
	static string DangerClose(vector point, IEntity player)
	{
		Faction ours = FactionOf(player);
		s_aQueryHits = {};
		GetGame().GetWorld().QueryEntitiesBySphere(point, DANGER_CLOSE, QueryHit, null, EQueryEntitiesFlags.ALL);
		string found;
		float nearest = float.MAX;
		foreach (IEntity hit : s_aQueryHits)
		{
			if (!ChimeraCharacter.Cast(hit) || !AIO_Server.IsAlive(hit))
				continue;
			Faction theirs = FactionOf(hit);
			bool friendly = hit == player || (ours && theirs && (theirs == ours || ours.IsFactionFriendly(theirs)));
			float distance = vector.Distance(hit.GetOrigin(), point);
			if (friendly && distance <= DANGER_CLOSE && distance < nearest)
			{
				nearest = distance;
				if (hit == player)
					found = "you are";
				else
					found = AIO_Server.NameOf(hit) + " is";
			}
		}
		s_aQueryHits = null;
		if (found.IsEmpty())
			return "";
		return string.Format("Danger close: %1 %2 m from that spot", found, Math.Round(nearest));
	}

	protected static bool QueryHit(IEntity entity)
	{
		s_aQueryHits.Insert(entity);
		return true;
	}

	protected static Faction FactionOf(IEntity entity)
	{
		if (!entity)
			return null;
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
		if (!affiliation)
			return null;
		return affiliation.GetAffiliatedFaction();
	}

	protected static vector Ground(vector spot)
	{
		spot[1] = GetGame().GetWorld().GetSurfaceY(spot[0], spot[2]);
		return spot;
	}

	// Conscious, on foot and not already throwing.
	protected static bool Ready(IEntity body)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(body);
		if (!character || !AIO_Server.IsAlive(body) || character.IsInVehicle() || character.GetCharacterController().IsUnconscious())
			return false;
		return !IsThrowing(AIO_Tasks.AgentOf(body));
	}

	// Hand grenades of that type he carries (and which, into found).
	static int Count(IEntity body, EWeaponType type, array<IEntity> found = null)
	{
		SCR_InventoryStorageManagerComponent inventory = AIO_Supplies.InventoryOf(body);
		if (!inventory)
			return 0;
		array<IEntity> items = {};
		inventory.GetItems(items);
		int count;
		foreach (IEntity item : items)
		{
			BaseWeaponComponent weapon = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent));
			if (weapon && weapon.GetWeaponType() == type)
			{
				count++;
				if (found)
					found.Insert(item);
			}
		}
		return count;
	}

	protected static bool Start(IEntity body, EWeaponType type, vector spot, float delay, int playerId)
	{
		AIAgent agent = AIO_Tasks.AgentOf(body);
		SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(agent);
		if (!utility)
			return false;
		AIO_Throw job = new AIO_Throw();
		job.m_Agent = agent;
		job.m_eType = type;
		job.m_vPoint = spot;
		job.m_fDelay = delay;
		job.m_iPlayerId = playerId;
		job.m_iGiveUp = System.GetTickCount() + GIVE_UP_MS;
		if (vector.Distance(body.GetOrigin(), spot) > THROW_RANGE)
		{
			AIO_HoldBehavior hold = AIO_Tasks.ActiveHold(utility);
			if (hold)
			{
				job.m_bHadHold = true;
				job.m_vSpot = hold.m_vSpot;
				job.m_Anchor = hold.m_Anchor;
				job.m_vOffset = hold.m_vOffset;
			}
			AIO_Tasks.Release(body);
			vector away = body.GetOrigin() - spot;
			away[1] = 0;
			away.Normalize();
			vector stand = Ground(spot + away * STAND_OFF);
			utility.AddAction(new AIO_MoveBehavior(utility, null, stand, radius: 2.0));
			utility.AddAction(new AIO_HoldBehavior(utility, null, stand));
			job.m_bMoved = true;
			job.m_iDeadline = System.GetTickCount() + WALK_MS;
		}
		else
			Turn(job, body, utility);
		s_aThrows.Insert(job);
		AIO_Tasks.Watch();
		return true;
	}

	protected static void Turn(AIO_Throw job, IEntity body, SCR_AIUtilityComponent utility)
	{
		utility.m_LookAction.LookAt(job.m_vPoint + Vector(0, 1, 0), TURN_LOOK_PRIORITY, 2.0);
		job.m_iStage = 2;
		job.m_iDeadline = System.GetTickCount() + TURN_MS;
	}

	// Degrees between where he faces and the point, on the ground plane.
	static float OffFacing(IEntity body, vector point)
	{
		vector forward = body.GetTransformAxis(2);
		forward[1] = 0;
		vector to = point - body.GetOrigin();
		to[1] = 0;
		if (forward.Length() < 0.01 || to.Length() < 0.01)
			return 0;
		forward.Normalize();
		to.Normalize();
		return Math.Acos(Math.Clamp(vector.Dot(forward, to), -1, 1)) * Math.RAD2DEG;
	}

	protected static void Throw(AIO_Throw job, IEntity body, SCR_AIUtilityComponent utility)
	{
		Print(string.Format("[AIO] GRENADES %1 throws facing %2 deg off the point", AIO_Server.NameOf(body), Math.Round(OffFacing(body, job.m_vPoint))));
		if (job.m_eType != EWeaponType.WT_FRAGGRENADE)
			Blocked(body, job.m_vPoint); // logged only: a smoke landing short does no harm
		job.m_aHeld.Clear();
		job.m_iCarried = Count(body, job.m_eType, job.m_aHeld);
		job.m_Behavior = new SCR_AIThrowGrenadeToBehavior(utility, null, job.m_vPoint, job.m_eType, job.m_fDelay,
			SCR_AIActionBase.PRIORITY_BEHAVIOR_THROW_GRENADE, SCR_AIActionBase.PRIORITY_LEVEL_PLAYER);
		utility.AddAction(job.m_Behavior);
		job.m_iStage = 1;
		job.m_iDeadline = System.GetTickCount() + THROW_MS;
	}

	static bool IsThrowing(AIAgent agent)
	{
		foreach (AIO_Throw job : s_aThrows)
		{
			if (job.m_Agent == agent)
				return true;
		}
		return false;
	}

	static string Label(AIAgent agent)
	{
		foreach (AIO_Throw job : s_aThrows)
		{
			if (job.m_Agent != agent)
				continue;
			if (job.m_iStage == 0)
				return "Moving up to throw";
			if (job.m_eType == EWeaponType.WT_FRAGGRENADE)
				return "Throwing frag";
			return "Throwing smoke";
		}
		return "";
	}

	// A new order for the man calls the throw off (a bound's own next leg doesn't).
	static void Cancel(AIAgent agent)
	{
		for (int i = s_aThrows.Count() - 1; i >= 0; i--)
		{
			AIO_Throw job = s_aThrows[i];
			if (job.m_Agent != agent || job.m_bEnding || (job.m_bCover && AIO_Bounding.IsIssuing()))
				continue;
			if (job.m_Behavior)
				job.m_Behavior.Fail();
			s_aThrows.Remove(i);
		}
	}

	// Called by the AIO_Tasks watchdog every second.
	static void Tick()
	{
		int now = System.GetTickCount();
		for (int b = s_aBursts.Count() - 1; b >= 0; b--)
		{
			AIO_Burst burst = s_aBursts[b];
			if (burst.m_Grenade && !burst.m_Grenade.IsDeleted() && now < burst.m_iDeadline)
			{
				burst.m_vLast = burst.m_Grenade.GetOrigin();
				continue;
			}
			if (!burst.m_Grenade || burst.m_Grenade.IsDeleted())
			{
				s_fLastMiss = vector.Distance(burst.m_vLast, burst.m_vPoint);
				Print(string.Format("[AIO] GRENADES %1's frag burst %2 m from the point, %3 m from where he threw (burst %4, thrower %5, point %6)", burst.m_sWho, Math.Round(s_fLastMiss), Math.Round(vector.Distance(burst.m_vLast, burst.m_vFrom)), burst.m_vLast, burst.m_vFrom, burst.m_vPoint));
			}
			s_aBursts.Remove(b);
		}
		for (int i = s_aThrows.Count() - 1; i >= 0; i--)
		{
			AIO_Throw job = s_aThrows[i];
			IEntity body = null;
			if (job.m_Agent)
				body = job.m_Agent.GetControlledEntity();
			if (!body || !AIO_Server.IsAlive(body))
			{
				s_aThrows.Remove(i);
				continue;
			}
			string what = "smoke";
			if (job.m_eType == EWeaponType.WT_FRAGGRENADE)
				what = "frag";
			int carried = Count(body, job.m_eType);
			// Self-aid comes first (it outranks the throw); his time doesn't run meanwhile.
			SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(job.m_Agent);
			bool healing = utility && utility.HasActionOfType(SCR_AIHealBehavior);
			if (healing && now < job.m_iGiveUp)
			{
				if (job.m_iDeadline < now + 2000)
					job.m_iDeadline = now + 2000;
			}
			else if (healing)
			{
				End(job, body, "is treating his wounds; throw called off");
				s_aThrows.Remove(i);
				continue;
			}
			if (job.m_iStage == 0)
			{
				if (carried == 0)
				{
					End(job, body, string.Format("no longer has a %1 grenade", what));
					s_aThrows.Remove(i);
					continue;
				}
				if (now > job.m_iDeadline)
				{
					End(job, body, "couldn't get close enough to throw");
					s_aThrows.Remove(i);
					continue;
				}
				if (vector.Distance(body.GetOrigin(), job.m_vPoint) > THROW_RANGE)
					continue;
				if (job.m_eType == EWeaponType.WT_FRAGGRENADE)
				{
					float wall = Blocked(body, job.m_vPoint);
					if (wall >= 0 && wall < DANGER_CLOSE)
					{
						End(job, body, string.Format("held his frag: his throw is blocked %1 m in front of him", Math.Round(wall)));
						s_aThrows.Remove(i);
						continue;
					}
					string close = DangerClose(job.m_vPoint, GetGame().GetPlayerManager().GetPlayerControlledEntity(job.m_iPlayerId));
					if (!close.IsEmpty())
					{
						End(job, body, "held his frag. " + close);
						s_aThrows.Remove(i);
						continue;
					}
				}
				Turn(job, body, utility);
				continue;
			}
			if (job.m_iStage == 2)
			{
				if (OffFacing(body, job.m_vPoint) > FACING_DEG && now < job.m_iDeadline)
				{
					utility.m_LookAction.LookAt(job.m_vPoint + Vector(0, 1, 0), TURN_LOOK_PRIORITY, 2.0);
					continue;
				}
				// Smoke off-line does no harm; a frag off-line can land in the squad.
				if (OffFacing(body, job.m_vPoint) > FACING_DEG && job.m_eType == EWeaponType.WT_FRAGGRENADE)
				{
					End(job, body, "couldn't turn to face the spot; throw called off");
					s_aThrows.Remove(i);
					continue;
				}
				// Someone may have stepped in while he turned.
				if (job.m_eType == EWeaponType.WT_FRAGGRENADE)
				{
					string late = DangerClose(job.m_vPoint, GetGame().GetPlayerManager().GetPlayerControlledEntity(job.m_iPlayerId));
					if (!late.IsEmpty())
					{
						End(job, body, "held his frag. " + late);
						s_aThrows.Remove(i);
						continue;
					}
				}
				Throw(job, body, utility);
				continue;
			}
			// The grenade has left his inventory: thrown.
			if (carried < job.m_iCarried)
			{
				s_iThrown++;
				if (job.m_eType == EWeaponType.WT_FRAGGRENADE)
					Track(job, body);
				Print(string.Format("[AIO] GRENADES %1 threw %2 at %3 from %4 m", AIO_Server.NameOf(body), what, job.m_vPoint, Math.Round(vector.Distance(body.GetOrigin(), job.m_vPoint))));
				AIO_Supplies.Report(job.m_iPlayerId, string.Format("%1: %2 out", AIO_Server.NameOf(body), what));
				End(job, body, "");
				s_aThrows.Remove(i);
				continue;
			}
			SCR_AIThrowGrenadeToBehavior behavior = job.m_Behavior;
			bool ended = !behavior || behavior.GetActionState() == EAIActionState.FAILED || behavior.GetActionState() == EAIActionState.COMPLETED;
			if (ended || now > job.m_iDeadline)
			{
				if (behavior && !ended)
					behavior.Fail();
				string why = "couldn't throw from there";
				if (!ended)
					why = "didn't get the throw off";
				End(job, body, why);
				s_aThrows.Remove(i);
			}
		}
	}

	protected static void Track(AIO_Throw job, IEntity body)
	{
		array<IEntity> still = {};
		Count(body, job.m_eType, still);
		foreach (IEntity held : job.m_aHeld)
		{
			if (!held || still.Contains(held))
				continue;
			AIO_Burst burst = new AIO_Burst();
			burst.m_Grenade = held;
			burst.m_vPoint = job.m_vPoint;
			burst.m_vFrom = body.GetOrigin();
			burst.m_vLast = held.GetOrigin();
			burst.m_sWho = AIO_Server.NameOf(body);
			burst.m_iDeadline = System.GetTickCount() + 15000;
			s_aBursts.Insert(burst);
		}
	}

	// Back to his previous orders if he walked up for the throw; otherwise he never left them.
	protected static void End(AIO_Throw job, IEntity body, string problem)
	{
		job.m_bEnding = true;
		if (job.m_bMoved)
		{
			AIO_Tasks.Rejoin(body, GetGame().GetPlayerManager().GetPlayerControlledEntity(job.m_iPlayerId));
			if (job.m_bHadHold)
			{
				if (job.m_Anchor && AIO_Server.IsAlive(job.m_Anchor))
					AIO_Tasks.KeepOffset(body, job.m_Anchor, job.m_vOffset);
				else if (!job.m_Anchor)
					AIO_Tasks.MoveTo(body, job.m_vSpot);
			}
		}
		if (!problem.IsEmpty())
		{
			AIO_Supplies.Report(job.m_iPlayerId, string.Format("%1 %2", AIO_Server.NameOf(body), problem));
			Print(string.Format("[AIO] GRENADES %1 %2", AIO_Server.NameOf(body), problem));
		}
	}
}
