// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). Licensed under the GNU GPL v3 or later (see LICENSE).

// Stance & speed > Behaviour: Arma 3's behaviour modes, built from the game's own AI settings.
// Server only. Reforger computes its threat state (SAFE .. THREATENED) itself, so a mode never
// overrides danger: it shapes how the men travel and wait while the game handles the fight.
// - Safe: weapons lowered while the man is quiet; he raises it the moment there is danger.
// - Aware: the game's normal behaviour (clears the mode and its stance/speed settings).
// - Combat: crouched or prone while travelling and waiting, running, weapons kept up.
// - Stealth: crouched or prone, walking, weapons kept up, return fire only.
// A later Stance or Speed order replaces the mode's stance or speed; the posture rule stays.

enum AIO_EBehaviour
{
	AWARE,
	SAFE,
	COMBAT,
	STEALTH
}

class AIO_Behaviour
{
	protected static ref array<AIAgent> s_aAgents = {};
	protected static ref array<int> s_aModes = {};
	protected static ref array<bool> s_aOwnROE = {}; // Stealth set his ROE; leaving Stealth gives it back

	static bool IsOrder(int order)
	{
		return order == AIO_EOrder.BEH_SAFE || order == AIO_EOrder.BEH_AWARE
			|| order == AIO_EOrder.BEH_COMBAT || order == AIO_EOrder.BEH_STEALTH;
	}

	static string Order(array<IEntity> bodies, int order)
	{
		int mode = AIO_EBehaviour.AWARE;
		string what = "back to normal behaviour";
		if (order == AIO_EOrder.BEH_SAFE)
		{
			mode = AIO_EBehaviour.SAFE;
			what = "in safe mode, weapons down while quiet";
		}
		else if (order == AIO_EOrder.BEH_COMBAT)
		{
			mode = AIO_EBehaviour.COMBAT;
			what = "in combat mode, low and ready";
		}
		else if (order == AIO_EOrder.BEH_STEALTH)
		{
			mode = AIO_EBehaviour.STEALTH;
			what = "in stealth mode, low, slow, return fire only";
		}
		int count;
		foreach (IEntity body : bodies)
		{
			if (Set(AIO_Tasks.AgentOf(body), mode))
				count++;
		}
		if (count == 0)
			return "No soldiers under your command";
		return string.Format("%1 men %2", count, what);
	}

	static int ModeOf(AIAgent agent)
	{
		int at = s_aAgents.Find(agent);
		if (at < 0)
			return AIO_EBehaviour.AWARE;
		return s_aModes[at];
	}

	static string Label(AIAgent agent)
	{
		int mode = ModeOf(agent);
		if (mode == AIO_EBehaviour.SAFE)
			return "Safe";
		if (mode == AIO_EBehaviour.COMBAT)
			return "Combat";
		if (mode == AIO_EBehaviour.STEALTH)
			return "Stealth";
		return "";
	}

	// Ordinary travel orders count as travel (optional AI mods may relax it) only outside
	// Combat and Stealth, where the men must stay ready.
	static bool AllowsTravel(IEntity body)
	{
		int mode = ModeOf(AIO_Tasks.AgentOf(body));
		return mode != AIO_EBehaviour.COMBAT && mode != AIO_EBehaviour.STEALTH;
	}

