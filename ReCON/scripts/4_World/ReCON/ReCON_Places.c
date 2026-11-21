// ReCON_Places reads the map's place names (towns, villages, hills, bays...)
// from its config, CfgWorlds <world> Names, for the Live Map.
class ReCON_Places
{
	static void Read(array<ref ReCON_Place> list)
	{
		string path = "CfgWorlds " + g_Game.GetWorldName() + " Names";
		int n = g_Game.ConfigGetChildrenCount(path);
		for (int i = 0; i < n; i++)
		{
			string cls;
			if (!g_Game.ConfigGetChildName(path, i, cls))
				continue;
			string at = path + " " + cls;
			ReCON_Place pl = new ReCON_Place();
			g_Game.ConfigGetText(at + " name", pl.name);
			g_Game.ConfigGetText(at + " type", pl.type);
			if (pl.name.IndexOf("$") == 0)
				pl.name = Widget.TranslateString("#" + pl.name.Substring(1, pl.name.Length() - 1));
			TFloatArray pos = new TFloatArray();
			g_Game.ConfigGetFloatArray(at + " position", pos);
			if (pl.name == "" || pos.Count() < 2)
				continue;
			pl.x = pos[0];
			pl.z = pos[1];
			list.Insert(pl);
		}
	}
};
