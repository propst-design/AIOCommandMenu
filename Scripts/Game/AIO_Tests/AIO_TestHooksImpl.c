// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). Licensed under the GNU GPL v3 or later (see LICENSE).

// Plugs the tests into the game's hooks (AIO_TestHooks.c). This folder is left out of the Workshop
// package, so nothing here may be named by game code.
modded class AIO_TestHooks
{
	override void InitStatics()
	{
		AIO_ContactTest.AIO_InitStatics();
		AIO_GarrisonTest.AIO_InitStatics();
		AIO_SquadTradeTest.AIO_InitStatics();
		AIO_Test.AIO_InitStatics();
	}

	override void GameStarted()
	{
		AIO_Test.MaybeStart();
	}
}
