// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). GPL v3 or later (see LICENSE).

// The verdict line every test suite ends with: "[AIO] VERDICT PASS|FAIL test=... passed= failed=
// missing= expected=". A suite passes only with no failures, at least one check, and no fewer checks
// than it declares, so a check that silently stops running (a skipped branch, an early return) fails
// the run instead of passing with fewer lines. Nightly sweep and verdict tools read this line.
// Raise a suite's count with every new Check. -1 = not declared (PASS-UNDECLARED). The counts were
// taken from the result history at main 1f82240 (5 Nov 2026); the -aca presets run the same suites.
class AIO_TestVerdict
{
	static int Expected()
	{
		if (System.IsCLIParam("aioTaskTest")) return 7;
		if (System.IsCLIParam("aioMoveTest")) return 5;
		if (System.IsCLIParam("aioInfantryTest")) return 7;
		if (System.IsCLIParam("aioSupplyTest")) return 5;
		if (System.IsCLIParam("aioAttackTest")) return 8;
		if (System.IsCLIParam("aioDefenseTest")) return 8;
		if (System.IsCLIParam("aioTowerTest")) return 10;
		if (System.IsCLIParam("aioGarrisonTest")) return 9;
		if (System.IsCLIParam("aioVehicleTest")) return 19;
		if (System.IsCLIParam("aioRecoveryTest")) return 5;
		if (System.IsCLIParam("aioFormationTest")) return 10;
		if (System.IsCLIParam("aioSupportTest")) return 29;
		if (System.IsCLIParam("aioRadiusTest")) return 8;
		if (System.IsCLIParam("aioContactTest")) return 18;
		if (System.IsCLIParam("aioTeleportTest")) return 27;
		if (System.IsCLIParam("aioBehaviourTest")) return 10;
		if (System.IsCLIParam("aioExplosivesTest")) return 9;
		if (System.IsCLIParam("aioSpecialistsTest")) return 8;
		if (System.IsCLIParam("aioFireMissionTest")) return 3;
		if (System.IsCLIParam("aioGrenadesTest")) return 18;
		if (System.IsCLIParam("aioLeadershipTest")) return 8;
		return -1;
	}

	static string Name()
	{
		array<string> names = {"Task", "Move", "Infantry", "Supply", "Attack", "Defense", "Tower", "Garrison", "Vehicle", "Recovery",
			"Formation", "Support", "Radius", "Contact", "Teleport", "Behaviour", "Explosives", "Specialists", "FireMission", "Grenades",
			"Leadership", "Repair"};
		foreach (string name : names)
		{
			if (System.IsCLIParam("aio" + name + "Test"))
				return name;
		}
		return "test";
	}

	// Call right before "[AIO] TEST COMPLETE".
	static void Report(int passed, int failed)
	{
		int expected = Expected();
		int ran = passed + failed;
		int missing = 0;
		if (expected >= 0 && expected > ran)
			missing = expected - ran;
		string verdict = "PASS";
		if (failed > 0 || missing > 0 || ran == 0)
			verdict = "FAIL";
		else if (expected < 0)
			verdict = "PASS-UNDECLARED";
		string expectedText = "?";
		if (expected >= 0)
			expectedText = expected.ToString();
		PrintFormat("[AIO] VERDICT %1 test=%2 passed=%3 failed=%4 missing=%5 expected=%6", verdict, Name(), passed, failed, missing, expectedText);
	}
}
