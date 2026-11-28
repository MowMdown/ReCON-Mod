// ReCON_MissionFiles reads mission files ReCON asks for (paths relative to the
// mission folder). The mod never writes mission files.
class ReCON_MissionFiles
{
	static ReCON_MissionFile Read(string name)
	{
		ReCON_MissionFile f = new ReCON_MissionFile();
		f.name = name;
		// Permit nested mission paths while preventing traversal outside the mission.
		if (name == "" || name.Contains("..") || name.Contains(":") || name.IndexOf("/") == 0 || name.Contains("\\") || name.Contains("//"))
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
