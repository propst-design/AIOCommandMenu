// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). Licensed under the GNU GPL v3 or later (see LICENSE).

modded class ArmaReforgerScripted
{
	override bool OnGameStart()
	{
		bool started = super.OnGameStart();
		Print("[AIO] ready (All-in-One Command Menu loaded)");
		AIO_Test.MaybeStart();
		return started;
	}

	// The UI workspace outlives the world: take our widgets down with it.
	override protected void OnBeforeWorldCleanup()
	{
		AIO_Menu.Cleanup();
		AIO_Routes.Cleanup();
		AIO_Bounding.Cleanup();
		AIO_Support.Cleanup();
		// Server jobs and the watchdog: statics outlive the world (e.g. an admin #restart).
		AIO_Tasks.Cleanup();
		AIO_Maneuvers.Cleanup();
		AIO_Behaviour.Cleanup();
		AIO_Explosives.Cleanup();
		AIO_Grenades.Cleanup();
		AIO_Repair.Cleanup();
		AIO_FireSupport.Cleanup();
		AIO_Logistics.Cleanup();
		AIO_Supplies.Cleanup();
		AIO_Vehicles.Cleanup();
		super.OnBeforeWorldCleanup();
	}
}
