// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). Licensed under the GNU GPL v3 or later (see LICENSE).

// The only way game code reaches the automated tests. The tests live in Scripts/Game/AIO_Tests, which
// the Workshop package leaves out (ACA's release console and tools/make_release.*), and mod this class
// there (AIO_TestHooksImpl.c). In a player's download every hook does nothing.
class AIO_TestHooks
{
	protected static ref AIO_TestHooks s_Instance;

	static AIO_TestHooks Get()
	{
		if (!s_Instance)
			s_Instance = new AIO_TestHooks();
		return s_Instance;
	}

	// The tests' static lists (AIO_StaticInit: static initializer budget).
	void InitStatics() {}
	// Game started (AIO_Game.OnGameStart): a test asked for on the command line starts here.
	void GameStarted() {}
}
