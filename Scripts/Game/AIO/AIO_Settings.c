// All-in-One Command Menu for Arma Reforger
// Copyright (C) 2026 Jacob (propst-design). Licensed under the GNU GPL v3 or later (see LICENSE).

// The player's menu options (More > Aim marker, Mouse control, Squad bar), kept across missions
// and game restarts in the game profile as "key=value" lines. Client only. Unknown keys and bad
// values are ignored, so an old or hand-edited file can't break the menu.
class AIO_Settings
{
	static const string FILE = "$profile:AIO_MenuSettings.txt";

	static AIO_EAimStyle s_eAimStyle = AIO_EAimStyle.CROSSHAIR;
	static bool s_bAimNoDistance;
	static int s_iAimSize = 1; // 0 small, 1 medium, 2 large
	static bool s_bMouse;
	static bool s_bSquadBarOff;

	protected static bool s_bLoaded;

	static void Load()
	{
		if (s_bLoaded)
			return;
		s_bLoaded = true;
		if (!FileIO.FileExists(FILE))
			return;
		FileHandle file = FileIO.OpenFile(FILE, FileMode.READ);
		if (!file)
			return;
		string line;
		while (file.ReadLine(line) > 0)
		{
			int split = line.IndexOf("=");
			if (split <= 0)
				continue;
			string key = line.Substring(0, split).Trim();
			string value = line.Substring(split + 1, line.Length() - split - 1).Trim();
			int number = value.ToInt();
			switch (key)
			{
				case "aimStyle":
					if (number >= AIO_EAimStyle.CROSSHAIR && number <= AIO_EAimStyle.OFF)
						s_eAimStyle = number;
					break;
				case "aimDistance": s_bAimNoDistance = number == 0; break;
				case "aimSize":
					if (number >= 0 && number <= 2)
						s_iAimSize = number;
					break;
				case "mouse": s_bMouse = number != 0; break;
				case "squadBar": s_bSquadBarOff = number == 0; break;
			}
		}
		file.Close();
	}

	static void Save()
	{
		FileHandle file = FileIO.OpenFile(FILE, FileMode.WRITE);
		if (!file)
		{
			Print("[AIO] could not save menu settings to " + FILE, LogLevel.WARNING);
			return;
		}
		int style = s_eAimStyle;
		file.WriteLine("aimStyle=" + style.ToString()); // 0 crosshair, 1 ring, 2 off
		file.WriteLine("aimDistance=" + Flag(!s_bAimNoDistance));
		file.WriteLine("aimSize=" + s_iAimSize.ToString()); // 0 small, 1 medium, 2 large
		file.WriteLine("mouse=" + Flag(s_bMouse));
		file.WriteLine("squadBar=" + Flag(!s_bSquadBarOff));
		file.Close();
	}

	protected static string Flag(bool on)
	{
		if (on)
			return "1";
		return "0";
	}
}
