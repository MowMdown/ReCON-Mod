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
			case "dry":
				Dry(p);
				break;
			case "kill":
				p.m_ReCON_AdminKill = true;
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
				err = Spawn(p, c.text, c.value, c.stat);
				break;
			case "condition":
				err = SetCondition(p, c.text.ToInt(), (int)c.value);
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

	// Dry makes them and everything they carry (clothes first of all: wet
	// clothes are what chill them) bone dry.
	static void Dry(PlayerBase p)
	{
		if (p.GetStatWet())
			p.GetStatWet().Set(0);
		array<EntityAI> items = new array<EntityAI>();
		p.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, items);
		EntityAI hands = p.GetItemInHands();
		if (hands)
		{
			array<EntityAI> held = new array<EntityAI>();
			hands.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, held);
			items.InsertAll(held);
		}
		foreach (EntityAI e : items)
		{
			ItemBase ib = ItemBase.Cast(e);
			if (ib)
				ib.SetWet(ib.GetWetMin());
		}
	}

	// Spawn makes count of type (full stacks and magazines) where says:
	// "hands" (one, into empty hands), "inventory" (anywhere free in their
	// clothing and containers; what doesn't fit goes on the ground) or, by
	// default, on the ground a metre or so in front of them, a little apart.
	// A vehicle is one, whole, 8 m in front of them and facing their way.
	static string Spawn(PlayerBase p, string type, float count, string where)
	{
		if (!ReCON_Catalog.Spawnable(type))
			return type + " isn't an item this server can spawn";
		if (g_Game.IsKindOf(type, "Transport"))
			return SpawnVehicle(p, type);
		int n = (int)count;
		if (n < 1)
			n = 1;
		if (n > 20)
			n = 20;
		EntityAI e;
		if (where == "hands")
		{
			if (p.GetItemInHands())
				return "their hands aren't empty";
			e = p.GetHumanInventory().CreateInHands(type);
			if (!e)
				return "the game couldn't put " + type + " in their hands";
			FillUp(e);
			return "";
		}
		for (int i = 0; i < n; i++)
		{
			e = null;
			if (where == "inventory")
				e = InInventory(p, type);
			if (!e)
				e = OnGround(p, type, i, n);
			if (!e)
				return "the game couldn't create " + type;
			FillUp(e);
		}
		return "";
	}

	// OnGround puts one of type on the ground in front of them, the i-th of
	// a row of n across, 0.4 m apart.
	static EntityAI OnGround(PlayerBase p, string type, int i, int n)
	{
		vector dir = p.GetDirection();
		dir[1] = 0;
		dir.Normalize();
		vector side = Vector(dir[2], 0, -dir[0]);
		float off = (i - (n - 1) / 2.0) * 0.4;
		vector at = p.GetPosition() + dir * 1.2 + side * off;
		at[1] = g_Game.SurfaceY(at[0], at[2]);
		return EntityAI.Cast(g_Game.CreateObjectEx(type, at, ECE_PLACE_ON_SURFACE));
	}

	// InInventory puts one of type somewhere free in their clothing and
	// containers, the way GameInventory.CreateInInventory does, but never
	// into a gun's magazine well (a magazine made there skips the gun's
	// loading); null when there is no room.
	static EntityAI InInventory(PlayerBase p, string type)
	{
		GameInventory inv = p.GetInventory();
		InventoryLocation loc = new InventoryLocation();
		bool found = inv.FindFirstFreeLocationForNewEntity(type, FindInventoryLocationType.CARGO | FindInventoryLocationType.ATTACHMENT, loc);
		if (found && Weapon_Base.Cast(loc.GetParent()))
			found = inv.FindFirstFreeLocationForNewEntity(type, FindInventoryLocationType.CARGO, loc);
		if (!found)
			return null;
		if (loc.GetType() == InventoryLocationType.ATTACHMENT)
			return loc.GetParent().GetInventory().CreateAttachmentEx(type, loc.GetSlot());
		if (loc.GetType() == InventoryLocationType.CARGO)
			return loc.GetParent().GetInventory().CreateEntityInCargoEx(type, loc.GetIdx(), loc.GetRow(), loc.GetCol(), loc.GetFlip());
		return null;
	}

	// FillUp gives a magazine all its rounds and a stack its full quantity.
	static void FillUp(EntityAI e)
	{
		Magazine mag = Magazine.Cast(e);
		ItemBase ib = ItemBase.Cast(e);
		if (mag)
			mag.ServerSetAmmoMax();
		else if (ib && ib.HasQuantity())
			ib.SetQuantityMax();
	}

	// SetCondition sets the condition of the item with that id (ReCON_Item.id)
	// that they carry: in their hands, worn, or inside either. level is
	// 0 pristine, 1 worn, 2 damaged, 3 badly damaged, 4 ruined, set to the
	// health DayZ itself gives an item it steps down to that level.
	static string SetCondition(PlayerBase p, int id, int level)
	{
		// Everything on them, then everything in their hands (in a list of
		// its own: EnumerateInventory fills the list it is given afresh).
		array<EntityAI> items = new array<EntityAI>();
		p.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, items);
		EntityAI hands = p.GetItemInHands();
		if (hands)
		{
			array<EntityAI> held = new array<EntityAI>();
			hands.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, held);
			items.InsertAll(held);
		}
		EntityAI item;
		foreach (EntityAI e : items)
		{
			if (e && e != p && e.GetID() == id)
			{
				item = e;
				break;
			}
		}
		if (!item)
			return "they aren't carrying that item any more";
		float frac;
		switch (level)
		{
			case 0:
				frac = GameConstants.DAMAGE_PRISTINE_VALUE;
				break;
			case 1:
				frac = GameConstants.DAMAGE_WORN_VALUE;
				break;
			case 2:
				frac = GameConstants.DAMAGE_DAMAGED_VALUE;
				break;
			case 3:
				frac = GameConstants.DAMAGE_BADLY_DAMAGED_VALUE;
				break;
			case 4:
				frac = GameConstants.DAMAGE_RUINED_VALUE;
				break;
			default:
				return "no such condition";
		}
		item.SetHealth("", "", item.GetMaxHealth("", "") * frac);
		return "";
	}

	// SpawnVehicle puts one of type 8 m in front of them, facing the way
	// they face, whole the way DayZ's own developer spawn and COT make one:
	// its OnDebugSpawn fits its wheels, doors and engine parts, then a car's
	// fuel, oil, brake fluid and coolant are filled. A boat keeps their
	// height (on the water where they stand at its edge); anything else
	// goes on the ground.
	static string SpawnVehicle(PlayerBase p, string type)
	{
		vector dir = p.GetDirection();
		dir[1] = 0;
		dir.Normalize();
		vector at = p.GetPosition() + dir * 8;
		int flags = ECE_PLACE_ON_SURFACE;
		if (g_Game.IsKindOf(type, "Boat"))
			flags = ECE_KEEPHEIGHT;
		else
			at[1] = g_Game.SurfaceY(at[0], at[2]);
		EntityAI v = EntityAI.Cast(g_Game.CreateObjectEx(type, at, flags));
		if (!v)
			return "the game couldn't create " + type;
		v.SetDirection(dir);
		v.OnDebugSpawn();
		// Every fluid to the brim, as COT does: a modded car's OnDebugSpawn
		// may fill none, and vanilla's leaves out brake fluid.
		Car car = Car.Cast(v);
		if (car)
		{
			TopUp(car, CarFluid.FUEL);
			TopUp(car, CarFluid.OIL);
			TopUp(car, CarFluid.BRAKE);
			TopUp(car, CarFluid.COOLANT);
		}
		return "";
	}

	// TopUp fills a car's fluid to its capacity.
	static void TopUp(Car car, CarFluid fluid)
	{
		float cap = car.GetFluidCapacity(fluid);
		float frac = car.GetFluidFraction(fluid);
		if (cap > 0 && frac < 1)
			car.Fill(fluid, cap * (1 - frac));
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
