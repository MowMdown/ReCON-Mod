// ReCON_Actions runs one command from ReCON on the server.
class ReCON_Actions
{
	static ReCON_Result Run(ReCON_Command c)
	{
		ReCON_Result r = new ReCON_Result();
		r.id = c.id;
		if (ReCON_WorldControl.IsWorldAction(c.action))
		{
			r.error = ReCON_WorldControl.Run(c);
			r.ok = r.error == "";
			ReCON_Log.Info(c.action + ": " + ResultText(r));
			return r;
		}
		PlayerBase p = ReCON_Players.BySteamId(c.steamId);
		if (!p)
		{
			r.error = "the player isn't online";
			return r;
		}
		if (!p.IsAlive())
		{
			r.error = "the player is dead";
			return r;
		}
		string err;
		PlayerBase to;
		vector at;
		switch (c.action)
		{
			case "heal":
				Heal(p);
				break;
			case "kill":
				p.SetHealth(0);
				break;
			case "teleport":
				err = Teleport(p, c.x, c.y, c.z);
				break;
			case "teleportTo":
				to = ReCON_Players.BySteamId(c.target);
				if (!to)
					err = "the player to go to isn't online";
				else
				{
					at = to.GetPosition();
					err = Teleport(p, at[0] + 1, at[1], at[2] + 1);
				}
				break;
			case "set":
				err = SetStat(p, c.stat, c.value);
				break;
			case "message":
				err = Message(p, c.title, c.text, c.seconds);
				break;
			case "spawn":
				err = Spawn(p, c.text, c.value);
				break;
			default:
				err = "unknown action " + c.action;
				break;
		}
		r.ok = err == "";
		r.error = err;
		ReCON_Log.Info(c.action + " " + c.steamId + ": " + ResultText(r));
		return r;
	}

	protected static string ResultText(ReCON_Result r)
	{
		if (r.ok)
			return "done";
		return r.error;
	}

	// Heal restores health, blood, shock, food, water and stamina; stops
	// bleeding; cures illnesses and their symptoms; mends broken legs; and
	// wakes them if knocked out. A player another tool made invulnerable is
	// made vulnerable for the moment, or the health changes don't stick.
	// Modelled on Community Online Tools' heal (COT, by Jacob_Mango and
	// contributors, CC BY-SA 4.0; no COT code copied; see NOTICE).
	static void Heal(PlayerBase p)
	{
		bool allowDamage = p.GetAllowDamage();
		if (!allowDamage)
			p.SetAllowDamage(true);
		DamageSystem.ResetAllZones(p);
		if (p.GetStatWater())
			p.GetStatWater().Set(p.GetStatWater().GetMax());
		if (p.GetStatEnergy())
			p.GetStatEnergy().Set(p.GetStatEnergy().GetMax());
		if (p.GetStaminaHandler())
			p.GetStaminaHandler().SetStamina(GameConstants.STAMINA_MAX);
		if (p.GetBleedingManagerServer())
			p.GetBleedingManagerServer().RemoveAllSources();
		p.RemoveAllAgents();
		if (p.GetModifiersManager())
			p.GetModifiersManager().DeactivateAllModifiers(); // symptoms; they come back only if their cause does
		if (p.GetBrokenLegs() != eBrokenLegs.NO_BROKEN_LEGS)
			p.SetBrokenLegs(eBrokenLegs.NO_BROKEN_LEGS);
		if (p.IsUnconscious())
			DayZPlayerSyncJunctures.SendPlayerUnconsciousness(p, false);
		if (!allowDamage)
			p.SetAllowDamage(false);
	}

	// Spawn puts count of type on the ground a metre or so in front of
	// them, a little apart, with full stacks and magazines.
	static string Spawn(PlayerBase p, string type, float count)
	{
		if (!ReCON_Catalog.Spawnable(type))
			return type + " isn't an item this server can spawn";
		int n = (int)count;
		if (n < 1)
			n = 1;
		if (n > 20)
			n = 20;
		vector pos = p.GetPosition();
		vector dir = p.GetDirection();
		dir[1] = 0;
		dir.Normalize();
		vector side = Vector(dir[2], 0, -dir[0]);
		for (int i = 0; i < n; i++)
		{
			// A row across in front of them, 0.4 m apart.
			float off = (i - (n - 1) / 2.0) * 0.4;
			vector at = pos + dir * 1.2 + side * off;
			at[1] = g_Game.SurfaceY(at[0], at[2]);
			EntityAI e = EntityAI.Cast(g_Game.CreateObjectEx(type, at, ECE_PLACE_ON_SURFACE));
			if (!e)
				return "the game couldn't create " + type;
			Magazine mag = Magazine.Cast(e);
			ItemBase ib = ItemBase.Cast(e);
			if (mag)
				mag.ServerSetAmmoMax();
			else if (ib && ib.HasQuantity())
				ib.SetQuantityMax();
		}
		return "";
	}

	// Teleport moves them (and their vehicle) to x, z; y 0 or below means
	// ground level there.
	static string Teleport(PlayerBase p, float x, float y, float z)
	{
		if (x <= 0 || z <= 0)
			return "no position";
		if (y <= 0)
			y = g_Game.SurfaceY(x, z);
		DeveloperTeleport.SetPlayerPosition(p, Vector(x, y, z));
		return "";
	}

	static string SetStat(PlayerBase p, string stat, float value)
	{
		if (value < 0)
			value = 0;
		switch (stat)
		{
			case "health":
				p.SetHealth("", "Health", Math.Min(value, p.GetMaxHealth("", "Health")));
				return "";
			case "blood":
				p.SetHealth("", "Blood", Math.Min(value, p.GetMaxHealth("", "Blood")));
				return "";
			case "water":
				if (!p.GetStatWater())
					return "no water stat";
				p.GetStatWater().Set(Math.Min(value, p.GetStatWater().GetMax()));
				return "";
			case "energy":
				if (!p.GetStatEnergy())
					return "no food stat";
				p.GetStatEnergy().Set(Math.Min(value, p.GetStatEnergy().GetMax()));
				return "";
			case "heatBuffer":
				if (!p.GetStatHeatBuffer())
					return "no heat buff stat";
				p.GetStatHeatBuffer().Set(Math.Min(value, p.GetStatHeatBuffer().GetMax()));
				return "";
		}
		return "unknown stat " + stat;
	}

	static string Message(PlayerBase p, string title, string text, float seconds)
	{
		if (text == "")
			return "no message";
		if (title == "")
			title = "Server";
		if (seconds <= 0)
			seconds = 8;
		NotificationSystem.SendNotificationToPlayerIdentityExtended(p.GetIdentity(), seconds, title, text);
		return "";
	}
};
