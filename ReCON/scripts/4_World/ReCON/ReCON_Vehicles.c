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
