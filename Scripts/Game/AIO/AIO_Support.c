// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). GPL v3 or later (see LICENSE).

class AIO_Support
{
	static bool IsOrder(int order)
	{
		return order == AIO_EOrder.WATCH_DIRECTION || order == AIO_EOrder.WATCH_AUTO;
	}

	static string Order(SCR_AIGroup group, array<IEntity> men, IEntity leader, int order, vector point, IEntity target, int playerId)
	{
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
}
