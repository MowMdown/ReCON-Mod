// ReCON_Players reads a player's state for ReCON.
class ReCON_Players
{
	static ReCON_PlayerState State(PlayerBase p, map<string, int> kills, map<string, int> deaths)
	{
		ReCON_PlayerState s = new ReCON_PlayerState();
		PlayerIdentity id = p.GetIdentity();
		s.steamId = id.GetPlainId();
		s.name = id.GetName();

		vector pos = p.GetPosition();
		s.x = pos[0];
		s.y = pos[1];
		s.z = pos[2];
		vector dir = p.GetDirection();
		s.heading = Math.Atan2(dir[0], dir[2]) * Math.RAD2DEG;
		if (s.heading < 0)
			s.heading += 360;

		s.alive = p.IsAlive();
		s.health = p.GetHealth("", "Health");
		s.blood = p.GetHealth("", "Blood");
		s.shock = p.GetHealth("", "Shock");
		if (p.GetStatWater())
			s.water = p.GetStatWater().Get();
		if (p.GetStatEnergy())
			s.energy = p.GetStatEnergy().Get();
		if (p.GetStatHeatComfort())
			s.heat = p.GetStatHeatComfort().Get();
		if (p.GetStatHeatBuffer())
		{
			s.heatBuffer = Math.Max(0, p.GetStatHeatBuffer().Get());
			s.heatBufferMax = p.GetStatHeatBuffer().GetMax();
		}
		s.heatBufferCap = p.GetHeatBufferDynamicMax();
		s.heatBufferStage = p.GetHeatBufferStage();
		if (p.GetStatWet())
			s.wet = p.GetStatWet().Get();
		StaminaHandler stamina = p.GetStaminaHandler();
		if (stamina)
		{
			s.stamina = stamina.GetStamina();
			s.staminaMax = stamina.GetStaminaMax();
			s.staminaCap = stamina.GetStaminaCap();
		}
		else if (p.GetStatStamina())
			s.stamina = p.GetStatStamina().Get();
		s.bleeding = p.GetBleedingSourceCount();
		s.agents = p.GetAgents();
		s.brokenLegs = p.GetBrokenLegs();
		s.unconscious = p.IsUnconscious();
		s.restrained = p.IsRestrained();

		EntityAI hands = p.GetItemInHands();
		if (hands)
		{
			s.hands = hands.GetType();
			s.handsName = hands.GetDisplayName();
		}
		HumanCommandVehicle hcv = p.GetCommand_Vehicle();
		if (hcv && hcv.GetTransport())
		{
			s.vehicle = hcv.GetTransport().GetType();
			s.vehicleName = hcv.GetTransport().GetDisplayName();
			Transport tr = hcv.GetTransport();
			s.vehicleId = tr.GetID();
			s.seat = tr.CrewMemberIndex(p);
			s.driver = tr.CrewDriver() == p;
		}

		s.kills = kills.Get(s.steamId);
		s.deaths = deaths.Get(s.steamId);
		return s;
	}

	// BySteamId is the online player with that SteamID64, or null.
	static PlayerBase BySteamId(string steamId)
	{
		if (steamId == "")
			return null;
		array<Man> men = new array<Man>();
		g_Game.GetPlayers(men);
		foreach (Man man : men)
		{
			if (man.GetIdentity() && man.GetIdentity().GetPlainId() == steamId)
				return PlayerBase.Cast(man);
		}
		return null;
	}
};
