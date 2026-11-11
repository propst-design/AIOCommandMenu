// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). Licensed under the GNU GPL v3 or later (see LICENSE).

// Stance & speed > Lights: light discipline for the whole squad or selected soldiers. Server only.
// The game's AI switches lights in three places, and each first asks the man's light setting
// (SCR_AICharacterLightInteractionSetting): the group's leader turns his flashlight on at night
// and off by day as he keeps formation (KeepInFormation.bt, SCR_AIToggleFlashlights); an AI
// driver turns the headlights and high beams on in the dark while he feels safe
// (SCR_AIIdleBehavior_Driver); a driver whose vehicle is stuck turns the hazard lights on
// (SCR_AIProcessFailedMovementResult).
// - Lights off: that setting forbids him to touch his lights in every behaviour, his flashlights
//   go off and so do the headlights, high beams and hazard lights of a vehicle he drives. The
//   watchdog puts out whatever comes on again (a flashlight back from his pack, a lit vehicle he
//   takes the wheel of, another mod) until he is told otherwise or leaves the squad.
// - Lights: own choice: Lights off ends and the game decides again. It only undoes Lights off: a
//   Vehicles > Vehicle controls lights order a driver has stays until Drivers handle the lights.
// A Vehicle controls lights order to his vehicle replaces Lights off for the driver, and Lights
// off replaces a Vehicle controls lights order: the newer order wins.
// The order covers the men in the squad when it is given; men who join later choose their own.

class AIO_Lights
{
	// The "came on again" line: at most once a minute per man, so another mod that keeps
	// re-lighting a lamp doesn't flood the log.
	static const float PRINT_GAP_MS = 60000;

	// Men told to keep their lights off, each with the leader who told them and the squad they
	// were in (parallel arrays).
	protected static ref array<AIAgent> s_aAgents;
	protected static ref array<IEntity> s_aLeaders;
	protected static ref array<SCR_AIGroup> s_aSquads;
	protected static ref array<float> s_aPrintAt;
	// Called by AIO_StaticInit at game start (static initializer budget: tools/check_static_init.py).
	static void AIO_InitStatics()
	{
		s_aAgents = {};
		s_aLeaders = {};
		s_aSquads = {};
		s_aPrintAt = {};
	}

	// Mission end (AIO_Game.c): the men belong to the world being torn down.
	static void Cleanup()
	{
		s_aAgents.Clear();
		s_aLeaders.Clear();
		s_aSquads.Clear();
		s_aPrintAt.Clear();
	}

	static bool IsOrder(int order)
	{
		return order == AIO_EOrder.LIGHTS_OFF || order == AIO_EOrder.LIGHTS_FREE;
	}

	// leader: the player who gave it (null in tests); squad: the squad the men are in.
	static string Order(array<IEntity> bodies, int order, IEntity leader, SCR_AIGroup squad)
	{
		bool dark = order == AIO_EOrder.LIGHTS_OFF;
		int count, torches, vehicles;
		foreach (IEntity body : bodies)
		{
			AIAgent agent = AIO_Tasks.AgentOf(body);
			// Own choice only undoes Lights off: a man without it may have a Vehicle controls
			// lights order, and that stays.
			if (!dark && !IsDark(agent))
				continue;
			if (!Allow(agent, !dark))
				continue;
			count++;
			Drop(agent);
			if (!dark)
				continue;
			s_aAgents.Insert(agent);
			s_aLeaders.Insert(leader);
			s_aSquads.Insert(squad);
			s_aPrintAt.Insert(0);
			torches += FlashlightsOff(body);
			if (VehicleLightsOff(body))
				vehicles++;
		}
		if (!dark)
		{
			if (count == 0)
				return "None of them have Lights off";
			return string.Format("%1 men choose their own lights again", count);
		}
		if (count == 0)
			return "No soldiers under your command";
		AIO_Tasks.Watch();
		string reply = string.Format("%1 men keep their lights off", count);
		if (torches > 0 || vehicles > 0)
			reply += string.Format(" (switched off: %1 flashlights, %2 vehicles)", torches, vehicles);
		return reply;
	}

	// allowed = false: he may not touch his lights in any behaviour (the game's AI asks this
	// setting before each switch); true: the commanded setting goes and the game decides again.
	protected static bool Allow(AIAgent agent, bool allowed)
	{
		if (!agent)
			return false;
		IEntity body = agent.GetControlledEntity();
		if (!body || !AIO_Server.IsAlive(body) || AIO_Server.IsPlayerControlled(body))
			return false; // a player has him (#16): his lights are his own
		SCR_AICharacterSettingsComponent settings = SCR_AICharacterSettingsComponent.Cast(agent.FindComponent(SCR_AICharacterSettingsComponent));
		if (!settings)
			return false;
		settings.RemoveSettingsOfTypeAndOrigin(SCR_AICharacterLightInteractionSettingBase, SCR_EAISettingOrigin.COMMANDING);
		if (!allowed)
			settings.AddCharacterSetting(SCR_AICharacterLightInteractionSetting.Create(SCR_EAISettingOrigin.COMMANDING, SCR_EAIBehaviorCause.ALWAYS, false), false, true);
		return true;
	}

	// May the game's AI switch his lights now (no setting forbids it)?
	static bool MayTouchLights(AIAgent agent)
	{
		if (!agent)
			return true;
		SCR_AICharacterSettingsComponent settings = SCR_AICharacterSettingsComponent.Cast(agent.FindComponent(SCR_AICharacterSettingsComponent));
		if (!settings)
			return true;
		SCR_AICharacterLightInteractionSettingBase setting = SCR_AICharacterLightInteractionSettingBase.Cast(settings.GetCurrentSetting(SCR_AICharacterLightInteractionSettingBase));
		return !setting || setting.IsLightInterractionAllowed();
	}

