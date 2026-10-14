// All-in-One Command Menu for Arma Reforger
// Based on Leopard20's All-In-One Command Menu for Arma 3 (GPLv3); rebuilt in Enforce Script.
// Copyright (C) 2026 Jacob (propst-design). Licensed under the GNU GPL v3 or later (see LICENSE).

// One Unflip job: helpers walk to a vehicle on its side, then it is set back on its wheels.
class AIO_UnflipJob
{
	IEntity m_Vehicle;
	ref array<IEntity> m_aHelpers = {};
	float m_fReadyAt;   // world time (ms) the push happens once enough helpers are there; 0 = not yet
	float m_fGiveUpAt;
	int m_iRights;      // times set upright (checked again after 2 s, up to 3 tries)
	float m_fCheckAt;
}

// A vehicle men were ordered out of: dropped from the squad's vehicles once nobody of the
// squad is aboard, so the squad's next move doesn't put them straight back in.
class AIO_DismountWatch
{
	IEntity m_Vehicle;
	SCR_AIGroup m_Group;
	float m_fUntil;
}

// Vehicle orders (Vehicles page): get in (any seat / driver / gunner / passenger), switch seats,
// get out, disembark passengers, eject, engine and lights, unflip. Per man, through the game's own
// get-in / get-out behaviours, so the squad's vehicle handling (who drives, formation) is vanilla.
class AIO_Vehicles
{
	static const float FLIPPED_DEG = 45;      // same test as the game's own Unflip action
	static const int UNFLIP_HELPERS = 4;
	static const float UNFLIP_TIMEOUT_MS = 90000;
	static const float DISMOUNT_WATCH_MS = 40000;

	protected static ref array<ref AIO_UnflipJob> s_aUnflips = {};
	protected static ref array<ref AIO_DismountWatch> s_aDismounts = {};

	static bool IsVehicleOrder(int order)
	{
		return order >= AIO_EOrder.VEH_GET_IN_ANY && order <= AIO_EOrder.VEH_LIGHTS_AUTO;
	}

	// ---------------------------------------------------------------- lookups

	// The vehicle (root entity with seats) the entity belongs to, or null.
	static IEntity VehicleRoot(IEntity entity)
	{
		if (!entity)
			return null;
		IEntity root = entity.GetRootParent();
		if (!root || ChimeraCharacter.Cast(root))
			return null;
		if (!root.FindComponent(BaseCompartmentManagerComponent))
			return null;
		return root;
	}

