// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). GPL v3 or later (see LICENSE).

// Let the native Attack behavior aim, select weapons, check friendly fire and shoot
// at a commanded post. Ordinary pursuit must not turn that into an advance on the enemy.
// Native grenade/vehicle avoidance uses separate higher-priority danger behaviors.
modded class SCR_AICombatMoveLogic_Attack
{
	protected override bool MoveToNextPosCondition()
	{
		AIO_HoldBehavior hold;
		if (m_Utility)
			hold = m_Utility.AIO_GetOrderHold();
		if (hold && !hold.m_bAllowPursuit && m_CombatComp && !m_CombatComp.GetRetreatTarget())
			return false;
		return super.MoveToNextPosCondition();
	}
}