	// Every flashlight he carries switched off the way the game's AI does it (the gadget manager
	// replicates it). Returns how many were on.
	static int FlashlightsOff(IEntity body)
	{
		SCR_GadgetManagerComponent gadgets = SCR_GadgetManagerComponent.GetGadgetManager(body);
		if (!gadgets)
			return 0;
		array<SCR_GadgetComponent> torches = gadgets.GetGadgetsByType(EGadgetType.FLASHLIGHT);
		if (!torches)
			return 0;
		int switched;
		foreach (SCR_GadgetComponent torch : torches)
		{
			if (!torch || !torch.IsToggledOn())
				continue;
			torch.ToggleActive(false, SCR_EUseContext.FROM_ACTION);
			switched++;
		}
		return switched;
	}

	static bool FlashlightOn(IEntity body)
	{
		SCR_GadgetManagerComponent gadgets = SCR_GadgetManagerComponent.GetGadgetManager(body);
		if (!gadgets)
			return false;
		array<SCR_GadgetComponent> torches = gadgets.GetGadgetsByType(EGadgetType.FLASHLIGHT);
		if (!torches)
			return false;
		foreach (SCR_GadgetComponent torch : torches)
		{
			if (torch && torch.IsToggledOn())
				return true;
		}
		return false;
	}

	// The vehicle he drives: headlights, high beams and hazard lights off (the ones the game's AI
	// drivers switch). True if any was on.
	static bool VehicleLightsOff(IEntity body)
	{
		IEntity vehicle = AIO_Vehicles.VehicleOf(body);
		if (!vehicle || AIO_Vehicles.Driver(vehicle) != body)
			return false;
		BaseLightManagerComponent lights = BaseLightManagerComponent.Cast(vehicle.FindComponent(BaseLightManagerComponent));
		if (!lights)
			return false;
		bool head = SwitchOff(lights, ELightType.Head);
		bool beams = SwitchOff(lights, ELightType.HiBeam);
		bool hazard = SwitchOff(lights, ELightType.Hazard);
		return head || beams || hazard;
	}

	protected static bool SwitchOff(BaseLightManagerComponent lights, ELightType type)
	{
		if (!lights.GetLightsState(type))
			return false;
		lights.SetLightsState(type, false);
		return true;
	}

	static bool IsDark(AIAgent agent)
	{
		return agent && s_aAgents.Contains(agent);
	}

	static string Label(AIAgent agent)
	{
		if (IsDark(agent))
			return "Lights off";
		return "";
	}

	// A Vehicle controls lights order to his vehicle (AIO_Vehicles.LeaveLightsAlone) replaces his
	// light setting: the watchdog must not switch off what that order turned on.
	static void Forget(AIAgent agent)
	{
		Drop(agent);
	}

	// He leaves the squad (AIO_Recruit.Clean: dismissed, destroyed) or the watchdog finds him
	// gone from it: Lights off ends for him and the game decides his lights again.
	static void Release(AIAgent agent)
	{
		if (!IsDark(agent))
			return;
		Drop(agent);
		SCR_AICharacterSettingsComponent settings = SCR_AICharacterSettingsComponent.Cast(agent.FindComponent(SCR_AICharacterSettingsComponent));
		if (settings)
			settings.RemoveSettingsOfTypeAndOrigin(SCR_AICharacterLightInteractionSettingBase, SCR_EAISettingOrigin.COMMANDING);
	}

	protected static void Drop(AIAgent agent)
	{
		int i = s_aAgents.Find(agent);
		if (i >= 0)
			DropAt(i);
	}

	protected static void DropAt(int i)
	{
		s_aAgents.Remove(i);
		s_aLeaders.Remove(i);
		s_aSquads.Remove(i);
		s_aPrintAt.Remove(i);
	}

	// Still in the squad that was told? The leader who gave the order must still command him
	// (AIO_Server.StillCommands); while that leader is dead or the order came without one, the
	// man must still be in the squad he was in.
	protected static bool StillInSquad(int i, AIAgent agent, IEntity body)
	{
		IEntity leader = s_aLeaders[i];
		if (leader && AIO_Server.IsAlive(leader) && !AIO_Server.StillCommands(leader, body))
			return false;
		SCR_AIGroup squad = s_aSquads[i];
		return !squad || agent.GetParentGroup() == squad;
	}

	// Called by the watchdog once a second: whatever came on again goes out. A man a player
	// possesses keeps the order but is left alone (#16) until the AI has him back; a man who
	// left the squad is released.
	static void Tick()
	{
		float now = GetGame().GetWorld().GetWorldTime();
		for (int i = s_aAgents.Count() - 1; i >= 0; i--)
		{
			AIAgent agent = s_aAgents[i];
			IEntity body = null;
			if (agent)
				body = agent.GetControlledEntity();
			if (!body || !AIO_Server.IsAlive(body))
			{
				DropAt(i);
				continue;
			}
			if (!StillInSquad(i, agent, body))
			{
				Print(string.Format("[AIO] LIGHTS %1 left the squad: Lights off ends for him", AIO_Server.NameOf(body)));
				Release(agent);
				continue;
			}
			if (AIO_Server.IsPlayerControlled(body))
				continue;
			int torches = FlashlightsOff(body);
			bool vehicle = VehicleLightsOff(body);
			if ((torches > 0 || vehicle) && now >= s_aPrintAt[i])
			{
				s_aPrintAt[i] = now + PRINT_GAP_MS;
				Print(string.Format("[AIO] LIGHTS %1: lights came on again, switched off (%2 flashlights, vehicle %3)", AIO_Server.NameOf(body), torches, vehicle));
			}
		}
	}
}
