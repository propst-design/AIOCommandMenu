// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). GPL v3 or later (see LICENSE).

// Own subtype: replacing an order cancels only our medical task, not vanilla self aid.
class AIO_TreatBehavior : SCR_AIMedicHealBehavior
{
	void AIO_TreatBehavior(SCR_AIUtilityComponent utility, SCR_AIActivityBase groupActivity, IEntity entityToHeal, bool allowHealMove, float priority = PRIORITY_BEHAVIOR_MEDIC_HEAL, float priorityLevel = PRIORITY_LEVEL_NORMAL) {}
}

class AIO_Treatment
{
	IEntity m_Medic;
	IEntity m_Patient;
	IEntity m_Leader;
	SCR_AIGroup m_Group;
	ref AIO_TreatBehavior m_Action;
	ref AIO_HoldBehavior m_Previous;
	int m_iPlayerId;
	float m_fStarted;
}

class AIO_Support
{
	protected static ref array<ref AIO_Treatment> s_aTreatments = {};

	static void Cleanup()
	{
		s_aTreatments.Clear();
	}

	static string Status(int playerId)
	{
		string text;
		int count;
		foreach (AIO_Treatment job : s_aTreatments)
		{
			if (job.m_iPlayerId != playerId || !job.m_Medic || !job.m_Patient || AIO_Server.Slave(playerId) != job.m_Group) continue;
			count++;
			if (count > 2) continue;
			if (text.IsEmpty()) text = "Treatment: ";
			else text += ", ";
			text += AIO_Server.NameOf(job.m_Medic) + " -> " + AIO_Server.NameOf(job.m_Patient);
		}
		if (count > 2) text += string.Format(" +%1", count - 2);
		return text;
	}

	static bool IsOrder(int order)
	{
		return order == AIO_EOrder.WATCH_DIRECTION || order == AIO_EOrder.WATCH_AUTO || order == AIO_EOrder.TREAT_TARGET || order == AIO_EOrder.TREAT_ME;
	}

