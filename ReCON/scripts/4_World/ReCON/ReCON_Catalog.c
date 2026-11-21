// ReCON_Catalog is every item the server can spawn, read from the loaded
// config (DayZ's and every mod's): the public (scope 2) classes that are
// inventory items, weapons, magazines or vehicles (cars, motorbikes, boats:
// spawned whole, see ReCON_Actions.Spawn). Buildings aren't in it.
class ReCON_Catalog
{
	// PAGE is how many entries go in one sync.
	static const int PAGE = 1000;

	static void Read(array<ref ReCON_CatalogItem> list)
	{
		Section("CfgVehicles", list);
		Section("CfgWeapons", list);
		Section("CfgMagazines", list);
	}

	protected static void Section(string section, array<ref ReCON_CatalogItem> list)
	{
		int n = g_Game.ConfigGetChildrenCount(section);
		for (int i = 0; i < n; i++)
		{
			string type;
			if (!g_Game.ConfigGetChildName(section, i, type))
				continue;
			string path = section + " " + type;
			if (g_Game.ConfigGetInt(path + " scope") != 2)
				continue;
			string kind = KindOf(section, type);
			if (kind == "")
				continue;
			ReCON_CatalogItem it = new ReCON_CatalogItem();
			it.type = type;
			g_Game.ConfigGetText(path + " displayName", it.name);
			it.kind = kind;
			list.Insert(it);
		}
	}

	// KindOf is what the list files a class under; "" for one that can't be
	// spawned as an item.
	static string KindOf(string section, string type)
	{
		switch (section)
		{
			case "CfgWeapons":
				return "weapon";
			case "CfgMagazines":
				if (g_Game.IsKindOf(type, "Ammunition_Base"))
					return "ammo";
				return "magazine";
			case "CfgVehicles":
				if (g_Game.IsKindOf(type, "Transport"))
					return "vehicle";
				if (!g_Game.IsKindOf(type, "Inventory_Base"))
					return "";
				if (g_Game.IsKindOf(type, "Clothing"))
					return "clothing";
				return "item";
		}
		return "";
	}

	// Spawnable says whether type is a public item class (ReCON checks it is
	// in the list too; this guards against anything else).
	static bool Spawnable(string type)
	{
		TStringArray sections = {"CfgVehicles", "CfgWeapons", "CfgMagazines"};
		foreach (string section : sections)
		{
			string path = section + " " + type;
			if (g_Game.ConfigIsExisting(path))
				return g_Game.ConfigGetInt(path + " scope") == 2 && KindOf(section, type) != "";
		}
		return false;
	}
};
