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
		super.OnBeforeWorldCleanup();
	}
}
