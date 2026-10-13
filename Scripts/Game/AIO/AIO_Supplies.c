// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). Licensed under the GNU GPL v3 or later (see LICENSE).

// Supplies: Rearm (nearest source), Rearm from what you look at, Take that weapon. Server only.
//
// A man walks to the source (a body, loose items on the ground, a vehicle's or crate's storage, or an
// arsenal) and takes what he lacks: magazines for each of his weapons up to a normal load, two frag
// grenades if he has none, two bandages if he has none. Arsenals charge supplies like the game's own
// AI does when the arsenal uses supplies. Afterwards he goes back to what he was doing.

// What one man still lacks.
class AIO_Need
{
	typename m_tWell;          // magazine well of a muzzle (null for grenades/bandages)
	int m_iMissing;            // how many to take
	ResourceName m_sPrefab;    // a magazine prefab he already uses (for arsenals)
	int m_iKind;               // AIO_Supplies.KIND_*
}

// A man on his way to a source.
class AIO_Fetch
{
	AIAgent m_Agent;
	IEntity m_Source;
	IEntity m_Weapon;          // Take weapon: the weapon to equip (null for plain rearm)
	int m_iPlayerId;           // who ordered it (for the report)
	int m_iDeadline;           // tick count
	// what he was doing before (restored afterwards)
	bool m_bHadHold;
	vector m_vSpot;
	IEntity m_Anchor;
	vector m_vOffset;
	bool m_bEnding; // being finished: Cancel leaves it alone (it's removed right after)
}

class AIO_Supplies
{
	static const int KIND_MAGAZINE = 0;
	static const int KIND_GRENADE = 1;
	static const int KIND_BANDAGE = 2;
	static const float SEARCH_RADIUS = 50;
	static const float REACH = 2.5;
	static const int FETCH_MS = 90000;

	protected static ref array<ref AIO_Fetch> s_aFetches = {};

	// ---------------------------------------------------------------- what he lacks

	static int Target(BaseWeaponComponent weapon, BaseMuzzleComponent muzzle)
	{
		EWeaponType type = weapon.GetWeaponType();
		if (type == EWeaponType.WT_ROCKETLAUNCHER)
			return 2;
		if (type == EWeaponType.WT_HANDGUN)
			return 2;
		if (type == EWeaponType.WT_FRAGGRENADE || type == EWeaponType.WT_SMOKEGRENADE)
			return 0;
		return 6; // rifles, machine guns, sniper rifles, grenade launchers (per muzzle)
	}

	static SCR_InventoryStorageManagerComponent InventoryOf(IEntity body)
	{
		if (!body)
			return null;
		return SCR_InventoryStorageManagerComponent.Cast(body.FindComponent(SCR_InventoryStorageManagerComponent));
	}

