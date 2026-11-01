// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). Licensed under the GNU GPL v3 or later (see LICENSE).

// Supplies > Give me ammo / Give me bandages: the nearest recipient who carries what you need walks
// over and hands it to you: magazines that fit your weapons (up to GIVE_MAGAZINES per magazine
// type) or bandages (up to GIVE_BANDAGES). He keeps one of each for himself. Server only.

class AIO_Delivery
{
	AIAgent m_Agent;
	IEntity m_Receiver;
	bool m_bBandages;
	int m_iPlayerId;
	int m_iDeadline;  // tick count
	vector m_vGoal;
	bool m_bHadHold;
	vector m_vSpot;
	IEntity m_Anchor;
	bool m_bAnchored; // a deleted anchor (player disconnected) reads as null, not dead
	vector m_vOffset;
	bool m_bEnding;
}

// Second half of a hand-over: the item has left the giver's inventory; the receiver takes it,
// or it stays at his feet if he has no room.
class AIO_HandOverCallback : ScriptedInventoryOperationCallback
{
	IEntity m_Item;
	IEntity m_Receiver;

	protected override void OnComplete()
	{
		if (!m_Item || !m_Receiver)
			return;
		SCR_InventoryStorageManagerComponent inventory = AIO_Supplies.InventoryOf(m_Receiver);
		if (inventory && inventory.TryInsertItem(m_Item))
			return;
		m_Item.SetOrigin(m_Receiver.GetOrigin() + "0 0.1 0");
		m_Item.Update();
	}
}

class AIO_Logistics
{
	static const int GIVE_MAGAZINES = 3;
	static const int GIVE_BANDAGES = 2;
	static const float REACH = 2.5;
	static const int WALK_MS = 90000;

	protected static ref array<ref AIO_Delivery> s_aDeliveries = {};
	protected static ref array<ref AIO_HandOverCallback> s_aHandOvers = {}; // keeps callbacks alive

	// Mission end (AIO_Game.c): the jobs belong to the world being torn down.
	static void Cleanup()
	{
		s_aDeliveries.Clear();
		s_aHandOvers.Clear();
	}

	static bool IsOrder(int order)
	{
		return order == AIO_EOrder.GIVE_AMMO || order == AIO_EOrder.GIVE_BANDAGES;
	}

	static string Order(array<IEntity> bodies, IEntity receiver, int order, int playerId)
	{
		if (!receiver || !AIO_Server.IsAlive(receiver))
			return "You need to be alive in the world for that";
		bool bandages = order == AIO_EOrder.GIVE_BANDAGES;
		IEntity giver = null;
		float best = 1000000;
		foreach (IEntity body : bodies)
		{
			ChimeraCharacter character = ChimeraCharacter.Cast(body);
			if (!character || body == receiver || !AIO_Server.IsAlive(body) || character.IsInVehicle() || IsDelivering(AIO_Tasks.AgentOf(body)))
				continue;
			array<IEntity> items = {};
			Spare(body, receiver, bandages, items);
			if (items.IsEmpty())
				continue;
			float distance = vector.Distance(body.GetOrigin(), receiver.GetOrigin());
			if (distance < best)
			{
				best = distance;
				giver = body;
			}
		}
		if (!giver)
		{
			if (bandages)
				return "Nobody has a bandage to spare";
			return "Nobody has spare magazines for your weapons";
		}
		if (best > 300)
			return "The nearest soldier who could help is too far away";
		Start(giver, receiver, bandages, playerId);
		if (bandages)
			return string.Format("%1 bringing you bandages", AIO_Server.NameOf(giver));
		return string.Format("%1 bringing you magazines", AIO_Server.NameOf(giver));
	}

