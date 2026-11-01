// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). GPL v3 or later (see LICENSE).

class AIO_BoundMember
{
	AIAgent m_Agent;
	bool m_bA;
	bool m_bMoving;
	bool m_bReleased;
	vector m_vFinal;
	vector m_vLeg;
	vector m_vProgress;
	float m_fProgressAt;
	ref AIO_HoldBehavior m_Hold;
}

class AIO_Bound
{
	ref array<ref AIO_BoundMember> m_aMembers = {};
	SCR_AIGroup m_Group;
	IEntity m_Leader;
	int m_iPlayerId;
	vector m_vTarget;
	bool m_bPeel;
	bool m_bAMoving = true;
	bool m_bHasLeg;
	bool m_bPaused;
	bool m_bManualPause;
	bool m_bFinished;
	bool m_bFailed;
	int m_iInitialCount;
	int m_iLegsReached;
	float m_fReadySince;
	float m_fLegStarted;
	float m_fLegBudget;
	float m_fLastReport = -100000;
	string m_sPauseReason;
}

class AIO_Bounding
{
	protected static ref array<ref AIO_Bound> s_aJobs = {};
	protected static bool s_bIssuing;
	static const float STEP_M = 20;
	static const float STALL_MS = 45000;

	static bool IsIssuing() { return s_bIssuing; }
	static void Cleanup() { s_aJobs.Clear(); }

	static AIO_Bound ActiveFor(AIAgent agent)
	{
		foreach (AIO_Bound job : s_aJobs)
		{
			foreach (AIO_BoundMember member : job.m_aMembers)
			{
				if (!member.m_bReleased && member.m_Agent == agent)
				return job;
			}
		}
		return null;
	}

	static void Leave(AIAgent agent)
	{
		if (s_bIssuing) return;
		foreach (AIO_Bound job : s_aJobs)
		{
			foreach (AIO_BoundMember member : job.m_aMembers)
			{
				if (member.m_Agent == agent) member.m_bReleased = true;
			}
		}
	}

	static string Label(AIAgent agent)
	{
		AIO_Bound job = ActiveFor(agent);
		if (!job) return "";
		if (job.m_bPaused) return "Bound paused";
		foreach (AIO_BoundMember member : job.m_aMembers)
		{
			if (member.m_Agent == agent && !member.m_bMoving) return "Covering";
		}
		if (job.m_bPeel) return "Peeling back";
		return "Bounding";
	}

	static string Status(int playerId)
	{
		int count;
		string first;
		foreach (AIO_Bound job : s_aJobs)
		{
			if (job.m_iPlayerId != playerId || !job.m_bPaused || job.m_bFinished || AIO_Server.Slave(playerId) != job.m_Group) continue;
			count++;
			if (count != 1) continue;
			string mode = "Bound";
			if (job.m_bPeel) mode = "Peel";
			first = mode + " paused: " + job.m_sPauseReason;
			if (job.m_bManualPause) first += " | Attack > Resume bound / peel";
			else first += " | resumes when ready";
		}
		if (count > 1) first += string.Format(" (+%1 paused maneuvers)", count - 1);
		return first;
	}

	static void Report(AIO_Bound job, string text, bool urgent = false)
	{
		string kind = "Bound";
		if (job.m_bPeel) kind = "Peel";
		string message = kind + ": " + text;
		Print("[AIO] " + message);
		float now = GetGame().GetWorld().GetWorldTime();
		if (urgent || now - job.m_fLastReport >= 10000)
		{
			AIO_Supplies.Report(job.m_iPlayerId, message);
			job.m_fLastReport = now;
		}
	}