	static void Needs(IEntity body, notnull array<ref AIO_Need> needs)
	{
		SCR_InventoryStorageManagerComponent inventory = InventoryOf(body);
		BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(body.FindComponent(BaseWeaponManagerComponent));
		if (!inventory || !weapons)
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
				int target = Target(weapon, muzzle);
				if (target <= 0)
					continue;
				int have = inventory.GetMagazineCountByMuzzle(body, muzzle);
				if (have >= target)
					continue;
				AIO_Need need = new AIO_Need();
				need.m_iKind = KIND_MAGAZINE;
				need.m_tWell = muzzle.GetMagazineWell().Type();
				need.m_iMissing = target - have;
				need.m_sPrefab = KnownMagazine(body, muzzle);
				needs.Insert(need);
			}
		}
		if (inventory.GetGrenadesCount() == 0)
		{
			AIO_Need grenades = new AIO_Need();
			grenades.m_iKind = KIND_GRENADE;
			grenades.m_iMissing = 2;
			needs.Insert(grenades);
		}
		if (CountBandages(inventory) == 0)
		{
			AIO_Need bandages = new AIO_Need();
			bandages.m_iKind = KIND_BANDAGE;
			bandages.m_iMissing = 2;
			needs.Insert(bandages);
		}
	}

	// Prefab of a magazine he already uses for this muzzle (loaded or carried), "" if none.
	static ResourceName KnownMagazine(IEntity body, BaseMuzzleComponent muzzle)
	{
		BaseMagazineComponent loaded = muzzle.GetMagazine();
		if (loaded && loaded.GetOwner() && loaded.GetOwner().GetPrefabData())
			return loaded.GetOwner().GetPrefabData().GetPrefabName();
		SCR_InventoryStorageManagerComponent inventory = InventoryOf(body);
		array<IEntity> items = {};
		inventory.GetItems(items);
		typename well = muzzle.GetMagazineWell().Type();
		foreach (IEntity item : items)
		{
			MagazineComponent magazine = MagazineComponent.Cast(item.FindComponent(MagazineComponent));
			if (magazine && magazine.GetMagazineWell() && magazine.GetMagazineWell().Type().ToString() == well.ToString() && item.GetPrefabData())
				return item.GetPrefabData().GetPrefabName();
		}
		return "";
	}

	static int CountBandages(SCR_InventoryStorageManagerComponent inventory)
	{
		array<IEntity> items = {};
		inventory.GetItems(items);
		int n;
		foreach (IEntity item : items)
		{
			if (Kind(item) == KIND_BANDAGE)
				n++;
		}
		return n;
	}

	// Kind of an item, or -1. For magazines see MatchesNeed.
	static int Kind(IEntity item)
	{
		if (!item)
			return -1;
		if (item.FindComponent(MagazineComponent))
			return KIND_MAGAZINE;
		BaseWeaponComponent weapon = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent));
		if (weapon && weapon.GetWeaponType() == EWeaponType.WT_FRAGGRENADE)
			return KIND_GRENADE;
		SCR_ConsumableItemComponent consumable = SCR_ConsumableItemComponent.Cast(item.FindComponent(SCR_ConsumableItemComponent));
		if (consumable && consumable.GetConsumableType() == SCR_EConsumableType.BANDAGE)
			return KIND_BANDAGE;
		return -1;
	}

	static bool MatchesNeed(IEntity item, AIO_Need need)
	{
		int kind = Kind(item);
		if (kind != need.m_iKind)
			return false;
		if (kind != KIND_MAGAZINE)
			return true;
		MagazineComponent magazine = MagazineComponent.Cast(item.FindComponent(MagazineComponent));
		if (!magazine || !magazine.GetMagazineWell())
			return false;
		// Compare well types by name (typename inequality isn't reliable in Enforce).
		if (magazine.GetMagazineWell().Type().ToString() != need.m_tWell.ToString())
			return false;
		return magazine.GetAmmoCount() > 0;
	}

	// ---------------------------------------------------------------- sources

	static bool IsArsenal(IEntity entity)
	{
		return entity != null && entity.FindComponent(SCR_ArsenalInventoryStorageManagerComponent) != null;
	}

	static bool IsDeadCharacter(IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character || !character.GetCharacterController())
			return false;
		return character.GetCharacterController().IsDead();
	}

	// Loose item lying on the ground (not in anyone's inventory).
	static bool IsLooseItem(IEntity entity)
	{
		if (!entity || ChimeraCharacter.Cast(entity))
			return false; // characters have an inventory item component too
		InventoryItemComponent item = InventoryItemComponent.Cast(entity.FindComponent(InventoryItemComponent));
		if (!item)
			return false;
		InventoryStorageSlot slot = item.GetParentSlot();
		return !slot || !slot.GetStorage();
	}

	// Items a source offers (not for arsenals).
	static void SourceItems(IEntity source, notnull array<IEntity> items)
	{
		if (IsLooseItem(source))
		{
			items.Insert(source);
			return;
		}
		InventoryStorageManagerComponent storage = InventoryStorageManagerComponent.Cast(source.FindComponent(InventoryStorageManagerComponent));
		if (storage)
			storage.GetItems(items);
	}

	// How useful the source is for these needs (0 = useless).
	static int Usefulness(IEntity source, IEntity taker, array<ref AIO_Need> needs)
	{
		if (!source || source == taker)
			return 0;
		if (IsArsenal(source))
		{
			SCR_ArsenalInventoryStorageManagerComponent arsenal = SCR_ArsenalInventoryStorageManagerComponent.Cast(source.FindComponent(SCR_ArsenalInventoryStorageManagerComponent));
			int offered;
			foreach (AIO_Need wanted : needs)
			{
				if (wanted.m_iKind == KIND_MAGAZINE && !wanted.m_sPrefab.IsEmpty() && arsenal.IsPrefabInArsenalStorage(wanted.m_sPrefab))
					offered += wanted.m_iMissing;
			}
			return offered;
		}
		ChimeraCharacter character = ChimeraCharacter.Cast(source);
		if (character && !IsDeadCharacter(source))
			return 0; // never rob the living
		array<IEntity> items = {};
		SourceItems(source, items);
		int useful;
		foreach (IEntity item : items)
		{
			foreach (AIO_Need need : needs)
			{
				if (MatchesNeed(item, need))
				{
					useful++;
					break;
				}
			}
		}
		return useful;
	}

	protected static ref array<IEntity> s_aQueryHits;

	protected static bool QueryHit(IEntity entity)
	{
		s_aQueryHits.Insert(entity);
		return true;
	}

	// Nearest useful source within SEARCH_RADIUS, or null.
	static IEntity FindSource(IEntity body, array<ref AIO_Need> needs)
	{
		s_aQueryHits = {};
		GetGame().GetWorld().QueryEntitiesBySphere(body.GetOrigin(), SEARCH_RADIUS, QueryHit, null, EQueryEntitiesFlags.ALL);
		IEntity best = null;
		float bestDistance = float.MAX;
		foreach (IEntity hit : s_aQueryHits)
		{
			if (!hit)
				continue;
			if (!IsArsenal(hit) && !IsDeadCharacter(hit) && !IsLooseItem(hit) && !hit.FindComponent(InventoryStorageManagerComponent))
				continue;
			if (Usefulness(hit, body, needs) <= 0)
				continue;
			float distance = vector.Distance(hit.GetOrigin(), body.GetOrigin());
			if (distance < bestDistance)
			{
				best = hit;
				bestDistance = distance;
			}
		}
		s_aQueryHits = null;
		return best;
	}

	// ---------------------------------------------------------------- taking

	// Takes what the man lacks from the source. Returns a short description ("" = nothing taken).
	static string TakeFrom(IEntity body, IEntity source)
	{
		SCR_InventoryStorageManagerComponent inventory = InventoryOf(body);
		if (!inventory || !source)
			return "";
		array<ref AIO_Need> needs = {};
		Needs(body, needs);
		int magazines, grenades, bandages;
		if (IsArsenal(source))
		{
			foreach (AIO_Need need : needs)
			{
				if (need.m_iKind == KIND_MAGAZINE && !need.m_sPrefab.IsEmpty())
					magazines += TakeFromArsenal(inventory, source, need.m_sPrefab, need.m_iMissing);
			}
		}
		else
		{
			array<IEntity> items = {};
			SourceItems(source, items);
			foreach (AIO_Need want : needs)
			{
				foreach (IEntity item : items)
				{
					if (want.m_iMissing <= 0)
						break;
					if (!item || !MatchesNeed(item, want))
						continue;
					if (!Move(inventory, item))
						continue;
					want.m_iMissing--;
					if (want.m_iKind == KIND_MAGAZINE)
						magazines++;
					else if (want.m_iKind == KIND_GRENADE)
						grenades++;
					else
						bandages++;
				}
			}
		}
		string taken = "";
		if (magazines > 0)
			taken = string.Format("%1 magazines", magazines);
		if (grenades > 0)
		{
			if (!taken.IsEmpty())
				taken += ", ";
			taken += string.Format("%1 grenades", grenades);
		}
		if (bandages > 0)
		{
			if (!taken.IsEmpty())
				taken += ", ";
			taken += string.Format("%1 bandages", bandages);
		}
		return taken;
	}

	// Moves an item (in another inventory, or on the ground) into the man's inventory.
	static bool Move(SCR_InventoryStorageManagerComponent inventory, IEntity item)
	{
		InventoryItemComponent itemComponent = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
		InventoryStorageSlot slot = null;
		if (itemComponent)
			slot = itemComponent.GetParentSlot();
		if (!slot || !slot.GetStorage())
			return inventory.TryInsertItem(item, EStoragePurpose.PURPOSE_DEPOSIT);
		BaseInventoryStorageComponent target = inventory.FindStorageForItem(item, EStoragePurpose.PURPOSE_DEPOSIT);
		if (!target)
			return false;
		return inventory.TryMoveItemToStorage(item, target);
	}

	// Spawns magazines from an arsenal into the man's inventory; pays supplies when the arsenal uses them.
	static int TakeFromArsenal(SCR_InventoryStorageManagerComponent inventory, IEntity arsenalEntity, ResourceName prefab, int count)
	{
		SCR_ArsenalInventoryStorageManagerComponent arsenal = SCR_ArsenalInventoryStorageManagerComponent.Cast(arsenalEntity.FindComponent(SCR_ArsenalInventoryStorageManagerComponent));
		if (!arsenal || !arsenal.IsPrefabInArsenalStorage(prefab))
			return 0;
		float cost = 0;
		SCR_ResourceConsumer consumer = null;
		SCR_ArsenalComponent arsenalComponent = SCR_ArsenalComponent.FindArsenalComponent(arsenalEntity);
		SCR_EntityCatalogManagerComponent catalogs = SCR_EntityCatalogManagerComponent.GetInstance();
		if (catalogs && (!arsenalComponent || arsenalComponent.IsArsenalUsingSupplies()))
		{
			SCR_Faction faction = null;
			if (arsenalComponent)
				faction = arsenalComponent.GetAssignedFaction();
			SCR_EntityCatalogEntry entry;
			if (faction)
				entry = catalogs.GetEntryWithPrefabFromFactionCatalog(EEntityCatalogType.ITEM, prefab, faction);
			else
				entry = catalogs.GetEntryWithPrefabFromCatalog(EEntityCatalogType.ITEM, prefab);
			if (entry)
			{
				SCR_ArsenalItem data = SCR_ArsenalItem.Cast(entry.GetEntityDataOfType(SCR_ArsenalItem));
				if (data)
				{
					if (arsenalComponent)
						cost = data.GetSupplyCost(arsenalComponent.GetSupplyCostType());
					else
						cost = data.GetSupplyCost(SCR_EArsenalSupplyCostType.DEFAULT);
				}
			}
			if (cost > 0)
			{
				SCR_ResourceComponent resources = SCR_ResourceComponent.Cast(arsenalEntity.FindComponent(SCR_ResourceComponent));
				if (!resources)
					return 0;
				consumer = resources.GetConsumer(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES);
				if (!consumer)
					return 0;
				cost *= consumer.GetBuyMultiplier();
			}
		}
		int taken;
		while (taken < count)
		{
			if (consumer)
			{
				SCR_ResourceConsumtionResponse response = consumer.RequestAvailability(cost);
				if (response.GetReason() != EResourceReason.SUFFICIENT)
					break;
			}
			if (!inventory.TrySpawnPrefabToStorage(prefab, purpose: EStoragePurpose.PURPOSE_DEPOSIT))
				break;
			if (consumer)
				consumer.RequestConsumtion(cost);
			taken++;
		}
		return taken;
	}

	// ---------------------------------------------------------------- orders

	// Rearm: source = null finds the nearest. Returns feedback.
	static string Rearm(array<IEntity> bodies, IEntity source, int playerId)
	{
		int going, full, nothing;
		foreach (IEntity body : bodies)
		{
			array<ref AIO_Need> needs = {};
			Needs(body, needs);
			if (needs.IsEmpty())
			{
				full++;
				continue;
			}
			IEntity from = source;
			if (!from)
				from = FindSource(body, needs);
			else if (Usefulness(from, body, needs) <= 0)
				from = null;
			if (!from)
			{
				nothing++;
				continue;
			}
			if (Start(body, from, null, playerId))
				going++;
		}
		string reply = string.Format("%1 men going to rearm", going);
		if (full > 0)
			reply += string.Format(", %1 already full", full);
		if (nothing > 0)
		{
			if (source)
				reply += string.Format(", %1 can't use anything there", nothing);
			else
				reply += string.Format(", %1 found nothing within %2 m", nothing, SEARCH_RADIUS);
		}
		return reply;
	}

	// The weapon the player means: the entity he looked at, or the main weapon of a body there.
	static IEntity WeaponAt(IEntity target)
	{
		if (!target)
			return null;
		if (target.FindComponent(BaseWeaponComponent) && IsLooseItem(target))
		{
			BaseWeaponComponent loose = BaseWeaponComponent.Cast(target.FindComponent(BaseWeaponComponent));
			EWeaponType looseType = loose.GetWeaponType();
			if (looseType != EWeaponType.WT_FRAGGRENADE && looseType != EWeaponType.WT_SMOKEGRENADE)
				return target;
		}
		if (!IsDeadCharacter(target))
			return null;
		// Soldiers drop their weapon when they fall: the best weapon lying within 3 m of the body.
		IEntity dropped = BestLooseWeaponNear(target.GetOrigin(), 3);
		if (dropped)
			return dropped;
		BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(target.FindComponent(BaseWeaponManagerComponent));
		if (!weapons)
			return null;
		array<IEntity> list = {};
		weapons.GetWeaponsList(list);
		IEntity best = null;
		int bestRank = 0;
		foreach (IEntity weaponEntity : list)
		{
			BaseWeaponComponent weapon = BaseWeaponComponent.Cast(weaponEntity.FindComponent(BaseWeaponComponent));
			if (!weapon)
				continue;
			int rank = 0;
			EWeaponType type = weapon.GetWeaponType();
			if (type == EWeaponType.WT_ROCKETLAUNCHER)
				rank = 4;
			else if (type == EWeaponType.WT_MACHINEGUN || type == EWeaponType.WT_SNIPERRIFLE)
				rank = 3;
			else if (type == EWeaponType.WT_RIFLE || type == EWeaponType.WT_GRENADELAUNCHER)
				rank = 2;
			else if (type == EWeaponType.WT_HANDGUN)
				rank = 1;
			if (rank > bestRank)
			{
				best = weaponEntity;
				bestRank = rank;
			}
		}
		return best;
	}

	protected static ref array<IEntity> s_aWeaponHits;

	protected static bool WeaponHit(IEntity entity)
	{
		if (entity.FindComponent(BaseWeaponComponent) && IsLooseItem(entity))
			s_aWeaponHits.Insert(entity);
		return true;
	}

	static int WeaponRank(IEntity weaponEntity)
	{
		BaseWeaponComponent weapon = BaseWeaponComponent.Cast(weaponEntity.FindComponent(BaseWeaponComponent));
		if (!weapon)
			return 0;
		EWeaponType type = weapon.GetWeaponType();
		if (type == EWeaponType.WT_ROCKETLAUNCHER)
			return 4;
		if (type == EWeaponType.WT_MACHINEGUN || type == EWeaponType.WT_SNIPERRIFLE)
			return 3;
		if (type == EWeaponType.WT_RIFLE || type == EWeaponType.WT_GRENADELAUNCHER)
			return 2;
		if (type == EWeaponType.WT_HANDGUN)
			return 1;
		return 0;
	}

	static IEntity BestLooseWeaponNear(vector center, float radius)
	{
		s_aWeaponHits = {};
		GetGame().GetWorld().QueryEntitiesBySphere(center, radius, WeaponHit, null, EQueryEntitiesFlags.ALL);
		IEntity best = null;
		int bestRank = 0;
		foreach (IEntity hit : s_aWeaponHits)
		{
			int rank = WeaponRank(hit);
			if (rank > bestRank)
			{
				best = hit;
				bestRank = rank;
			}
		}
		s_aWeaponHits = null;
		return best;
	}

	// Take weapon: one man (the first given) walks over, equips it, takes ammo for it there.
	static string TakeWeapon(array<IEntity> bodies, IEntity target, IEntity looked, int playerId)
	{
		IEntity weapon = null;
		// He looked straight at a weapon (on the ground or on a body): that one.
		if (looked && looked != target && IsDeadCharacter(target))
		{
			BaseWeaponComponent lookedWeapon = BaseWeaponComponent.Cast(looked.FindComponent(BaseWeaponComponent));
			if (lookedWeapon && lookedWeapon.GetWeaponType() != EWeaponType.WT_FRAGGRENADE && lookedWeapon.GetWeaponType() != EWeaponType.WT_SMOKEGRENADE)
				weapon = looked;
		}
		if (!weapon)
			weapon = WeaponAt(target);
		if (!weapon)
			return "Look at a weapon on the ground or a body";
		if (bodies.IsEmpty())
			return "No soldiers under your command";
		// The nearest of the given men goes.
		IEntity taker = null;
		float nearest = float.MAX;
		foreach (IEntity body : bodies)
		{
			float distance = vector.Distance(body.GetOrigin(), weapon.GetOrigin());
			if (distance < nearest)
			{
				taker = body;
				nearest = distance;
			}
		}
		IEntity source = target;
		if (IsLooseItem(weapon))
			source = weapon;
		if (!Start(taker, source, weapon, playerId))
			return "He can't go there";
		return string.Format("%1 going to take the weapon (%2 m)", AIO_Server.NameOf(taker), Math.Round(nearest));
	}

	protected static bool Start(IEntity body, IEntity source, IEntity weapon, int playerId)
	{
		AIAgent agent = AIO_Tasks.AgentOf(body);
		SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(agent);
		if (!utility)
			return false;
		Cancel(agent);
		AIO_Fetch fetch = new AIO_Fetch();
		fetch.m_Agent = agent;
		fetch.m_Source = source;
		fetch.m_Weapon = weapon;
		fetch.m_iPlayerId = playerId;
		fetch.m_iDeadline = System.GetTickCount() + FETCH_MS;
		// Remember his hold (Move-menu station, held spot) to go back to it afterwards.
		AIO_HoldBehavior hold = AIO_Tasks.ActiveHold(utility);
		if (hold)
		{
			fetch.m_bHadHold = true;
			fetch.m_vSpot = hold.m_vSpot;
			fetch.m_Anchor = hold.m_Anchor;
			fetch.m_vOffset = hold.m_vOffset;
		}
		AIO_Tasks.Release(body);
		vector spot = source.GetOrigin();
		utility.AddAction(new AIO_MoveBehavior(utility, null, spot, radius: 1.0));
		utility.AddAction(new AIO_HoldBehavior(utility, null, spot));
		s_aFetches.Insert(fetch);
		AIO_Tasks.Watch();
		return true;
	}

	static bool IsFetching(AIAgent agent)
	{
		foreach (AIO_Fetch fetch : s_aFetches)
		{
			if (fetch.m_Agent == agent)
				return true;
		}
		return false;
	}

	// A new order for the man ends his errand.
	static void Cancel(AIAgent agent)
	{
		for (int i = s_aFetches.Count() - 1; i >= 0; i--)
		{
			if (s_aFetches[i].m_Agent == agent && !s_aFetches[i].m_bEnding)
				s_aFetches.Remove(i);
		}
	}

	// Called by the AIO_Tasks watchdog every second.
	static void Tick()
	{
		int now = System.GetTickCount();
		for (int i = s_aFetches.Count() - 1; i >= 0; i--)
		{
			AIO_Fetch fetch = s_aFetches[i];
			IEntity body = null;
			if (fetch.m_Agent)
				body = fetch.m_Agent.GetControlledEntity();
			if (!body || !AIO_Server.IsAlive(body))
			{
				s_aFetches.Remove(i);
				continue;
			}
			if (!fetch.m_Source || now > fetch.m_iDeadline)
			{
				fetch.m_bEnding = true;
				Finish(fetch, body, "couldn't reach it");
				s_aFetches.Remove(i);
				continue;
			}
			if (vector.DistanceXZ(body.GetOrigin(), fetch.m_Source.GetOrigin()) > REACH)
				continue;
			fetch.m_bEnding = true; // Finish releases the man, which would cancel this errand
			string result = "";
			if (fetch.m_Weapon)
			{
				SCR_InventoryStorageManagerComponent inventory = InventoryOf(body);
				if (inventory && inventory.EquipWeapon(fetch.m_Weapon, null, true))
					result = "took the weapon";
				else
					result = "couldn't take the weapon";
			}
			// Ammo for whatever he carries now (the new weapon only after its equip completes,
			// so a second pass runs a moment later).
			string taken = TakeFrom(body, fetch.m_Source);
			if (!taken.IsEmpty())
			{
				if (!result.IsEmpty())
					result += ", ";
				result += "took " + taken;
			}
			if (fetch.m_Weapon)
				GetGame().GetCallqueue().CallLater(SecondPass, 1500, false, body, fetch.m_Source, fetch.m_iPlayerId);
			if (result.IsEmpty())
				result = "found nothing he could use";
			Finish(fetch, body, result);
			s_aFetches.Remove(i);
		}
	}

	protected static void SecondPass(IEntity body, IEntity source, int playerId)
	{
		if (!body || !source || !AIO_Server.IsAlive(body))
			return;
		string taken = TakeFrom(body, source);
		if (!taken.IsEmpty())
			Report(playerId, string.Format("%1 took %2 for his new weapon", AIO_Server.NameOf(body), taken));
	}

	protected static void Finish(AIO_Fetch fetch, IEntity body, string result)
	{
		AIO_Tasks.Release(body);
		if (fetch.m_bHadHold)
		{
			if (fetch.m_Anchor && AIO_Server.IsAlive(fetch.m_Anchor))
				AIO_Tasks.KeepOffset(body, fetch.m_Anchor, fetch.m_vOffset);
			else if (!fetch.m_Anchor)
				AIO_Tasks.MoveTo(body, fetch.m_vSpot);
		}
		Report(fetch.m_iPlayerId, string.Format("%1 %2", AIO_Server.NameOf(body), result));
		Print(string.Format("[AIO] SUPPLY %1 %2", AIO_Server.NameOf(body), result));
	}

	static void Report(int playerId, string text)
	{
		if (playerId <= 0)
			return;
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (pc)
			pc.AIO_Notify(text);
	}

	// ---------------------------------------------------------------- squad bar

	// Magazines for his current weapon (or first weapon), -1 if he has none.
	static int MagazineCount(IEntity body)
	{
		SCR_InventoryStorageManagerComponent inventory = InventoryOf(body);
		BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(body.FindComponent(BaseWeaponManagerComponent));
		if (!inventory || !weapons)
			return -1;
		BaseWeaponComponent weapon = weapons.GetCurrentWeapon();
		if (!weapon || weapon.GetWeaponType() == EWeaponType.WT_FRAGGRENADE || weapon.GetWeaponType() == EWeaponType.WT_SMOKEGRENADE)
		{
			array<IEntity> list = {};
			weapons.GetWeaponsList(list);
			weapon = null;
			foreach (IEntity entity : list)
			{
				BaseWeaponComponent candidate = BaseWeaponComponent.Cast(entity.FindComponent(BaseWeaponComponent));
				if (candidate && candidate.GetWeaponType() != EWeaponType.WT_FRAGGRENADE && candidate.GetWeaponType() != EWeaponType.WT_SMOKEGRENADE && candidate.GetWeaponType() != EWeaponType.WT_HANDGUN)
				{
					weapon = candidate;
					break;
				}
			}
		}
		if (!weapon || !weapon.GetCurrentMuzzle())
			return -1;
		return inventory.GetMagazineCountByMuzzle(body, weapon.GetCurrentMuzzle());
	}
}