	// What the giver can hand over: fitting magazines (per magazine type at most GIVE_MAGAZINES,
	// keeping one) or bandages (at most GIVE_BANDAGES, keeping one).
	static void Spare(IEntity giver, IEntity receiver, bool bandages, notnull array<IEntity> spare)
	{
		SCR_InventoryStorageManagerComponent inventory = AIO_Supplies.InventoryOf(giver);
		if (!inventory)
			return;
		array<IEntity> items = {};
		inventory.GetItems(items);
		if (bandages)
		{
			array<IEntity> found = {};
			foreach (IEntity item : items)
			{
				if (AIO_Supplies.Kind(item) == AIO_Supplies.KIND_BANDAGE)
					found.Insert(item);
			}
			for (int b = 0; b < found.Count() - 1 && b < GIVE_BANDAGES; b++)
				spare.Insert(found[b]);
			return;
		}
		array<string> wells = {};
		ReceiverWells(receiver, wells);
		foreach (string well : wells)
		{
			array<IEntity> found = {};
			foreach (IEntity item : items)
			{
				MagazineComponent magazine = MagazineComponent.Cast(item.FindComponent(MagazineComponent));
				// Magazines in his weapons are not in his storage list; only carried full or part mags count.
				if (magazine && magazine.GetAmmoCount() > 0 && magazine.GetMagazineWell() && magazine.GetMagazineWell().Type().ToString() == well)
					found.Insert(item);
			}
			for (int m = 0; m < found.Count() - 1 && m < GIVE_MAGAZINES; m++)
				spare.Insert(found[m]);
		}
	}

	// Magazine well type names of the receiver's weapons (compared by name, see AIO_Supplies).
	static void ReceiverWells(IEntity receiver, notnull array<string> wells)
	{
		BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(receiver.FindComponent(BaseWeaponManagerComponent));
		if (!weapons)
			return;
		array<IEntity> weaponEntities = {};
		weapons.GetWeaponsList(weaponEntities);
		array<BaseMuzzleComponent> muzzles = {};
		foreach (IEntity weaponEntity : weaponEntities)
		{
			BaseWeaponComponent weapon = BaseWeaponComponent.Cast(weaponEntity.FindComponent(BaseWeaponComponent));
			if (!weapon)
				continue;
			muzzles.Clear();
			weapon.GetMuzzlesList(muzzles);
			foreach (BaseMuzzleComponent muzzle : muzzles)
			{
				if (!muzzle || !muzzle.GetMagazineWell())
					continue;
				string name = muzzle.GetMagazineWell().Type().ToString();
				if (!wells.Contains(name))
					wells.Insert(name);
			}
		}
	}

	static bool IsDelivering(AIAgent agent)
	{
		foreach (AIO_Delivery delivery : s_aDeliveries)
		{
			if (delivery.m_Agent == agent)
				return true;
		}
		return false;
	}

	static string Label(AIAgent agent)
	{
		if (IsDelivering(agent))
			return "Bringing supplies";
		return "";
	}

	static void Cancel(AIAgent agent)
	{
		for (int i = s_aDeliveries.Count() - 1; i >= 0; i--)
		{
			if (s_aDeliveries[i].m_Agent == agent && !s_aDeliveries[i].m_bEnding)
				s_aDeliveries.Remove(i);
		}
	}

	protected static void Start(IEntity giver, IEntity receiver, bool bandages, int playerId)
	{
		AIAgent agent = AIO_Tasks.AgentOf(giver);
		SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(agent);
		if (!utility)
			return;
		AIO_Delivery delivery = new AIO_Delivery();
		delivery.m_Agent = agent;
		delivery.m_Receiver = receiver;
		delivery.m_bBandages = bandages;
		delivery.m_iPlayerId = playerId;
		delivery.m_iDeadline = System.GetTickCount() + WALK_MS;
		AIO_HoldBehavior hold = AIO_Tasks.ActiveHold(utility);
		if (hold)
		{
			delivery.m_bHadHold = true;
			delivery.m_vSpot = hold.m_vSpot;
			delivery.m_Anchor = hold.m_Anchor;
			delivery.m_bAnchored = hold.m_bAnchored;
			delivery.m_vOffset = hold.m_vOffset;
		}
		AIO_Tasks.Release(giver);
		s_aDeliveries.Insert(delivery);
		Walk(delivery, giver);
		AIO_Tasks.Watch();
	}