	static string Order(SCR_AIGroup group, array<IEntity> men, IEntity leader, int order, vector point, IEntity target, int playerId)
	{
		if (order == AIO_EOrder.TREAT_ME)
			target = leader;
		if (order == AIO_EOrder.TREAT_ME || order == AIO_EOrder.TREAT_TARGET)
			return Treat(group, men, leader, target, playerId);
		int count;
		foreach (IEntity man : men)
		{
			SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(man));
			if (!utility)
				continue;
			AIO_HoldBehavior hold = AIO_Tasks.ActiveHold(utility);
			if (order == AIO_EOrder.WATCH_DIRECTION && !hold)
			{
				AIO_Tasks.HoldHere(man);
				hold = AIO_Tasks.ActiveHold(utility);
			}
			if (hold)
			{
				if (order == AIO_EOrder.WATCH_AUTO)
					hold.m_vLook = vector.Zero;
				else
					hold.m_vLook = point + Vector(0, 1.5, 0);
				count++;
			}
		}
		if (order == AIO_EOrder.WATCH_AUTO)
			return string.Format("%1 men watching at own discretion", count);
		return string.Format("%1 men watching that direction", count);
	}

	static bool Wounded(IEntity patient)
	{
		return patient && AIO_Server.IsAlive(patient) && SCR_AIDamageHandling.IsCharacterWounded(patient);
	}

	static string Treat(SCR_AIGroup group, array<IEntity> men, IEntity leader, IEntity patient, int playerId)
	{
		while (patient && !ChimeraCharacter.Cast(patient) && patient.GetParent())
			patient = patient.GetParent();
		array<IEntity> squad = {};
		AIO_Server.Members(group, squad);
		if (!patient || (!squad.Contains(patient) && patient != leader))
			return "Look at a soldier in your squad, or use Treat me";
		if (!AIO_Server.IsAlive(patient))
			return "That soldier is dead";
		if (!Wounded(patient))
			return "That soldier needs no treatment";
		if (CompartmentAccessComponent.GetVehicleIn(patient))
			return "Disembark the casualty before treatment";
		foreach (AIO_Treatment existing : s_aTreatments)
		{
			if (existing.m_Patient == patient)
				return "A soldier is already treating that casualty";
		}
		IEntity best;
		float nearest = 200;
		foreach (IEntity man : men)
		{
			if (man == patient || !AIO_Server.IsAlive(man) || CompartmentAccessComponent.GetVehicleIn(man))
				continue;
			ChimeraCharacter character = ChimeraCharacter.Cast(man);
			if (!character || character.GetCharacterController().IsUnconscious())
				continue;
			SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(man));
			if (!utility || !utility.m_AIInfo || !utility.m_AIInfo.HasRole(EUnitRole.MEDIC))
				continue;
			bool busy;
			foreach (AIO_Treatment treatment : s_aTreatments)
			{
				if (treatment.m_Medic == man)
					busy = true;
			}
			if (busy)
				continue;
			float distance = vector.Distance(man.GetOrigin(), patient.GetOrigin());
			if (distance < nearest)
			{
				best = man;
				nearest = distance;
			}
		}
		if (!best)
			return "No conscious soldier on foot with medical supplies within 200 m of the casualty";
		AIO_Treatment job = new AIO_Treatment();
		job.m_Medic = best;
		job.m_Patient = patient;
		job.m_Leader = leader;
		job.m_Group = group;
		job.m_iPlayerId = playerId;
		job.m_fStarted = GetGame().GetWorld().GetWorldTime();
		SCR_AIUtilityComponent medicUtility = AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(best));
		job.m_Previous = AIO_Tasks.ActiveHold(medicUtility);
		AIO_Tasks.Release(best);
		job.m_Action = new AIO_TreatBehavior(medicUtility, null, patient, true);
		medicUtility.AddAction(job.m_Action);
		s_aTreatments.Insert(job);
		AIO_Tasks.Watch();
		return string.Format("%1 treating %2", AIO_Server.NameOf(best), AIO_Server.NameOf(patient));
	}

	static void Cancel(IEntity man)
	{
		for (int i = s_aTreatments.Count() - 1; i >= 0; i--)
		{
			AIO_Treatment job = s_aTreatments[i];
			if (job.m_Medic != man)
				continue;
			job.m_Action.SetRemoveAction(true);
			job.m_Action.Fail();
			s_aTreatments.RemoveOrdered(i);
		}
	}

	static void Tick()
	{
		float now = GetGame().GetWorld().GetWorldTime();
		for (int i = s_aTreatments.Count() - 1; i >= 0; i--)
		{
			AIO_Treatment job = s_aTreatments[i];
			array<IEntity> squad = {};
			AIO_Server.Members(job.m_Group, squad);
			if (!squad.Contains(job.m_Medic) || (AIO_Server.IsAlive(job.m_Patient) && job.m_Patient != job.m_Leader && !squad.Contains(job.m_Patient)) || !AIO_Server.IsAlive(job.m_Leader) || (job.m_iPlayerId > 0 && AIO_Server.Slave(job.m_iPlayerId) != job.m_Group))
			{
				Cancel(job.m_Medic);
				continue;
			}
			bool done = !Wounded(job.m_Patient);
			bool ended = job.m_Action.GetActionState() == EAIActionState.FAILED || job.m_Action.GetActionState() == EAIActionState.COMPLETED || job.m_Action.GetRemoveAction();
			if (!done && !ended && now - job.m_fStarted < 180000)
				continue;
			s_aTreatments.RemoveOrdered(i); // remove before restoring, which calls Release
			job.m_Action.SetRemoveAction(true);
			job.m_Action.Fail();
			string result = "Treatment stopped; casualty still needs aid";
			if (!AIO_Server.IsAlive(job.m_Patient))
				result = "Treatment stopped; casualty lost";
			else if (done)
				result = "Treatment complete";
			AIO_Supplies.Report(job.m_iPlayerId, string.Format("%1: %2", AIO_Server.NameOf(job.m_Medic), result));
			if (job.m_Previous && !job.m_Previous.m_aLegs && !job.m_Previous.m_Anchor)
			{
				AIO_Tasks.MoveTo(job.m_Medic, job.m_Previous.m_vSpot);
				AIO_HoldBehavior restored = AIO_Tasks.ActiveHold(AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(job.m_Medic)));
				if (restored)
				{
					restored.m_vLook = job.m_Previous.m_vLook;
					if (job.m_Previous.m_bInterior)
					{
						restored.m_Building = job.m_Previous.m_Building;
						AIO_Tasks.GuardInterior(job.m_Medic, restored, job.m_iPlayerId);
					}
				}
			}
			else
				AIO_Tasks.Rejoin(job.m_Medic, job.m_Leader);
		}
	}
}
