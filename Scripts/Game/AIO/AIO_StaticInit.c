// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). Licensed under the GNU GPL v3 or later (see LICENSE).

// Static lists are filled here, once, when the game object is created (AIO_Game.c), instead of
// with "= {}" on the declaration. The engine builds every such initializer inside one hidden
// function per script module, and that function has an instruction limit shared by every loaded
// mod: with a big mod list the Game module stops compiling. tools/check_static_init.py (CI)
// keeps new ones out.
class AIO_StaticInit
{
	protected static bool s_bFilled;

	static void Run()
	{
		if (s_bFilled)
			return;
		s_bFilled = true;
		AIO_Behaviour.AIO_InitStatics();
		AIO_Bounding.AIO_InitStatics();
		AIO_ContactTest.AIO_InitStatics();
		AIO_Explosives.AIO_InitStatics();
		AIO_FireSupport.AIO_InitStatics();
		AIO_GarrisonTest.AIO_InitStatics();
		AIO_Grenades.AIO_InitStatics();
		AIO_Lights.AIO_InitStatics();
		AIO_Logistics.AIO_InitStatics();
		AIO_Maneuvers.AIO_InitStatics();
		AIO_Repair.AIO_InitStatics();
		AIO_Routes.AIO_InitStatics();
		AIO_Server.AIO_InitStatics();
		AIO_Supplies.AIO_InitStatics();
		AIO_Support.AIO_InitStatics();
		AIO_Tasks.AIO_InitStatics();
		AIO_Test.AIO_InitStatics();
		AIO_Vehicles.AIO_InitStatics();
	}
}
