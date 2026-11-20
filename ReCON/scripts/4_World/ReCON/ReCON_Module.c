// ReCON_Module runs on the server only. Every intervalMs it sends ReCON the
// state of every online player and runs the commands ReCON answers with.
// One request is in flight at a time: a slow ReCON slows the syncs down
// rather than piling them up.
[CF_RegisterModule(ReCON_Module)]
class ReCON_Module : CF_ModuleWorld
{
	protected ref ReCON_Config m_Config;
	protected string m_Password;
	protected RestContext m_Rest;
	protected ref ReCON_Callback m_Callback;
	protected ref JsonSerializer m_Json;

	protected bool m_Enabled;
	protected bool m_InFlight;
	protected float m_SinceSync;    // seconds
	protected float m_SinceStart;   // seconds
	protected float m_InFlightFor;  // seconds
	protected int m_IntervalMs;
	protected bool m_LastFailed;

	// Events and command results waiting for the next sync; the ones in
	// the request in flight are kept until ReCON answers, then dropped (or
	// sent again if it didn't).
	protected ref array<ref ReCON_Event> m_Events = new array<ref ReCON_Event>();
	protected ref array<ref ReCON_Result> m_Results = new array<ref ReCON_Result>();
	protected ref array<ref ReCON_Event> m_SentEvents = new array<ref ReCON_Event>();
	protected ref array<ref ReCON_Result> m_SentResults = new array<ref ReCON_Result>();

	// Kills and deaths this session, by SteamID64.
	protected ref map<string, int> m_Kills = new map<string, int>();
	protected ref map<string, int> m_Deaths = new map<string, int>();

	static const int MAX_QUEUED = 200;

	static ReCON_Module Get()
	{
		return CF_Modules<ReCON_Module>.Get();
	}

	override void OnInit()
	{
		super.OnInit();
		if (!g_Game.IsDedicatedServer())
			return;
		EnableMissionStart();
		EnableUpdate();
		EnableClientDisconnect();
	}

	override void OnMissionStart(Class sender, CF_EventArgs args)
	{
		super.OnMissionStart(sender, args);
		if (!g_Game.IsDedicatedServer())
			return;
		m_Config = ReCON_Config.Load();
		m_IntervalMs = m_Config.intervalMs;
		m_Password = m_Config.RconPassword();
		if (m_Password == "")
		{
			ReCON_Log.Info("no RCON password: put it in " + ReCON_Config.FILE + " (\"password\"), or set \"battlEyePath\" to the folder with BEServer_x64.cfg. ReCON won't get any data until then.");
			return;
		}
		RestApi api = GetRestApi();
		if (!api)
			api = CreateRestApi();
		if (!api)
		{
			ReCON_Log.Info("the server's REST API isn't available");
			return;
		}
		api.SetOption(ERestOption.ERESTOPTION_READOPERATION, 5);
		api.SetOption(ERestOption.ERESTOPTION_CONNECTION, 5);
		m_Rest = api.GetRestContext(m_Config.url);
		m_Rest.SetHeader("application/json");
		m_Callback = new ReCON_Callback(this);
		m_Json = new JsonSerializer();
		m_Enabled = true;
		ReCON_Log.Info("sending player data to " + m_Config.url + " every " + m_IntervalMs + " ms");
	}

	override void OnUpdate(Class sender, CF_EventArgs args)
	{
		super.OnUpdate(sender, args);
		if (!m_Enabled)
			return;
		CF_EventUpdateArgs update = CF_EventUpdateArgs.Cast(args);
		float dt = 0;
		if (update)
			dt = update.DeltaTime;
		m_SinceStart += dt;
		if (m_InFlight)
		{
			// The REST API times out on its own; this is a safety net in
			// case a callback never comes.
			m_InFlightFor += dt;
			if (m_InFlightFor < 15)
				return;
			ReCON_Log.Info("no answer from ReCON in 15 s; sending again");
			OnFailed();
		}
		m_SinceSync += dt;
		if (m_SinceSync * 1000 < m_IntervalMs)
			return;
		m_SinceSync = 0;
		Send();
	}

	override void OnClientDisconnect(Class sender, CF_EventArgs args)
	{
		super.OnClientDisconnect(sender, args);
		CF_EventPlayerDisconnectedArgs dc = CF_EventPlayerDisconnectedArgs.Cast(args);
		if (dc && dc.UID != "")
		{
			m_Kills.Remove(dc.UID);
			m_Deaths.Remove(dc.UID);
		}
	}