	static string Start(array<IEntity> bodies, vector dir, vector target, bool peel, int playerId, IEntity leader, out AIO_Bound started)
	{
		started = null;
		if (bodies.Count() < 2 || dir == vector.Zero) return "Bounding needs at least two men and a destination";
		AIO_Bound job = new AIO_Bound();
		job.m_Leader = leader;
		job.m_iPlayerId = playerId;
		job.m_vTarget = target;
		job.m_bPeel = peel;
		int firstTeam;
		bool byTeam;
		foreach (IEntity body : bodies)
		{
			if (!AIO_Server.IsAlive(body) || !AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(body)) || CompartmentAccessComponent.GetVehicleIn(body))
				return "Bounding needs living soldiers on foot";
			int team = AIO_Maneuvers.TeamOf(body);
			if (team > 0 && (firstTeam == 0 || team < firstTeam)) firstTeam = team;
		}
		foreach (IEntity soldier : bodies)
		{
			if (firstTeam > 0 && AIO_Maneuvers.TeamOf(soldier) != firstTeam) byTeam = true;
		}
		job.m_Group = SCR_AIGroup.Cast(AIO_Tasks.AgentOf(bodies[0]).GetParentGroup());
		vector across = Vector(dir[2], 0, -dir[0]);
		int a, b;
		for (int i = 0; i < bodies.Count(); i++)
		{
			AIO_BoundMember member = new AIO_BoundMember();
			member.m_Agent = AIO_Tasks.AgentOf(bodies[i]);
			member.m_bA = i % 2 == 0;
			if (byTeam) member.m_bA = AIO_Maneuvers.TeamOf(bodies[i]) == firstTeam;
			if (member.m_bA) a++;
			else b++;
			member.m_vFinal = AIO_Maneuvers.Ground(target + across * ((i - (bodies.Count() - 1) * 0.5) * 3));
			if (peel) member.m_vFinal = AIO_Maneuvers.Ground(bodies[i].GetOrigin() + dir * 50);
			job.m_aMembers.Insert(member);
		}
		if (a == 0 || b == 0) return "Bounding needs two elements";
		job.m_iInitialCount = job.m_aMembers.Count();
		foreach (IEntity ordered : bodies) AIO_Tasks.Release(ordered);
		s_bIssuing = true;
		foreach (AIO_BoundMember holder : job.m_aMembers) EstablishPost(job, holder);
		s_bIssuing = false;
		s_aJobs.Insert(job);
		started = job;
		AIO_Tasks.Watch();
		if (peel) return string.Format("%1 men peeling back in two elements", bodies.Count());
		return string.Format("%1 men bounding to position in two elements", bodies.Count());
	}

	protected static void EstablishPost(AIO_Bound job, AIO_BoundMember member)
	{
		IEntity body = member.m_Agent.GetControlledEntity();
		AIO_Tasks.HoldHere(body);
		member.m_Hold = AIO_Tasks.ActiveHold(AIO_Tasks.UtilityOf(member.m_Agent));
		member.m_bMoving = false;
		if (member.m_Hold && !job.m_bPeel) member.m_Hold.m_vLook = job.m_vTarget + Vector(0, 1.5, 0);
	}

	static bool Conscious(IEntity body)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(body);
		return character && AIO_Server.IsAlive(body) && !character.GetCharacterController().IsUnconscious();
	}

	static bool At(IEntity body, vector point)
	{
		return vector.DistanceXZ(body.GetOrigin(), point) <= 4 && Math.AbsFloat(body.GetOrigin()[1] - point[1]) <= 2.5;
	}

	static bool Pinned(AIAgent agent)
	{
		SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(agent);
		return utility && ((utility.m_ThreatSystem && utility.m_ThreatSystem.GetState() == EAIThreatState.THREATENED) || (utility.m_CombatComponent && utility.m_CombatComponent.GetRetreatTarget()));
	}

	// Uses only the soldiers' native perceived targets. A map/ray destination is never
	// turned into a fabricated enemy or an instruction to fire blindly into terrain.
	static bool CoverReady(AIO_Bound job)
	{
		bool contact;
		bool armedCover;
		bool visibleCover;
		foreach (AIO_BoundMember member : job.m_aMembers)
		{
			SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(member.m_Agent);
			BaseTarget target;
			if (utility && utility.m_CombatComponent) target = utility.m_CombatComponent.GetCurrentTarget();
			if (target && target.GetTimeSinceSeen() <= 3) contact = true;
			if (member.m_bA == job.m_bAMoving || member.m_bReleased) continue;
			IEntity body = member.m_Agent.GetControlledEntity();
			if (!Conscious(body) || Pinned(member.m_Agent) || !member.m_Hold || !At(body, member.m_Hold.m_vSpot)) continue;
			BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(body.FindComponent(BaseWeaponManagerComponent));
			BaseWeaponComponent weapon;
			if (weapons) weapon = weapons.GetCurrentWeapon();
			BaseMagazineComponent magazine;
			if (weapon) magazine = weapon.GetCurrentMagazine();
			if (!magazine || magazine.GetAmmoCount() <= 0) continue;
			armedCover = true;
			if (utility && utility.m_CombatComponent && utility.m_CombatComponent.GetCombatMode() != EAIGroupCombatMode.HOLD_FIRE && target && utility.m_CombatComponent.IsTargetVisible(target) && target.GetTraceFraction() >= 0.99)
				visibleCover = true;
		}
		return armedCover && (!contact || visibleCover);
	}

	protected static void Pause(AIO_Bound job, string reason, bool manual)
	{
		if (job.m_bPaused)
		{
			if (manual && !job.m_bManualPause)
			{
				job.m_bManualPause = true;
				job.m_sPauseReason = reason;
				Report(job, "paused: " + reason + "; use Resume bound / peel", true);
			}
			return;
		}
		job.m_bPaused = true;
		job.m_bManualPause = manual;
		job.m_sPauseReason = reason;
		job.m_fReadySince = 0;
		s_bIssuing = true;
		foreach (AIO_BoundMember member : job.m_aMembers) EstablishPost(job, member);
		s_bIssuing = false;
		string suffix = "; waiting for conditions to improve";
		if (manual) suffix = "; use Resume bound / peel";
		Report(job, "paused: " + reason + suffix, manual);
	}

	static string Resume(array<IEntity> bodies, int playerId)
	{
		int count;
		foreach (AIO_Bound job : s_aJobs)
		{
			if (!job.m_bPaused || job.m_iPlayerId != playerId) continue;
			bool selected;
			foreach (AIO_BoundMember member : job.m_aMembers)
			{
				if (member.m_Agent && bodies.Contains(member.m_Agent.GetControlledEntity())) selected = true;
			}
			if (!selected) continue;
			job.m_bManualPause = false;
			job.m_iInitialCount = job.m_aMembers.Count(); // explicit acknowledgement of casualties
			job.m_fReadySince = 0;
			count++;
		}
		return string.Format("%1 maneuvers waiting to resume when ready", count);
	}

	protected static void Dispatch(AIO_Bound job)
	{
		float now = GetGame().GetWorld().GetWorldTime();
		job.m_fLegBudget = 120000;
		s_bIssuing = true;
		foreach (AIO_BoundMember member : job.m_aMembers)
		{
			IEntity body = member.m_Agent.GetControlledEntity();
			member.m_bMoving = member.m_bA == job.m_bAMoving;
			if (!member.m_bMoving) continue;
			if (!job.m_bHasLeg)
			{
				vector difference = member.m_vFinal - body.GetOrigin();
				difference[1] = 0;
				float length = difference.Length();
				float step = Math.Min(STEP_M, length);
				if (job.m_bPeel) step = Math.Min(25, length);
				member.m_vLeg = member.m_vFinal;
				if (length > step) member.m_vLeg = AIO_Maneuvers.Ground(body.GetOrigin() + AIO_Maneuvers.Flat(difference) * step);
			}
			AIO_Tasks.MoveTo(body, member.m_vLeg);
			member.m_Hold = AIO_Tasks.ActiveHold(AIO_Tasks.UtilityOf(member.m_Agent));
			member.m_vProgress = body.GetOrigin();
			member.m_fProgressAt = now;
			job.m_fLegBudget = Math.Max(job.m_fLegBudget, vector.Distance(body.GetOrigin(), member.m_vLeg) * 2000 + 60000);
		}
		s_bIssuing = false;
		job.m_bHasLeg = true;
		job.m_bPaused = false;
		job.m_fLegStarted = now;
		job.m_fReadySince = 0;
	}

	protected static void Finish(AIO_Bound job, string message, bool failed)
	{
		job.m_bFinished = true;
		job.m_bFailed = failed;
		if (failed)
		{
			s_bIssuing = true;
			foreach (AIO_BoundMember member : job.m_aMembers)
			{
				if (Conscious(member.m_Agent.GetControlledEntity())) EstablishPost(job, member);
			}
			s_bIssuing = false;
		}
		Report(job, message, true);
	}

	static void Tick()
	{
		float now = GetGame().GetWorld().GetWorldTime();
		for (int j = s_aJobs.Count() - 1; j >= 0; j--)
		{
			AIO_Bound job = s_aJobs[j];
			bool commanded = job.m_iPlayerId <= 0 || AIO_Server.Slave(job.m_iPlayerId) == job.m_Group;
			bool authorized = (!job.m_Leader || AIO_Server.IsAlive(job.m_Leader)) && commanded;
			int a, b;
			for (int i = job.m_aMembers.Count() - 1; i >= 0; i--)
			{
				AIO_BoundMember member = job.m_aMembers[i];
				IEntity body;
				if (member.m_Agent) body = member.m_Agent.GetControlledEntity();
				SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(member.m_Agent);
				if (member.m_bReleased || !AIO_Server.IsAlive(body) || !utility || member.m_Hold != AIO_Tasks.ActiveHold(utility)
					|| member.m_Agent.GetParentGroup() != job.m_Group) // moved to another group (Game Master)
				{
					job.m_aMembers.RemoveOrdered(i);
					continue;
				}
				if (member.m_bA) a++;
				else b++;
			}
			if (!commanded)
			{
				// He stepped down or left the squad: no posts for a chain of command that never
				// gave them (Routes does the same). The new leader's orders start from formation.
				s_aJobs.RemoveOrdered(j);
				foreach (AIO_BoundMember lost : job.m_aMembers)
					AIO_Tasks.Release(lost.m_Agent.GetControlledEntity());
				job.m_bFinished = true;
				job.m_bFailed = true;
				Report(job, "stopped: you no longer command these men", true);
				continue;
			}
			if (!authorized || a == 0 || b == 0)
			{
				Finish(job, "stopped: command cancelled or an element lost. Holding.", true);
				s_aJobs.RemoveOrdered(j);
				continue;
			}
			bool allFinal = true;
			bool pinned;
			foreach (AIO_BoundMember soldier : job.m_aMembers)
			{
				IEntity entity = soldier.m_Agent.GetControlledEntity();
				if (!Conscious(entity) || !At(entity, soldier.m_vFinal)) allFinal = false;
				if (!Conscious(entity) || Pinned(soldier.m_Agent)) pinned = true;
			}
			if (allFinal)
			{
				Finish(job, "destination reached. Holding.", false);
				s_aJobs.RemoveOrdered(j);
				continue;
			}
			if (job.m_aMembers.Count() * 100 < job.m_iInitialCount * 60 && !job.m_bManualPause)
				Pause(job, "heavy losses", true);
			if (pinned && !job.m_bPaused) Pause(job, "pinned or a man needs aid", false);
			if (job.m_bPaused && (job.m_bManualPause || pinned)) continue;
			if (!job.m_bHasLeg || job.m_bPaused)
			{
				if (!CoverReady(job))
				{
					job.m_fReadySince = 0;
					if (!job.m_bPaused) Pause(job, "covering element not ready", false);
					continue;
				}
				if (job.m_fReadySince == 0) job.m_fReadySince = now;
				if (now - job.m_fReadySince < 2000) continue;
				bool resuming = job.m_bPaused;
				Dispatch(job);
				if (resuming) Report(job, "resuming");
				continue;
			}
			bool reached = true;
			bool stalled;
			foreach (AIO_BoundMember mover : job.m_aMembers)
			{
				if (!mover.m_bMoving) continue;
				IEntity moving = mover.m_Agent.GetControlledEntity();
				if (At(moving, mover.m_vLeg)) continue;
				reached = false;
				if (vector.Distance(moving.GetOrigin(), mover.m_vProgress) > 1.5)
				{
					mover.m_vProgress = moving.GetOrigin();
					mover.m_fProgressAt = now;
				}
				if (now - mover.m_fProgressAt > STALL_MS) stalled = true;
			}
			if (stalled || now - job.m_fLegStarted > job.m_fLegBudget)
			{
				Pause(job, "movement blocked", true);
				continue;
			}
			if (!CoverReady(job))
			{
				Pause(job, "covering element not ready", false);
				continue;
			}
			if (!reached) continue;
			job.m_iLegsReached++;
			job.m_bAMoving = !job.m_bAMoving;
			job.m_bHasLeg = false;
			job.m_fReadySince = 0;
			s_bIssuing = true;
			foreach (AIO_BoundMember settled : job.m_aMembers) EstablishPost(job, settled);
			s_bIssuing = false;
		}
	}
}
