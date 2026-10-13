// All-in-One Command Menu for Arma Reforger
// Based on Leopard20's All-In-One Command Menu for Arma 3 (GPLv3); rebuilt in Enforce Script.
// Copyright (C) 2026 Jacob (propst-design). Licensed under the GNU GPL v3 or later (see LICENSE).

// The menu runs on the local player's controller; orders and roster requests travel
// to the server as RPCs on it.
modded class SCR_PlayerController
{
	protected float m_fAIO_Window = -1000;
	protected int m_iAIO_InWindow;

	override void OnUpdate(float timeSlice)
	{
		super.OnUpdate(timeSlice);
		if (this == GetGame().GetPlayerController())
			AIO_Menu.Get().Update(this);
	}

	// ---------------------------------------------------------------- roster

	void AIO_AskRoster()
	{
		Rpc(AIO_RpcAskRoster);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void AIO_RpcAskRoster()
	{
		array<RplId> ids = {};
		array<string> names = {};
		array<int> health = {};
		array<string> tasks = {};
		array<int> ammo = {};
		AIO_Server.Roster(GetPlayerId(), ids, names, health, tasks, ammo);
		Rpc(AIO_RpcRoster, ids, names, health, tasks, ammo, AIO_Server.AdminAllowed(GetPlayerId()));
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void AIO_RpcRoster(array<RplId> ids, array<string> names, array<int> health, array<string> tasks, array<int> ammo, bool admin)
	{
		AIO_Menu.Get().SetRoster(ids, names, health, tasks, ammo, admin);
	}

	// ---------------------------------------------------------------- orders

	void AIO_SendOrder(AIO_EOrder order, array<RplId> members, vector point, RplId target)
	{
		Rpc(AIO_RpcOrder, order, members, point, target);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void AIO_RpcOrder(int order, array<RplId> members, vector point, RplId target)
	{
		// Flood guard: a handful of orders a second is plenty for a menu.
		float now = GetGame().GetWorld().GetWorldTime();
		if (now - m_fAIO_Window > 1000)
		{
			m_fAIO_Window = now;
			m_iAIO_InWindow = 0;
		}
		m_iAIO_InWindow++;
		if (m_iAIO_InWindow > 10)
			return;
		string feedback = AIO_Server.Execute(this, order, members, point, target);
		if (!feedback.IsEmpty())
			Rpc(AIO_RpcFeedback, feedback);
	}

	// Server: tell this player something (e.g. a man reporting back after an errand).
	void AIO_Notify(string text)
	{
		Rpc(AIO_RpcFeedback, text);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void AIO_RpcFeedback(string text)
	{
		Print("[AIO] " + text);
		AIO_Menu.Get().ShowFeedback(text);
	}
}