	protected static void Walk(AIO_Delivery delivery, IEntity giver)
	{
		SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(delivery.m_Agent);
		if (!utility)
			return;
		delivery.m_bEnding = true; // Release would cancel this delivery
		AIO_Tasks.Release(giver);
		delivery.m_bEnding = false;
		delivery.m_vGoal = delivery.m_Receiver.GetOrigin();
		utility.AddAction(new AIO_MoveBehavior(utility, null, delivery.m_vGoal, radius: 1.5));
		utility.AddAction(new AIO_HoldBehavior(utility, null, delivery.m_vGoal));
	}

	// Called by the AIO_Tasks watchdog every second.
	static void Tick()
	{
		int now = System.GetTickCount();
		if (s_aDeliveries.IsEmpty())
			s_aHandOvers.Clear();
		for (int i = s_aDeliveries.Count() - 1; i >= 0; i--)
		{
			AIO_Delivery delivery = s_aDeliveries[i];
			IEntity giver = null;
			if (delivery.m_Agent)
				giver = delivery.m_Agent.GetControlledEntity();
			if (!giver || !AIO_Server.IsAlive(giver))
			{
				s_aDeliveries.Remove(i);
				continue;
			}
			if (!delivery.m_Receiver || !AIO_Server.IsAlive(delivery.m_Receiver) || now > delivery.m_iDeadline)
			{
				End(delivery, giver, "couldn't reach you");
				s_aDeliveries.Remove(i);
				continue;
			}
			if (vector.Distance(giver.GetOrigin(), delivery.m_Receiver.GetOrigin()) > REACH)
			{
				if (vector.Distance(delivery.m_Receiver.GetOrigin(), delivery.m_vGoal) > 4)
					Walk(delivery, giver); // you moved: follow
				continue;
			}
			array<IEntity> items = {};
			Spare(giver, delivery.m_Receiver, delivery.m_bBandages, items);
			// Straight moves between two living soldiers' storages fail: the giver lets go of the
			// item first, then the receiver takes it (AIO_HandOverCallback).
			SCR_InventoryStorageManagerComponent from = AIO_Supplies.InventoryOf(giver);
			int given;
			foreach (IEntity item : items)
			{
				InventoryItemComponent itemComponent = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
				if (!from || !itemComponent || !itemComponent.GetParentSlot())
					continue;
				AIO_HandOverCallback handOver = new AIO_HandOverCallback();
				handOver.m_Item = item;
				handOver.m_Receiver = delivery.m_Receiver;
				s_aHandOvers.Insert(handOver);
				if (from.TryRemoveItemFromStorage(item, itemComponent.GetParentSlot().GetStorage(), handOver))
					given++;
			}
			string what = "magazines";
			if (delivery.m_bBandages)
				what = "bandages";
			if (given > 0)
				End(delivery, giver, string.Format("gave you %1 %2", given, what));
			else if (items.IsEmpty())
				End(delivery, giver, "has nothing left to give you");
			else
				End(delivery, giver, "couldn't hand anything over");
			s_aDeliveries.Remove(i);
		}
	}

	protected static void End(AIO_Delivery delivery, IEntity giver, string result)
	{
		delivery.m_bEnding = true;
		AIO_Tasks.Rejoin(giver, GetGame().GetPlayerManager().GetPlayerControlledEntity(delivery.m_iPlayerId));
		if (delivery.m_bHadHold)
		{
			if (delivery.m_Anchor && AIO_Server.IsAlive(delivery.m_Anchor) && AIO_Server.StillCommands(delivery.m_Anchor, giver))
				AIO_Tasks.KeepOffset(giver, delivery.m_Anchor, delivery.m_vOffset);
			else if (!delivery.m_bAnchored)
				AIO_Tasks.MoveTo(giver, delivery.m_vSpot);
		}
		AIO_Supplies.Report(delivery.m_iPlayerId, string.Format("%1 %2", AIO_Server.NameOf(giver), result));
		Print(string.Format("[AIO] GIVE %1 %2", AIO_Server.NameOf(giver), result));
	}
}
