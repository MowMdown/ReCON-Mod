// ReCON_EventSpots checks the places events can spawn (ReCON works them out
// from the mission's files and sends them: the reply's spotChecks): a few
// each sync, each up when an object of one of its types is near it.
class ReCON_EventSpots
{
	protected static ref array<ref ReCON_SpotCheck> s_Checks = new array<ref ReCON_SpotCheck>();
	protected static int s_Next;
	static const int PER_SYNC = 40;

	static void Set(array<ref ReCON_SpotCheck> checks)
	{
		s_Checks.Clear();
		s_Next = 0;
		if (!checks)
			return;
		foreach (ReCON_SpotCheck c : checks)
			s_Checks.Insert(c);
	}

	static int Count()
	{
		return s_Checks.Count();
	}

	// Check looks at the next PER_SYNC spots and adds whether each is up.
	static void Check(array<ref ReCON_SpotState> list)
	{
		int n = s_Checks.Count();
		if (n == 0)
			return;
		int todo = PER_SYNC;
		if (todo > n)
			todo = n;
		array<Object> found = new array<Object>();
		for (int i = 0; i < todo; i++)
		{
			if (s_Next >= n)
				s_Next = 0;
			ReCON_SpotCheck c = s_Checks[s_Next];
			s_Next++;
			ReCON_SpotState st = new ReCON_SpotState();
			st.id = c.id;
			found.Clear();
			vector at = Vector(c.x, g_Game.SurfaceY(c.x, c.z), c.z);
			g_Game.GetObjectsAtPosition(at, c.radius, found, null);
			foreach (Object o : found)
			{
				if (o && c.types.Find(o.GetType()) >= 0)
				{
					st.on = true;
					break;
				}
			}
			list.Insert(st);
		}
	}
};

// ReCON_MissionFiles reads a mission file ReCON asked for, as text.
class ReCON_MissionFiles
{
	static ReCON_MissionFile Read(string name)
	{
		ReCON_MissionFile f = new ReCON_MissionFile();
		f.name = name;
		// Only plain names in the mission folder: no going up out of it.
		if (name.Contains("..") || name.Contains(":") || name.IndexOf("/") == 0 || name.Contains("\\"))
			return f;
		FileHandle h = OpenFile("$mission:" + name, FileMode.READ);
		if (h)
		{
			f.found = true;
			string line = "";
			while (FGets(h, line) >= 0)
			{
				f.text += line + "\n";
				line = "";
			}
			CloseFile(h);
		}
		return f;
	}
};