	protected void Send()
	{
		ReCON_Sync sync = new ReCON_Sync();
		sync.key = m_Password;
		g_Game.GetWorldName(sync.world);
		sync.world.ToLower();
		g_Game.GetWorld().GetDate(sync.year, sync.month, sync.day, sync.hour, sync.minute);
		ReCON_WorldControl.Read(sync.weather);

		array<Man> men = new array<Man>();
		g_Game.GetPlayers(men);
		foreach (Man man : men)
		{
			PlayerBase p = PlayerBase.Cast(man);
			if (!p || !p.GetIdentity())
				continue;
			sync.players.Insert(ReCON_Players.State(p, m_Kills, m_Deaths));
		}

		// Events and results not yet answered go again, ahead of the new ones.
		foreach (ReCON_Event e : m_Events)
			m_SentEvents.Insert(e);
		m_Events.Clear();
		foreach (ReCON_Result r : m_Results)
			m_SentResults.Insert(r);
		m_Results.Clear();
		while (m_SentEvents.Count() > MAX_QUEUED)
			m_SentEvents.Remove(0);
		while (m_SentResults.Count() > MAX_QUEUED)
			m_SentResults.Remove(0);
		sync.events = m_SentEvents;
		sync.results = m_SentResults;

		string body;
		if (!m_Json.WriteToString(sync, false, body))
		{
			ReCON_Log.Info("couldn't write the player data as JSON");
			return;
		}
		m_InFlight = true;
		m_InFlightFor = 0;
		m_Rest.POST(m_Callback, "api/mod/sync", body);
	}

	void OnAnswer(string data)
	{
		if (!m_InFlight)
			return; // answered after the safety net gave up on it
		m_InFlight = false;
		ReCON_Reply reply = new ReCON_Reply();
		string err;
		if (!m_Json.ReadFromString(reply, data, err))
		{
			ReCON_Log.Info("ReCON's answer isn't valid JSON: " + err);
			OnFailed();
			return;
		}
		if (!reply.ok)
		{
			if (!m_LastFailed)
				ReCON_Log.Info("ReCON refused the data: " + reply.error);
			m_LastFailed = true;
			KeepUnsent();
			return;
		}
		if (m_LastFailed)
			ReCON_Log.Info("ReCON is getting the data again");
		m_LastFailed = false;
		m_SentEvents.Clear();
		m_SentResults.Clear();
		if (reply.interval >= 250)
			m_IntervalMs = reply.interval;
		if (reply.commands)
		{
			foreach (ReCON_Command c : reply.commands)
				m_Results.Insert(ReCON_Actions.Run(c));
		}
	}

	void OnFailed()
	{
		m_InFlight = false;
		if (!m_LastFailed)
			ReCON_Log.Info("can't reach ReCON at " + m_Config.url + "; trying again every " + m_IntervalMs + " ms");
		m_LastFailed = true;
		KeepUnsent();
	}

	// KeepUnsent puts what the failed request carried back in the queues.
	protected void KeepUnsent()
	{
		foreach (ReCON_Event e : m_Events)
			m_SentEvents.Insert(e);
		m_Events = m_SentEvents;
		m_SentEvents = new array<ref ReCON_Event>();
		foreach (ReCON_Result r : m_Results)
			m_SentResults.Insert(r);
		m_Results = m_SentResults;
		m_SentResults = new array<ref ReCON_Result>();
	}

	// OnKilled is called by PlayerBase.EEKilled on the server.
	void OnKilled(PlayerBase victim, Object killer)
	{
		if (!m_Enabled)
			return;
		ReCON_Event e = new ReCON_Event();
		e.type = "kill";
		e.at = (int)m_SinceStart;
		vector pos = victim.GetPosition();
		e.x = pos[0];
		e.y = pos[1];
		e.z = pos[2];
		if (victim.GetIdentity())
		{
			e.victim = victim.GetIdentity().GetPlainId();
			e.victimName = victim.GetIdentity().GetName();
			m_Deaths.Set(e.victim, m_Deaths.Get(e.victim) + 1);
		}

		PlayerBase by;
		EntityAI weapon;
		if (killer)
		{
			EntityAI ent = EntityAI.Cast(killer);
			if (ent)
			{
				by = PlayerBase.Cast(ent.GetHierarchyRootPlayer());
				if (ent != by)
					weapon = ent;
			}
			if (!by)
			{
				// A vehicle: its driver.
				Transport t = Transport.Cast(killer);
				if (t)
				{
					by = PlayerBase.Cast(t.CrewMember(0));
					weapon = t;
				}
			}
		}
		if (by && !weapon && by.GetItemInHands())
			weapon = by.GetItemInHands();
		if (weapon)
			e.weapon = weapon.GetType();
		else if (killer)
			e.weapon = killer.GetType();
		if (by && by != victim && by.GetIdentity())
		{
			e.killer = by.GetIdentity().GetPlainId();
			e.killerName = by.GetIdentity().GetName();
			e.distance = vector.Distance(by.GetPosition(), pos);
			m_Kills.Set(e.killer, m_Kills.Get(e.killer) + 1);
		}
		else if (by == victim)
		{
			e.killer = e.victim; // suicide
			e.killerName = e.victimName;
		}
		m_Events.Insert(e);
		while (m_Events.Count() > MAX_QUEUED)
			m_Events.Remove(0);
	}
};

class ReCON_Callback : RestCallback
{
	protected ReCON_Module m_Module;

	void ReCON_Callback(ReCON_Module module)
	{
		m_Module = module;
	}

	override void OnSuccess(string data, int dataSize)
	{
		m_Module.OnAnswer(data);
	}

	override void OnError(int errorCode)
	{
		m_Module.OnFailed();
	}

	override void OnTimeout()
	{
		m_Module.OnFailed();
	}
};
