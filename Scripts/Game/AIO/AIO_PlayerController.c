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
		Rpc(AIO_RpcRoster, ids, names, health, tasks, ammo, AIO_Server.AdminAllowed(GetPlayerId()), AIO_Server.OrderStatus(GetPlayerId()));
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void AIO_RpcRoster(array<RplId> ids, array<string> names, array<int> health, array<string> tasks, array<int> ammo, bool admin, string orderStatus)
	{
		AIO_Menu.Get().SetRoster(ids, names, health, tasks, ammo, admin);
		AIO_Menu.Get().SetOrderStatus(orderStatus);
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

	void AIO_SendTeam(array<RplId> ids, int team)
	{
		Rpc(AIO_RpcTeam, ids, team);
	}

	void AIO_SendRoute(array<RplId> ids, array<vector> points, bool patrol)
	{
		Rpc(AIO_RpcRoute, ids, points, patrol);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void AIO_RpcRoute(array<RplId> ids, array<vector> points, bool patrol)
	{
		float now = GetGame().GetWorld().GetWorldTime();
		if (now - m_fAIO_Window > 1000)
		{
			m_fAIO_Window = now;
			m_iAIO_InWindow = 0;
		}
		m_iAIO_InWindow++;
		if (m_iAIO_InWindow > 10)
			return;
		SCR_AIGroup group = AIO_Server.Slave(GetPlayerId());
		array<IEntity> men = {};
		if (group && ids && ids.Count() <= 64)
			AIO_Tasks.Resolve(group, ids, men);
		if (!ids || ids.IsEmpty() || men.Count() != ids.Count())
		{
			AIO_Notify("Route members are no longer in your squad; clear the draft and select again");
			return;
		}
		AIO_Route started;
		AIO_Notify(AIO_Routes.Start(group, GetControlledEntity(), men, points, patrol, GetPlayerId(), started));
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void AIO_RpcTeam(array<RplId> ids, int team)
	{
		SCR_AIGroup slave = AIO_Server.Slave(GetPlayerId());
		if (!slave)
			return;
		array<IEntity> bodies = {};
		AIO_Tasks.Resolve(slave, ids, bodies);
		foreach (IEntity body : bodies)
			AIO_Maneuvers.SetTeam(body, team);
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
