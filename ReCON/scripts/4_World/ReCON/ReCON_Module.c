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
	// When the vehicles were last sent (m_SinceStart), and how often.
	protected float m_VehiclesAt = -1000;
	// The vehicles ReCON wants to see inside (by id).
	protected ref array<int> m_WatchVehicles = new array<int>();
	// When the placed objects were last sent, and the ones ReCON wants to see inside.
	protected float m_PlacedAt = -1000;
	protected ref array<int> m_WatchPlaced = new array<int>();
	static const float PLACED_EVERY = 30;
	// ReCON wants the map's place names in the next sync.
	protected bool m_WantPlaces;
	static const float VEHICLES_EVERY = 10;
	// The vehicles that had someone in them on the last sync (by id).
	protected ref array<int> m_Crewed = new array<int>();
	// Mission files ReCON asked for, sent in the next sync.
	protected ref array<string> m_WantMission = new array<string>();
	protected bool m_LastFailed;
	protected string m_LastWhy; // the last failure logged, so a new reason is logged too

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

	// The players ReCON is watching (SteamID64s, from the last reply): their
	// inventory goes in the syncs.
	protected ref set<string> m_Watch = new set<string>();

	// Every item the server can spawn (read once), and 1 + the entry ReCON
	// wants next (0: it has them all).
	protected ref array<ref ReCON_CatalogItem> m_Catalog = new array<ref ReCON_CatalogItem>();
	protected int m_CatalogNext;

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
			ReCON_Log.Info("no RCON password: put it in " + ReCON_Config.FILE + " (\"password\"), or set \"battlEyePath\" to the folder with BattlEye's BEServer_x64_active_*.cfg. ReCON won't get any data until then.");
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
		ReCON_Catalog.Read(m_Catalog);
		ReCON_Log.Info(m_Catalog.Count().ToString() + " items can be spawned");
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
			OnFailed("no answer from ReCON at " + m_Config.url + " in 15 s");
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
			ReCON_PlayerState st = ReCON_Players.State(p, m_Kills, m_Deaths);
			if (m_Watch.Find(st.steamId) >= 0)
			{
				ReCON_Inventory.Read(p, st.inventory);
				st.inv = true;
			}
			sync.players.Insert(st);
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

		// The vehicles every VEHICLES_EVERY seconds.
		if (m_SinceStart - m_VehiclesAt >= VEHICLES_EVERY)
		{
			m_VehiclesAt = m_SinceStart;
			sync.vehiclesSent = true;
			ReCON_Vehicles.Read(sync.vehicles);
			ReCON_WorldEvents.Read(sync.worldEvents);
		}
		// The predators every sync: they move.
		sync.predatorsSent = true;
		ReCON_Predators.Read(sync.predators);
		sync.crewSent = true;
		ReCON_Vehicles.ReadCrewed(sync.crewVehicles, m_Crewed);
		foreach (string mf : m_WantMission)
			sync.missionFiles.Insert(ReCON_MissionFiles.Read(mf));
		m_WantMission.Clear();
		sync.spotsHeld = ReCON_EventSpots.Count();
		ReCON_EventSpots.Check(sync.spots);
		if (m_WantPlaces)
		{
			m_WantPlaces = false;
			sync.placesSent = true;
			ReCON_Places.Read(sync.places);
		}
		if (m_SinceStart - m_PlacedAt >= PLACED_EVERY)
		{
			m_PlacedAt = m_SinceStart;
			sync.placedSent = true;
			ReCON_PlacedObjects.Read(sync.placed);
		}
		foreach (int oid : m_WatchPlaced)
		{
			ReCON_PlacedDetail od = ReCON_PlacedObjects.Detail(oid);
			if (od)
				sync.placedDetails.Insert(od);
		}
		foreach (int vid : m_WatchVehicles)
		{
			ReCON_VehicleDetail vd = ReCON_Vehicles.Detail(vid);
			if (vd)
				sync.vehicleDetails.Insert(vd);
		}

		sync.catalogTotal = m_Catalog.Count();
		if (m_CatalogNext > 0)
		{
			sync.catalogAt = m_CatalogNext - 1;
			for (int i = sync.catalogAt; i < m_Catalog.Count() && i < sync.catalogAt + ReCON_Catalog.PAGE; i++)
				sync.catalog.Insert(m_Catalog[i]);
		}

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
			int shown = data.Length();
			if (shown > 200)
				shown = 200;
			OnFailed("ReCON's answer isn't valid JSON (" + err + "): " + data.Substring(0, shown));
			return;
		}
		if (!reply.ok)
		{
			OnFailed("ReCON refused the data: " + reply.error);
			return;
		}
		if (m_LastFailed)
			ReCON_Log.Info("ReCON is getting the data again");
		m_LastFailed = false;
		m_LastWhy = "";
		m_SentEvents.Clear();
		m_SentResults.Clear();
		if (reply.interval >= 250)
			m_IntervalMs = reply.interval;
		m_CatalogNext = reply.catalogNext;
		m_Watch.Clear();
		if (reply.watch)
		{
			foreach (string w : reply.watch)
				m_Watch.Insert(w);
		}
		m_WantPlaces = reply.wantPlaces;
		m_WantMission.Clear();
		if (reply.wantMission)
		{
			foreach (string wm : reply.wantMission)
				m_WantMission.Insert(wm);
		}
		if (reply.spotsSet)
			ReCON_EventSpots.Set(reply.spotChecks);
		m_WatchVehicles.Clear();
		if (reply.watchVehicles)
		{
			foreach (int wv : reply.watchVehicles)
				m_WatchVehicles.Insert(wv);
		}
		m_WatchPlaced.Clear();
		if (reply.watchPlaced)
		{
			foreach (int wp : reply.watchPlaced)
				m_WatchPlaced.Insert(wp);
		}
		if (reply.commands)
		{
			foreach (ReCON_Command c : reply.commands)
				m_Results.Insert(ReCON_Actions.Run(c));
		}
	}

	// OnFailed logs why a sync failed (once, until the reason changes or a
	// sync works again) and keeps what it carried for the next one.
	void OnFailed(string why)
	{
		m_InFlight = false;
		if (why != m_LastWhy)
			ReCON_Log.Info(why + "; trying again every " + m_IntervalMs + " ms");
		m_LastFailed = true;
		m_LastWhy = why;
		KeepUnsent();
	}

	// OnRestError: DayZ's REST API reports an HTTP error answer (ReCON
	// refusing the data) the same way as no answer at all, as an
	// ERestResultState; name it so the log tells them apart.
	void OnRestError(int code)
	{
		string what = "error " + code.ToString();
		switch (code)
		{
			case ERestResultState.EREST_ERROR:
				what = "the request failed (EREST_ERROR): either nothing answered at that address and port, or ReCON refused the data (then ReCON's log says why)";
				break;
			case ERestResultState.EREST_ERROR_CLIENTERROR:
				what = "ReCON answered with an error (EREST_ERROR_CLIENTERROR): wrong password, or it couldn't read the data; ReCON's log says which";
				break;
			case ERestResultState.EREST_ERROR_SERVERERROR:
				what = "the address answered with a server error (EREST_ERROR_SERVERERROR): is it ReCON, or a proxy in front of it?";
				break;
			case ERestResultState.EREST_ERROR_APPERROR:
				what = "EREST_ERROR_APPERROR";
				break;
			case ERestResultState.EREST_ERROR_TIMEOUT:
				what = "no answer in time (EREST_ERROR_TIMEOUT)";
				break;
		}
		OnFailed("can't sync with ReCON at " + m_Config.url + ": " + what);
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
			// What they had as they died, before the body can be looted.
			ReCON_Inventory.Read(victim, e.inventory);
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
		e.cause = Cause(victim, killer, by);
		e.agents = victim.GetAgents();
		// What they were killed with: never their own body (a death no one
		// caused passes the victim as the killer), and what is in their
		// hands only for a player who killed them, or for a suicide.
		if (by && !weapon && by.GetItemInHands() && (by != victim || e.cause == "suicide"))
			weapon = by.GetItemInHands();
		if (weapon)
			e.weapon = weapon.GetType();
		else if (killer && killer != victim)
			e.weapon = killer.GetType();
		if (by && by != victim && by.GetIdentity())
		{
			e.killer = by.GetIdentity().GetPlainId();
			e.killerName = by.GetIdentity().GetName();
			e.distance = vector.Distance(by.GetPosition(), pos);
			m_Kills.Set(e.killer, m_Kills.Get(e.killer) + 1);
		}
		else if (e.cause == "suicide")
		{
			e.killer = e.victim;
			e.killerName = e.victimName;
		}
		m_Events.Insert(e);
		while (m_Events.Count() > MAX_QUEUED)
			m_Events.Remove(0);
	}

	// Cause is what killed the victim (see ReCON_Event.cause). killer is
	// what DayZ passed to EEKilled, by the player behind it, if any. With
	// no one else behind it, it goes by what last hurt them (a fall, a
	// fire) if that was just now, then by the state they died in, the way
	// DayZ's own modifiers kill: drowning, blood below the fatal level,
	// thirst, hunger, cold, illness.
	string Cause(PlayerBase victim, Object killer, PlayerBase by)
	{
		if (victim.m_ReCON_AdminKill)
			return "admin";
		if (victim.CommitedSuicide())
			return "suicide";
		if (killer && killer.IsTransport())
			return "vehicle";
		if (by && by != victim)
			return "player";
		if (killer && killer != victim)
		{
			EntityAI ent = EntityAI.Cast(killer);
			if (ent && ent.IsZombie())
				return "infected";
			if (ent && ent.IsAnimal())
				return "animal";
			if (LandMineTrap.Cast(killer) || ExplosivesBase.Cast(killer) || killer.IsExplosive())
				return "explosion";
			if (TrapBase.Cast(killer))
				return "trap";
		}
		if (g_Game.GetTime() - victim.m_ReCON_LastHitAt < 3000)
		{
			string ammo = victim.m_ReCON_LastAmmo;
			if (ammo.Contains("FallDamage"))
				return "fall";
			if (ammo == "FireDamage" || ammo == "HeatDamage")
				return "fire";
		}
		if (victim.GetDrowningWaterLevelCheck())
			return "drowned";
		if (victim.GetHealth("GlobalHealth", "Blood") < PlayerConstants.BLOOD_THRESHOLD_FATAL)
			return "bleeding";
		if (victim.GetStatWater() && victim.GetStatWater().Get() <= PlayerConstants.LOW_WATER_THRESHOLD)
			return "dehydration";
		if (victim.GetStatEnergy() && victim.GetStatEnergy().Get() <= PlayerConstants.LOW_ENERGY_THRESHOLD)
			return "starvation";
		if (victim.GetStatHeatComfort() && victim.GetStatHeatComfort().Get() < PlayerConstants.ENERGY_LOSS_THRESHOLD_HC_MINUS_HIGH)
			return "cold";
		if (victim.GetAgents() != 0)
			return "illness";
		BleedingSourcesManagerServer bleeding = victim.GetBleedingManagerServer();
		if (bleeding && bleeding.GetBleedingSourcesCount() > 0)
			return "bleeding";
		return "";
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
		m_Module.OnRestError(errorCode);
	}

	override void OnTimeout()
	{
		m_Module.OnFailed("no answer from ReCON in time (timeout)");
	}
};
