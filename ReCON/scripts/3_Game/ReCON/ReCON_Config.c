// The mod's settings: $profile:ReCON/ReCON.json, written with the defaults
// on the first start.
//
//   url          where ReCON is, as the game server reaches it
//                ("http://10.0.1.20:9996"). ReCON's web port.
//   password     the server's RCON password, which ReCON also has: it is
//                what proves to ReCON that the data comes from this server.
//                Left empty, the mod reads it from BattlEye's config
//                (BEServer_x64*.cfg's RConPassword).
//   battlEyePath where BattlEye's config is, if it isn't found on its own
//                (the server's -BEpath).
//   intervalMs   how often the mod sends player data and picks up commands
//                (ReCON can ask for another rate in its reply).
class ReCON_Config
{
	string url = "http://127.0.0.1:8080";
	string password = "";
	string battlEyePath = "";
	int intervalMs = 1000;

	static const string DIR = "$profile:ReCON";
	static const string FILE = "$profile:ReCON/ReCON.json";

	static ReCON_Config Load()
	{
		ReCON_Config cfg = new ReCON_Config();
		string err;
		if (!FileExist(FILE))
		{
			if (!FileExist(DIR))
				MakeDirectory(DIR);
			if (!JsonFileLoader<ReCON_Config>.SaveFile(FILE, cfg, err))
				ReCON_Log.Info("couldn't write " + FILE + ": " + err);
			else
				ReCON_Log.Info("wrote the default settings to " + FILE + ": set url to ReCON's address");
			return cfg;
		}
		if (!JsonFileLoader<ReCON_Config>.LoadFile(FILE, cfg, err))
		{
			ReCON_Log.Info(err + " (using the defaults)");
			cfg = new ReCON_Config();
		}
		if (cfg.intervalMs < 250)
			cfg.intervalMs = 250;
		cfg.url = cfg.url.Trim();
		if (cfg.url.Length() > 0 && cfg.url.Substring(cfg.url.Length() - 1, 1) != "/")
			cfg.url = cfg.url + "/";
		return cfg;
	}

	// RconPassword is the configured password, or BattlEye's.
	string RconPassword()
	{
		string pw = password.Trim();
		if (pw != "")
		{
			ReCON_Log.Info("using the RCON password from " + FILE + " (" + pw.Length() + " characters)");
			return pw;
		}
		array<string> dirs = new array<string>();
		if (battlEyePath.Trim() != "")
			dirs.Insert(battlEyePath.Trim());
		string cli;
		if (GetCLIParam("BEpath", cli) && cli != "")
			dirs.Insert(cli);
		dirs.Insert("$profile:BattlEye");
		dirs.Insert("$profile:battleye");
		dirs.Insert("battleye");
		dirs.Insert("BattlEye");
		foreach (string dir : dirs)
		{
			pw = PasswordIn(dir);
			if (pw != "")
			{
				ReCON_Log.Info("using the RCON password from BattlEye's config in " + dir + " (" + pw.Length() + " characters; " + FILE + " has none)");
				return pw;
			}
		}
		return "";
	}

	// PasswordIn reads RConPassword from the BEServer_x64*.cfg files in dir
	// (BattlEye runs from a BEServer_x64_active_*.cfg copy).
	protected static string PasswordIn(string dir)
	{
		string name;
		FileAttr attr;
		FindFileHandle h = FindFile(dir + "/BEServer_x64*.cfg", name, attr, FindFileFlags.DIRECTORIES);
		string pw = "";
		if (name != "")
		{
			pw = PasswordInFile(dir + "/" + name);
			while (pw == "" && FindNextFile(h, name, attr))
				pw = PasswordInFile(dir + "/" + name);
		}
		CloseFindFile(h);
		return pw;
	}

	protected static string PasswordInFile(string path)
	{
		FileHandle f = OpenFile(path, FileMode.READ);
		if (!f)
			return "";
		string line;
		string pw = "";
		int lines = 0;
		while (FGets(f, line) >= 0 && lines < 1000)
		{
			lines++;
			line = line.Trim();
			string lower = line;
			lower.ToLower();
			if (lower.IndexOf("rconpassword ") == 0)
			{
				pw = line.Substring(13, line.Length() - 13).Trim();
				break;
			}
		}
		CloseFile(f);
		return pw;
	}
};

class ReCON_Log
{
	static void Info(string msg)
	{
		Print("[ReCON] " + msg);
	}
};