	// The vehicle the man sits in, or null.
	static IEntity VehicleOf(IEntity body)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(body);
		if (!character || !character.IsInVehicle())
			return null;
		IEntity inside = CompartmentAccessComponent.GetVehicleIn(body);
		if (!inside)
			return null;
		return inside.GetRootParent();
	}

	static BaseCompartmentSlot SeatOf(IEntity body)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(body);
		if (!character)
			return null;
		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (!access)
			return null;
		return access.GetCompartment();
	}

	// Every seat of the vehicle, turrets on child entities included (GetCompartments isn't recursive).
	static void Seats(IEntity entity, notnull array<BaseCompartmentSlot> seats)
	{
		if (!entity)
			return;
		BaseCompartmentManagerComponent manager = BaseCompartmentManagerComponent.Cast(entity.FindComponent(BaseCompartmentManagerComponent));
		if (manager)
		{
			array<BaseCompartmentSlot> own = {};
			manager.GetCompartments(own);
			foreach (BaseCompartmentSlot slot : own)
			{
				if (slot && !seats.Contains(slot))
					seats.Insert(slot);
			}
		}
		IEntity child = entity.GetChildren();
		while (child)
		{
			if (!ChimeraCharacter.Cast(child))
				Seats(child, seats);
			child = child.GetSibling();
		}
	}

	static bool SeatFree(BaseCompartmentSlot slot, IEntity body)
	{
		if (!slot.IsCompartmentAccessible() || slot.IsGetInLockedFor(body))
			return false;
		if (slot.IsReserved() && !slot.IsReservedBy(body))
			return false;
		return slot.GetOccupant() == null;
	}

	// A free seat of that type (ECompartmentType), or of any type when type < 0 (driver, then
	// gunners, then passengers).
	static BaseCompartmentSlot FreeSeat(IEntity vehicle, int type, IEntity body)
	{
		array<BaseCompartmentSlot> seats = {};
		Seats(vehicle, seats);
		array<int> order = {};
		if (type >= 0)
			order.Insert(type);
		else
			order = {ECompartmentType.PILOT, ECompartmentType.TURRET, ECompartmentType.CARGO};
		foreach (int wanted : order)
		{
			foreach (BaseCompartmentSlot slot : seats)
			{
				if (slot.GetType() == wanted && SeatFree(slot, body))
					return slot;
			}
		}
		return null;
	}

	static string SeatName(int type)
	{
		if (type == ECompartmentType.PILOT)
			return "Driver";
		if (type == ECompartmentType.TURRET)
			return "Gunner";
		if (type == ECompartmentType.CARGO)
			return "Passenger";
		return "Seat";
	}

	// Squad bar label: his seat, or Boarding / Dismounting on the way.
	static string Label(IEntity body)
	{
		SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(body));
		if (utility && utility.HasActionOfType(SCR_AIGetOutVehicle))
			return "Dismounting";
		if (utility && utility.HasActionOfType(SCR_AIGetInVehicle))
			return "Boarding";
		BaseCompartmentSlot seat = SeatOf(body);
		if (seat)
			return SeatName(seat.GetType());
		return "";
	}

	// The vehicle an order is about: the one looked at, else the player's own, else the nearest
	// within 15 m of the point looked at.
	static IEntity TargetVehicle(IEntity looked, IEntity player, vector point)
	{
		IEntity vehicle = VehicleRoot(looked);
		if (vehicle)
			return vehicle;
		vehicle = VehicleOf(player);
		if (vehicle)
			return vehicle;
		if (point == vector.Zero)
			return null;
		s_Nearest = null;
		s_fNearest = 15 * 15;
		s_vNear = point;
		GetGame().GetWorld().QueryEntitiesBySphere(point, 15, NearestVehicle, null, EQueryEntitiesFlags.DYNAMIC);
		return s_Nearest;
	}

	protected static IEntity s_Nearest;
	protected static float s_fNearest;
	protected static vector s_vNear;

	protected static bool NearestVehicle(IEntity entity)
	{
		if (!Vehicle.Cast(entity) || entity.GetParent())
			return true;
		float distance = vector.DistanceSq(entity.GetOrigin(), s_vNear);
		if (distance < s_fNearest)
		{
			s_fNearest = distance;
			s_Nearest = entity;
		}
		return true;
	}

	// ---------------------------------------------------------------- get in

	// type: ECompartmentType, or -1 = any seat.
	static string GetIn(notnull array<IEntity> bodies, IEntity vehicle, int type)
	{
		if (!vehicle)
			return "Look at a vehicle (or be in one)";
		if (bodies.IsEmpty())
			return "No soldiers under your command";
		int boarding, already, noSeat, tooFar;
		foreach (IEntity body : bodies)
		{
			if (vector.Distance(body.GetOrigin(), vehicle.GetOrigin()) > 400)
			{
				tooFar++;
				continue;
			}
			BaseCompartmentSlot current = SeatOf(body);
			if (current && VehicleOf(body) == vehicle && (type < 0 || current.GetType() == type))
			{
				already++;
				continue;
			}
			BaseCompartmentSlot seat = FreeSeat(vehicle, type, body);
			if (!seat)
			{
				noSeat++;
				continue;
			}
			if (Board(body, vehicle, seat))
				boarding++;
		}
		string reply = string.Format("%1 men getting in", boarding);
		if (type >= 0)
			reply = string.Format("%1 men getting in as %2", boarding, SeatName(type));
		if (already > 0)
			reply += string.Format(", %1 already there", already);
		if (noSeat > 0)
			reply += string.Format(", no seat for %1", noSeat);
		if (tooFar > 0)
			reply += string.Format(", %1 too far away", tooFar);
		return reply;
	}

	// Sends the man to that seat (getting out of another vehicle first). Same seat type in his own
	// vehicle and section = a direct seat switch.
	static bool Board(IEntity body, IEntity vehicle, BaseCompartmentSlot seat)
	{
		AIAgent agent = AIO_Tasks.AgentOf(body);
		SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(agent);
		if (!utility || !seat)
			return false;
		AIO_Tasks.Release(body);
		utility.SetStateAllActionsOfType(SCR_AIGetInVehicle, EAIActionState.FAILED);
		utility.SetStateAllActionsOfType(SCR_AIGetOutVehicle, EAIActionState.FAILED);
		seat.SetReserved(body);
		IEntity inside = VehicleOf(body);
		if (inside)
		{
			BaseCompartmentSlot current = SeatOf(body);
			if (inside == vehicle && current && current.GetCompartmentSection() == seat.GetCompartmentSection())
			{
				ChimeraCharacter character = ChimeraCharacter.Cast(body);
				CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
				if (access && access.GetInVehicle(seat.GetOwner(), seat, false, -1, ECloseDoorAfterActions.RETURN_TO_PREVIOUS_STATE, false))
				{
					GetGame().GetCallqueue().CallLater(Unreserve, 5000, false, seat, body);
					Print(string.Format("[AIO] VEHICLE %1 switches to %2", AIO_Server.NameOf(body), SeatName(seat.GetType())));
					return true;
				}
			}
			// Out first (higher priority), then in.
			utility.AddAction(new SCR_AIGetOutVehicle(utility, null, inside, priority: SCR_AIActionBase.PRIORITY_BEHAVIOR_GET_IN_VEHICLE + 10));
		}
		utility.AddAction(new SCR_AIGetInVehicle(utility, null, vehicle, seat, seat.GetType(), SCR_AIActionBase.PRIORITY_BEHAVIOR_GET_IN_VEHICLE));
		Print(string.Format("[AIO] VEHICLE %1 boards %2 as %3", AIO_Server.NameOf(body), vehicle, SeatName(seat.GetType())));
		return true;
	}

	protected static void Unreserve(BaseCompartmentSlot seat, IEntity body)
	{
		if (seat && seat.IsReservedBy(body))
			seat.SetReserved(null);
	}

	// ---------------------------------------------------------------- switch seat

	static string SwitchSeat(notnull array<IEntity> bodies, int type)
	{
		int moving, already, noSeat, outside;
		foreach (IEntity body : bodies)
		{
			IEntity vehicle = VehicleOf(body);
			BaseCompartmentSlot current = SeatOf(body);
			if (!vehicle || !current)
			{
				outside++;
				continue;
			}
			if (current.GetType() == type)
			{
				already++;
				continue;
			}
			BaseCompartmentSlot seat = FreeSeat(vehicle, type, body);
			if (!seat)
			{
				noSeat++;
				continue;
			}
			if (Board(body, vehicle, seat))
				moving++;
		}
		string reply = string.Format("%1 men moving to %2", moving, SeatName(type));
		if (already > 0)
			reply += string.Format(", %1 already there", already);
		if (noSeat > 0)
			reply += string.Format(", no free seat for %1", noSeat);
		if (outside > 0)
			reply += string.Format(", %1 not in a vehicle", outside);
		return reply;
	}

	// ---------------------------------------------------------------- get out

	// Get out (everyone), Disembark passengers (cargo seats only), Eject (out at once, even moving).
	static string GetOut(notnull array<IEntity> bodies, SCR_AIGroup group, int order)
	{
		bool cargoOnly = order == AIO_EOrder.VEH_DISEMBARK;
		bool eject = order == AIO_EOrder.VEH_EJECT;
		int leaving;
		array<IEntity> vehicles = {};
		foreach (IEntity body : bodies)
		{
			IEntity vehicle = VehicleOf(body);
			BaseCompartmentSlot seat = SeatOf(body);
			if (!vehicle || !seat)
				continue;
			if (cargoOnly && seat.GetType() != ECompartmentType.CARGO)
				continue;
			AIAgent agent = AIO_Tasks.AgentOf(body);
			SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(agent);
			if (!utility)
				continue;
			AIO_Tasks.Release(body);
			utility.SetStateAllActionsOfType(SCR_AIGetInVehicle, EAIActionState.FAILED);
			if (eject)
			{
				ChimeraCharacter character = ChimeraCharacter.Cast(body);
				CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
				if (!access || !access.GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.LEAVE_OPEN, false))
					continue;
			}
			else
				utility.AddAction(new SCR_AIGetOutVehicle(utility, null, vehicle, priority: SCR_AIActionBase.PRIORITY_BEHAVIOR_GET_OUT_VEHICLE_HIGH_PRIORITY));
			leaving++;
			if (!vehicles.Contains(vehicle))
				vehicles.Insert(vehicle);
		}
		if (!cargoOnly)
		{
			foreach (IEntity vehicle : vehicles)
				WatchDismount(vehicle, group);
		}
		if (leaving == 0)
		{
			if (cargoOnly)
				return "No passengers to disembark";
			return "Nobody is in a vehicle";
		}
		if (eject)
			return string.Format("%1 men bailing out", leaving);
		if (cargoOnly)
			return string.Format("%1 passengers getting out", leaving);
		return string.Format("%1 men getting out", leaving);
	}

	protected static void WatchDismount(IEntity vehicle, SCR_AIGroup group)
	{
		if (!vehicle || !group)
			return;
		AIO_DismountWatch watch = new AIO_DismountWatch();
		watch.m_Vehicle = vehicle;
		watch.m_Group = group;
		watch.m_fUntil = GetGame().GetWorld().GetWorldTime() + DISMOUNT_WATCH_MS;
		s_aDismounts.Insert(watch);
		AIO_Tasks.Watch();
	}

	// True when nobody of the group sits in or is boarding the vehicle.
	static bool GroupLeft(SCR_AIGroup group, IEntity vehicle)
	{
		array<IEntity> members = {};
		AIO_Server.Members(group, members);
		foreach (IEntity body : members)
		{
			if (VehicleOf(body) == vehicle)
				return false;
			SCR_AIUtilityComponent utility = AIO_Tasks.UtilityOf(AIO_Tasks.AgentOf(body));
			if (utility && utility.HasActionOfType(SCR_AIGetInVehicle))
				return false;
		}
		return true;
	}

	static void ForgetVehicle(SCR_AIGroup group, IEntity vehicle)
	{
		SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(group.FindComponent(SCR_AIGroupUtilityComponent));
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.FindOnNearestParent(vehicle, vehicle);
		if (!utility || !usage || !utility.m_VehicleMgr || !utility.IsUsableVehicle(usage))
			return;
		utility.m_VehicleMgr.RemoveVehicle(usage);
		Print(string.Format("[AIO] VEHICLE %1 no longer used by the squad", vehicle));
	}

	// ---------------------------------------------------------------- controls

	static string Controls(notnull array<IEntity> bodies, int order)
	{
		array<IEntity> vehicles = {};
		foreach (IEntity body : bodies)
		{
			IEntity vehicle = VehicleOf(body);
			if (vehicle && !vehicles.Contains(vehicle))
				vehicles.Insert(vehicle);
		}
		if (vehicles.IsEmpty())
			return "Nobody is in a vehicle";
		int done;
		foreach (IEntity vehicle : vehicles)
		{
			if (order == AIO_EOrder.VEH_LIGHTS_ON || order == AIO_EOrder.VEH_LIGHTS_OFF || order == AIO_EOrder.VEH_LIGHTS_AUTO)
			{
				BaseLightManagerComponent lights = BaseLightManagerComponent.Cast(vehicle.FindComponent(BaseLightManagerComponent));
				if (!lights)
					continue;
				// An AI driver switches the headlights by daylight and danger: our choice holds only
				// while he is told to leave them alone.
				bool keepLights = order != AIO_EOrder.VEH_LIGHTS_AUTO;
				LeaveLightsAlone(Driver(vehicle), keepLights);
				if (keepLights)
					lights.SetLightsState(ELightType.Head, order == AIO_EOrder.VEH_LIGHTS_ON);
				done++;
				continue;
			}
			BaseVehicleControllerComponent controller = BaseVehicleControllerComponent.Cast(vehicle.FindComponent(BaseVehicleControllerComponent));
			if (!controller || !HasDriver(vehicle))
				continue;
			if (order == AIO_EOrder.VEH_ENGINE_ON)
				controller.StartEngine();
			else
				controller.StopEngine();
			done++;
		}
		switch (order)
		{
			case AIO_EOrder.VEH_LIGHTS_ON: return string.Format("Lights on (%1 vehicles)", done);
			case AIO_EOrder.VEH_LIGHTS_OFF: return string.Format("Lights off (%1 vehicles)", done);
			case AIO_EOrder.VEH_LIGHTS_AUTO: return string.Format("Drivers handle the lights (%1 vehicles)", done);
			case AIO_EOrder.VEH_ENGINE_ON:
				if (done == 0)
					return "Engine on needs one of them in the driver's seat";
				return string.Format("Engine on (%1 vehicles)", done);
		}
		if (done == 0)
			return "Engine off needs one of them in the driver's seat";
		return string.Format("Engine off (%1 vehicles)", done);
	}

	static bool HasDriver(IEntity vehicle)
	{
		return Driver(vehicle) != null;
	}

	static IEntity Driver(IEntity vehicle)
	{
		array<BaseCompartmentSlot> seats = {};
		Seats(vehicle, seats);
		foreach (BaseCompartmentSlot seat : seats)
		{
			if (seat.GetType() == ECompartmentType.PILOT && seat.GetOccupant())
				return seat.GetOccupant();
		}
		return null;
	}

	// keep = true: the AI driver doesn't touch the lights; false = his own choice again.
	static void LeaveLightsAlone(IEntity driver, bool keep)
	{
		AIAgent agent = AIO_Tasks.AgentOf(driver);
		if (!agent)
			return;
		SCR_AICharacterSettingsComponent settings = SCR_AICharacterSettingsComponent.Cast(agent.FindComponent(SCR_AICharacterSettingsComponent));
		if (!settings)
			return;
		settings.RemoveSettingsOfTypeAndOrigin(SCR_AICharacterLightInteractionSettingBase, SCR_EAISettingOrigin.COMMANDING);
		if (keep)
			settings.AddCharacterSetting(SCR_AICharacterLightInteractionSetting.Create(SCR_EAISettingOrigin.COMMANDING, SCR_EAIBehaviorCause.GROUP_GOAL, false), false, true);
	}

	// ---------------------------------------------------------------- unflip

	static bool IsFlipped(IEntity vehicle)
	{
		vector angles = vehicle.GetYawPitchRoll();
		return Math.AbsFloat(angles[1]) > FLIPPED_DEG || Math.AbsFloat(angles[2]) > FLIPPED_DEG;
	}

	// Up to four men on foot walk to the vehicle; once two (or all, if fewer) are at it they push
	// it back on its wheels.
	static string Unflip(notnull array<IEntity> bodies, IEntity vehicle)
	{
		if (!vehicle)
			return "Look at a vehicle on its side";
		if (!IsFlipped(vehicle))
			return "That vehicle is on its wheels";
		array<IEntity> walkers = {};
		foreach (IEntity body : bodies)
		{
			if (!VehicleOf(body))
				walkers.Insert(body);
		}
		if (walkers.IsEmpty())
			return "Nobody on foot to push it";
		// Nearest first.
		AIO_UnflipJob job = new AIO_UnflipJob();
		job.m_Vehicle = vehicle;
		while (!walkers.IsEmpty() && job.m_aHelpers.Count() < UNFLIP_HELPERS)
		{
			int best = 0;
			for (int i = 1; i < walkers.Count(); i++)
			{
				if (vector.DistanceSq(walkers[i].GetOrigin(), vehicle.GetOrigin()) < vector.DistanceSq(walkers[best].GetOrigin(), vehicle.GetOrigin()))
					best = i;
			}
			job.m_aHelpers.Insert(walkers[best]);
			walkers.Remove(best);
		}
		// Spots around it, clear of where it will stand once upright (it would hit them).
		vector center = Center(vehicle);
		float ring = Reach(vehicle) - 2;
		for (int h = 0; h < job.m_aHelpers.Count(); h++)
		{
			float angle = h * Math.PI * 0.5 + 0.4;
			vector spot = center + Vector(Math.Cos(angle) * ring, 0, Math.Sin(angle) * ring);
			spot[1] = GetGame().GetWorld().GetSurfaceY(spot[0], spot[2]);
			AIO_Tasks.MoveTo(job.m_aHelpers[h], spot);
		}
		job.m_fGiveUpAt = GetGame().GetWorld().GetWorldTime() + UNFLIP_TIMEOUT_MS;
		foreach (AIO_UnflipJob other : s_aUnflips)
		{
			if (other.m_Vehicle == vehicle)
			{
				s_aUnflips.RemoveItem(other);
				break;
			}
		}
		s_aUnflips.Insert(job);
		AIO_Tasks.Watch();
		return string.Format("%1 men going to push it over", job.m_aHelpers.Count());
	}

	// How close a helper must be: half the vehicle's longest side plus a few metres.
	static float Reach(IEntity vehicle)
	{
		vector mins, maxs;
		vehicle.GetBounds(mins, maxs);
		float size = Math.Max(maxs[0] - mins[0], maxs[2] - mins[2]);
		size = Math.Max(size, maxs[1] - mins[1]);
		return size * 0.5 + 4;
	}

	static vector Center(IEntity vehicle)
	{
		vector mins, maxs;
		vehicle.GetWorldBounds(mins, maxs);
		vector center = (mins + maxs) * 0.5;
		center[1] = GetGame().GetWorld().GetSurfaceY(center[0], center[2]);
		return center;
	}

	// Back on its wheels: same heading, resting just above the ground, no spin.
	static void Right(IEntity vehicle)
	{
		vector angles = vehicle.GetYawPitchRoll();
		vector mat[4];
		Math3D.AnglesToMatrix(Vector(angles[0], 0, 0), mat);
		vector center = Center(vehicle);
		vector mins, maxs;
		vehicle.GetBounds(mins, maxs);
		// Highest ground under its corners, so no wheel ends up in a slope.
		float ground = center[1];
		for (int corner = 0; corner < 4; corner++)
		{
			float cx = mins[0];
			if (corner == 1 || corner == 2)
				cx = maxs[0];
			float cz = mins[2];
			if (corner >= 2)
				cz = maxs[2];
			vector at = center + mat[0] * cx + mat[2] * cz;
			ground = Math.Max(ground, GetGame().GetWorld().GetSurfaceY(at[0], at[2]));
		}
		mat[3] = Vector(center[0], ground - mins[1] + 0.4, center[2]);
		vector previous = vehicle.GetOrigin();
		SetPose(vehicle, mat);
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if (rpl)
			rpl.ForceNodeMovement(previous);
		Print(string.Format("[AIO] VEHICLE %1 set back on its wheels", vehicle));
	}

	// Moves a physics vehicle, its simulation included (Teleport alone was undone by the
	// physics within a second; the Game Master's own transform change resets it like this).
	static void SetPose(IEntity vehicle, vector mat[4])
	{
		Physics physics = vehicle.GetPhysics();
		if (physics)
			physics.SetActive(ActiveState.ACTIVE); // a sleeping body kept its old pose
		BaseGameEntity game = BaseGameEntity.Cast(vehicle);
		if (game)
			game.Teleport(mat);
		vehicle.SetWorldTransform(mat);
		vehicle.Update();
		GenericEntity generic = GenericEntity.Cast(vehicle);
		if (generic)
			generic.OnTransformReset();
		if (physics)
		{
			physics.SetActive(ActiveState.ACTIVE);
			physics.SetVelocity(vector.Zero);
			physics.SetAngularVelocity(vector.Zero);
		}
	}

	// ---------------------------------------------------------------- watchdog (AIO_Tasks.WatchTick)

	static void Tick()
	{
		float now = GetGame().GetWorld().GetWorldTime();
		for (int d = s_aDismounts.Count() - 1; d >= 0; d--)
		{
			AIO_DismountWatch watch = s_aDismounts[d];
			if (!watch.m_Vehicle || !watch.m_Group || now > watch.m_fUntil)
			{
				s_aDismounts.Remove(d);
				continue;
			}
			if (GroupLeft(watch.m_Group, watch.m_Vehicle))
			{
				ForgetVehicle(watch.m_Group, watch.m_Vehicle);
				s_aDismounts.Remove(d);
			}
		}
		for (int u = s_aUnflips.Count() - 1; u >= 0; u--)
		{
			AIO_UnflipJob job = s_aUnflips[u];
			for (int h = job.m_aHelpers.Count() - 1; h >= 0; h--)
			{
				if (!job.m_aHelpers[h] || !AIO_Server.IsAlive(job.m_aHelpers[h]))
					job.m_aHelpers.Remove(h);
			}
			if (!job.m_Vehicle || job.m_aHelpers.IsEmpty() || now > job.m_fGiveUpAt)
			{
				if (job.m_Vehicle)
					Print(string.Format("[AIO] VEHICLE unflip of %1 given up", job.m_Vehicle));
				EndUnflip(job);
				s_aUnflips.Remove(u);
				continue;
			}
			if (!IsFlipped(job.m_Vehicle))
			{
				EndUnflip(job);
				s_aUnflips.Remove(u);
				continue;
			}
			if (job.m_iRights > 0)
			{
				// Set upright but on its side again: the physics kept its old pose; try again.
				if (now < job.m_fCheckAt)
					continue;
				if (job.m_iRights >= 3)
				{
					Print(string.Format("[AIO] VEHICLE unflip of %1 did not hold", job.m_Vehicle));
					EndUnflip(job);
					s_aUnflips.Remove(u);
					continue;
				}
				Right(job.m_Vehicle);
				job.m_iRights++;
				job.m_fCheckAt = now + 2000;
				continue;
			}
			vector center = Center(job.m_Vehicle);
			int there;
			foreach (IEntity helper : job.m_aHelpers)
			{
				float away = vector.DistanceXZ(helper.GetOrigin(), center);
				if (away <= Reach(job.m_Vehicle) && away >= Reach(job.m_Vehicle) - 3.5) // at it, not where it lands
					there++;
			}
			int needed = Math.Min(2, job.m_aHelpers.Count());
			if (there < needed)
			{
				job.m_fReadyAt = 0;
				continue;
			}
			if (job.m_fReadyAt == 0)
			{
				job.m_fReadyAt = now + 2500; // a moment of pushing
				continue;
			}
			if (now < job.m_fReadyAt)
				continue;
			Right(job.m_Vehicle);
			job.m_iRights = 1;
			job.m_fCheckAt = now + 2000;
		}
	}

	protected static void EndUnflip(AIO_UnflipJob job)
	{
		foreach (IEntity helper : job.m_aHelpers)
		{
			if (helper)
				AIO_Tasks.Release(helper);
		}
	}

	static bool IsPushing(IEntity body)
	{
		foreach (AIO_UnflipJob job : s_aUnflips)
		{
			if (job.m_aHelpers.Contains(body))
				return true;
		}
		return false;
	}

	// ---------------------------------------------------------------- dispatch (AIO_Server)

	static string Order(notnull array<IEntity> bodies, SCR_AIGroup group, int order, IEntity looked, IEntity player, vector point)
	{
		switch (order)
		{
			case AIO_EOrder.VEH_GET_IN_ANY: return GetIn(bodies, TargetVehicle(looked, player, point), -1);
			case AIO_EOrder.VEH_GET_IN_DRIVER: return GetIn(bodies, TargetVehicle(looked, player, point), ECompartmentType.PILOT);
			case AIO_EOrder.VEH_GET_IN_GUNNER: return GetIn(bodies, TargetVehicle(looked, player, point), ECompartmentType.TURRET);
			case AIO_EOrder.VEH_GET_IN_CARGO: return GetIn(bodies, TargetVehicle(looked, player, point), ECompartmentType.CARGO);
			case AIO_EOrder.VEH_TO_DRIVER: return SwitchSeat(bodies, ECompartmentType.PILOT);
			case AIO_EOrder.VEH_TO_GUNNER: return SwitchSeat(bodies, ECompartmentType.TURRET);
			case AIO_EOrder.VEH_TO_CARGO: return SwitchSeat(bodies, ECompartmentType.CARGO);
			case AIO_EOrder.VEH_GET_OUT: return GetOut(bodies, group, order);
			case AIO_EOrder.VEH_DISEMBARK: return GetOut(bodies, group, order);
			case AIO_EOrder.VEH_EJECT: return GetOut(bodies, group, order);
			case AIO_EOrder.VEH_UNFLIP: return Unflip(bodies, TargetVehicle(looked, null, point));
		}
		return Controls(bodies, order);
	}
}
