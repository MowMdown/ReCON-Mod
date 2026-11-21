// ReCON_Vehicles keeps every vehicle on the server (cars, motorbikes,
// boats) as DayZ makes and deletes them, for the Live Map's vehicles, and
// deletes one when ReCON asks.
class ReCON_Vehicles
{
	protected static ref array<EntityAI> s_All = new array<EntityAI>();

	static void Add(EntityAI v)
	{
		if (g_Game.IsDedicatedServer() && s_All.Find(v) < 0)
			s_All.Insert(v);
	}

	static void Remove(EntityAI v)
	{
		int i = s_All.Find(v);
		if (i >= 0)
			s_All.Remove(i);
	}

	static void Read(array<ref ReCON_Vehicle> list)
	{
		foreach (EntityAI v : s_All)
		{
			if (!v)
				continue;
			ReCON_Vehicle it = new ReCON_Vehicle();
			it.id = v.GetID();
			it.type = v.GetType();
			it.name = v.GetDisplayName();
			vector pos = v.GetPosition();
			it.x = pos[0];
			it.z = pos[2];
			vector dir = v.GetDirection();
			it.heading = Math.Atan2(dir[0], dir[2]) * Math.RAD2DEG;
			if (it.heading < 0)
				it.heading += 360;
			it.health = v.GetHealth01("", "");
			it.crew = Crew(v);
			it.fuel = -1;
			Car car = Car.Cast(v);
			if (car)
			{
				it.fuel = car.GetFluidFraction(CarFluid.FUEL);
				it.engine = car.EngineIsOn();
			}
			list.Insert(it);
		}
	}

	// Detail is what is in the vehicle with that id: its damage zones' health,
	// its parts and its cargo; null when it is gone.
	static ReCON_VehicleDetail Detail(int id)
	{
		foreach (EntityAI v : s_All)
		{
			if (!v || v.GetID() != id)
				continue;
			ReCON_VehicleDetail d = new ReCON_VehicleDetail();
			d.id = id;
			TStringArray zones = new TStringArray();
			v.GetDamageZones(zones);
			foreach (string z : zones)
			{
				if (z == "")
					continue;
				ReCON_Zone zone = new ReCON_Zone();
				zone.name = z;
				zone.health = v.GetHealth01(z, "Health");
				d.zones.Insert(zone);
			}
			ReCON_Inventory.ReadEntity(v, d.items);
			return d;
		}
		return null;
	}

	// PersistentID is the vehicle's ID that survives a server restart
	// (b1:b2:b3:b4); "" if the game hasn't given it one.
	static string PersistentID(EntityAI v)
	{
		int b1, b2, b3, b4;
		v.GetPersistentID(b1, b2, b3, b4);
		if (b1 == 0 && b2 == 0 && b3 == 0 && b4 == 0)
			return "";
		return b1.ToString() + ":" + b2.ToString() + ":" + b3.ToString() + ":" + b4.ToString();
	}

	static EntityAI ByPersistentID(string pid)
	{
		if (pid == "")
			return null;
		foreach (EntityAI v : s_All)
		{
			if (v && PersistentID(v) == pid)
				return v;
		}
		return null;
	}

	// Replace deletes the vehicle with persistent ID pid (a player's last
	// chat car), whoever is in it, unless it is keep (the new one). Gone
	// already: nothing to do.
	static void Replace(string pid, EntityAI keep)
	{
		EntityAI old = ByPersistentID(pid);
		if (!old || old == keep)
			return;
		ReCON_Log.Info("replacing vehicle " + pid + " (" + old.GetType() + ")");
		ForceDelete(old);
	}

	protected static ref array<ref ReCON_VehicleRemover> s_Removers = new array<ref ReCON_VehicleRemover>();

	// ForceDelete deletes v even with people in it: they are asked to get
	// out (as DayZ does in deep water), and 3 s later anyone still in is put
	// on the ground beside it, then it goes. Empty: it goes at once.
	static void ForceDelete(EntityAI v)
	{
		Transport t = Transport.Cast(v);
		if (!t || Crew(v) == 0)
		{
			g_Game.ObjectDelete(v);
			return;
		}
		for (int i = 0; i < t.CrewSize(); i++)
		{
			Human h = t.CrewMember(i);
			if (h && h.GetCommand_Vehicle())
				h.GetCommand_Vehicle().GetOutVehicle();
		}
		ReCON_VehicleRemover rm = new ReCON_VehicleRemover(v);
		s_Removers.Insert(rm);
		g_Game.GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(rm.Finish, 3000, false);
	}

	static void Done(ReCON_VehicleRemover rm)
	{
		int i = s_Removers.Find(rm);
		if (i >= 0)
			s_Removers.Remove(i);
	}

	// Crew is how many are in it.
	static int Crew(EntityAI v)
	{
		Transport t = Transport.Cast(v);
		if (!t)
			return 0;
		int n = 0;
		for (int i = 0; i < t.CrewSize(); i++)
		{
			if (t.CrewMember(i))
				n++;
		}
		return n;
	}

	// Delete removes the vehicle with that id (ReCON_Vehicle.id), and all
	// it holds; not while anyone is in it.
	static string Delete(string idText)
	{
		int id = idText.ToInt();
		EntityAI found;
		foreach (EntityAI v : s_All)
		{
			if (v && v.GetID() == id)
			{
				found = v;
				break;
			}
		}
		if (!found)
			return "that vehicle isn't on the server any more";
		if (Crew(found) > 0)
			return "someone is in it";
		g_Game.ObjectDelete(found);
		return "";
	}
};

// ReCON_VehicleRemover finishes a ForceDelete: whoever is still in the
// vehicle is taken out and stood 2.5 m beside it, then it is deleted.
class ReCON_VehicleRemover
{
	protected EntityAI m_Vehicle;

	void ReCON_VehicleRemover(EntityAI v)
	{
		m_Vehicle = v;
	}

	void Finish()
	{
		if (m_Vehicle)
		{
			Transport t = Transport.Cast(m_Vehicle);
			vector dir = m_Vehicle.GetDirection();
			vector side = Vector(dir[2], 0, -dir[0]);
			for (int i = 0; t && i < t.CrewSize(); i++)
			{
				if (!t.CrewMember(i))
					continue;
				Human h = t.CrewGetOut(i);
				if (!h)
					continue;
				h.UnlinkFromLocalSpace();
				vector at = m_Vehicle.GetPosition() + side * 2.5;
				at[1] = g_Game.SurfaceY(at[0], at[2]);
				h.SetPosition(at);
			}
			g_Game.ObjectDelete(m_Vehicle);
		}
		ReCON_Vehicles.Done(this);
	}
};

modded class CarScript
{
	override void EEInit()
	{
		super.EEInit();
		ReCON_Vehicles.Add(this);
	}

	override void EEDelete(EntityAI parent)
	{
		ReCON_Vehicles.Remove(this);
		super.EEDelete(parent);
	}
};

modded class BoatScript
{
	override void EEInit()
	{
		super.EEInit();
		ReCON_Vehicles.Add(this);
	}

	override void EEDelete(EntityAI parent)
	{
		ReCON_Vehicles.Remove(this);
		super.EEDelete(parent);
	}
};

modded class MotorbikeScript
{
	override void EEInit()
	{
		super.EEInit();
		ReCON_Vehicles.Add(this);
	}

	override void EEDelete(EntityAI parent)
	{
		ReCON_Vehicles.Remove(this);
		super.EEDelete(parent);
	}
};