	static bool Set(AIAgent agent, int mode)
	{
		if (!agent)
			return false;
		ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
		if (!character || !AIO_Server.IsAlive(character))
			return false;
		SCR_AICharacterSettingsComponent settings = SCR_AICharacterSettingsComponent.Cast(agent.FindComponent(SCR_AICharacterSettingsComponent));
		if (!settings)
			return false;

		bool ownROE = false;
		int at = s_aAgents.Find(agent);
		if (at >= 0)
		{
			ownROE = s_aOwnROE[at];
			s_aAgents.RemoveOrdered(at);
			s_aModes.RemoveOrdered(at);
			s_aOwnROE.RemoveOrdered(at);
		}
		if (ownROE && mode != AIO_EBehaviour.STEALTH)
			AIO_Maneuvers.SetROE(character, -1);

		// Each mode starts from the man's own stance and pace.
		AIO_Tasks.StopCopying(agent);
		AIO_Tasks.SetStance(agent, -1);
		AIO_Tasks.SetSpeed(agent, -1);

		CharacterControllerComponent controller = character.GetCharacterController();
		if (mode == AIO_EBehaviour.AWARE)
		{
			if (controller && !character.IsInVehicle())
				controller.SetWeaponRaised(true);
			return true;
		}

		if (mode == AIO_EBehaviour.COMBAT || mode == AIO_EBehaviour.STEALTH)
		{
			// Crouch or prone for travel and waiting; Stealth also for the first reactions to
			// gunshots. Real combat behaviours above that still choose their own stance.
			SCR_EAIBehaviorCause cause = SCR_EAIBehaviorCause.GROUP_GOAL;
			if (mode == AIO_EBehaviour.STEALTH)
				cause = SCR_EAIBehaviorCause.DANGER_LOW;
			settings.AddCharacterSetting(SCR_AICharacterStanceSetting_Range.Create(SCR_EAISettingOrigin.COMMANDING, cause, ECharacterStance.PRONE, ECharacterStance.CROUCH), false, true);
			if (mode == AIO_EBehaviour.COMBAT)
				AIO_Tasks.SetSpeed(agent, EMovementType.RUN);
			else
				AIO_Tasks.SetSpeed(agent, EMovementType.WALK);
			// Idle men only re-read the stance setting when a behaviour sets one: crouch now.
			if (controller && !character.IsInVehicle() && controller.GetStance() == ECharacterStance.STAND)
				SCR_AIStanceHandling.SetStance(controller, ECharacterStance.CROUCH);
		}
		if (mode == AIO_EBehaviour.STEALTH)
		{
			AIO_Maneuvers.StopLead(agent);
			AIO_Maneuvers.SetROE(character, EAIGroupCombatMode.RETURN_FIRE);
			ownROE = true;
		}
		else
		{
			ownROE = false;
		}

		s_aAgents.Insert(agent);
		s_aModes.Insert(mode);
		s_aOwnROE.Insert(ownROE);
		ApplyPosture(agent, character, mode);
		AIO_Tasks.Watch();
		return true;
	}

	// Called by the watchdog once a second.
	static void Tick()
	{
		for (int i = s_aAgents.Count() - 1; i >= 0; i--)
		{
			AIAgent agent = s_aAgents[i];
			ChimeraCharacter character = null;
			if (agent)
				character = ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (!character || !AIO_Server.IsAlive(character))
			{
				s_aAgents.RemoveOrdered(i);
				s_aModes.RemoveOrdered(i);
				s_aOwnROE.RemoveOrdered(i);
				continue;
			}
			ApplyPosture(agent, character, s_aModes[i]);
		}
	}

	// Safe lowers the weapon while the man is quiet; Combat and Stealth keep it up.
	protected static void ApplyPosture(AIAgent agent, ChimeraCharacter character, int mode)
	{
		// A grenade throw raises the arm itself; lowering it would cancel the throw.
		if (character.IsInVehicle() || AIO_Grenades.IsThrowing(agent))
			return;
		CharacterControllerComponent controller = character.GetCharacterController();
		if (!controller || controller.IsUnconscious() || controller.IsReloading() || controller.IsChangingItem()
			|| controller.IsGadgetInHands() || controller.GetDisableWeaponControls() || controller.IsClimbing() || controller.IsSwimming())
			return;
		if (mode == AIO_EBehaviour.SAFE)
		{
			bool quiet = Quiet(agent);
			if (quiet == controller.IsWeaponRaised())
				controller.SetWeaponRaised(!quiet);
			return;
		}
		if ((mode == AIO_EBehaviour.COMBAT || mode == AIO_EBehaviour.STEALTH) && !controller.IsWeaponRaised())
			controller.SetWeaponRaised(true);
	}

	// No danger events, no target, nothing seen or heard, and the game's threat state is SAFE.
	static bool Quiet(AIAgent agent)
	{
		SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(agent);
		if (!utility || !utility.m_CombatComponent || !utility.m_ThreatSystem || !utility.m_PerceptionComponent)
			return false;
		if (agent.GetDangerEventsCount() > 0)
			return false;
		if (utility.m_CombatComponent.GetCurrentTarget() || utility.m_CombatComponent.GetRetreatTarget())
			return false;
		if (utility.m_ThreatSystem.GetState() != EAIThreatState.SAFE)
			return false;
		array<BaseTarget> targets = {};
		utility.m_PerceptionComponent.GetTargetsList(targets, ETargetCategory.ENEMY);
		if (!targets.IsEmpty())
			return false;
		utility.m_PerceptionComponent.GetTargetsList(targets, ETargetCategory.DETECTED);
		return targets.IsEmpty();
	}
}
