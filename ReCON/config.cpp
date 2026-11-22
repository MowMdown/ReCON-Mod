class CfgPatches
{
	class ReCON_Mod
	{
		units[] = {};
		weapons[] = {};
		requiredVersion = 0.1;
		requiredAddons[] = {"DZ_Data", "DZ_Scripts", "JM_CF_Scripts"};
	};
};

class CfgMods
{
	class ReCON_Mod
	{
		dir = "ReCON";
		name = "ReCON";
		author = "MowMdown";
		// Monetized servers need Bohemia Interactive's approval: see NOTICE.
		version = "1.0.3";
		// CF mod storage: who placed an object is saved with it (ReCON_Placed.c).
		storageVersion = 1;
		type = "mod";
		dependencies[] = {"Game", "World"};

		class defs
		{
			class gameScriptModule
			{
				value = "";
				files[] = {"ReCON/scripts/3_Game"};
			};
			class worldScriptModule
			{
				value = "";
				files[] = {"ReCON/scripts/4_World"};
			};
		};
	};
};
