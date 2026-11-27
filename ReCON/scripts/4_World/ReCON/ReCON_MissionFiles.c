// ReCON_MissionFiles is the mission filesystem bridge used by ReCON.
// It is deliberately separate from event-spot detection and economy logic.
// All paths are relative to the active DayZ mission directory.
// ReCON_MissionFiles reads a mission file ReCON asked for, as text.
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

	// Write replaces an existing mission XML file only. Requiring the file
	// to exist prevents this transport API from becoming a directory writer.
	// Paths are mission-relative and may contain subdirectories (for example db/types.xml).
	static ReCON_MissionWriteResult Write(ReCON_MissionWrite request)
	{
		ReCON_MissionWriteResult result = new ReCON_MissionWriteResult();
		if (!request)
		{
			result.error = "missing write request";
			return result;
		}
		result.id = request.id;
		if (request.id <= 0 || request.name == "" || request.text.Length() > 2097152)
		{
			result.error = "invalid write request or file exceeds 2 MiB";
			return result;
		}
		string name = request.name;
		if (name.Contains("..") || name.Contains(":") || name.IndexOf("/") == 0 || name.Contains("\\") || name.Contains("//") || name.Length() < 4 || name.Substring(name.Length() - 4, 4).ToLower() != ".xml")
		{
			result.error = "unsafe mission path (XML files only)";
			return result;
		}
		string filePath = "$mission:" + name;
		if (!FileExist(filePath))
		{
			result.error = "mission file does not exist; writes cannot create files";
			return result;
		}
		FileHandle handle = OpenFile(filePath, FileMode.WRITE);
		if (!handle)
		{
			result.error = "could not open mission file for writing";
			return result;
		}
		FPrint(handle, request.text);
		CloseFile(handle);
		result.ok = true;
		return result;
	}
};
