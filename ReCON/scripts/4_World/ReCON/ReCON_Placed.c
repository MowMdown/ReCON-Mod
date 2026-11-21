// ReCON_PlacedObjects keeps the objects players placed, for the Live Map's Placed
// objects layer: base building and pitched tents always (also ones from
// before this mod version), and anything else once a player places it.
// Who placed it, and when, is recorded at placing and saved with the object
// (CF mod storage), so it survives a restart. An object picked up again (in
// someone's inventory) isn't sent.
class ReCON_PlacedObjects
{
	protected static ref array<EntityAI> s_All = new array<EntityAI>();

	static void Add(EntityAI e)
	{
		if (g_Game.IsDedicatedServer() && e && s_All.Find(e) < 0)
			s_All.Insert(e);
	}

	static void Remove(EntityAI e)
	{
		int i = s_All.Find(e);
		if (i >= 0)
			s_All.Remove(i);
	}

	// Kind is base, tent, storage, trap or other.
	static string Kind(EntityAI e)
	{
		if (BaseBuildingBase.Cast(e))
			return "base";
		if (TentBase.Cast(e))
			return "tent";
		if (TrapBase.Cast(e))
			return "trap";
		if (e.GetInventory() && e.GetInventory().GetCargo())
			return "storage";
		return "other";
	}

	// Shown is whether it is out in the world now (not picked up; a tent
	// pitched).
	static bool Shown(EntityAI e)
	{
		if (!e || e.GetHierarchyParent())
			return false;
		TentBase tent = TentBase.Cast(e);
		if (tent && tent.GetState() != TentBase.PITCHED)
			return false;
		return true;
	}

	static void Read(array<ref ReCON_Placed> list)
	{
		foreach (EntityAI e : s_All)
		{
			if (!Shown(e))
				continue;
			ReCON_Placed p = new ReCON_Placed();
			p.id = e.GetID();
			p.pid = ReCON_Vehicles.PersistentID(e);
			p.type = e.GetType();
			p.kind = Kind(e);
			p.name = e.GetDisplayName();
			vector pos = e.GetPosition();
			p.x = pos[0];
			p.z = pos[2];
			p.health = e.GetHealth01("", "");
			ItemBase ib = ItemBase.Cast(e);
			if (ib)
			{
				p.placer = ib.m_ReCON_Placer;
				p.placerId = ib.m_ReCON_PlacerId;
				p.placedAt = ib.m_ReCON_PlacedAt;
			}
			list.Insert(p);
		}
	}

	// Detail is what is in and on the object with that id, and its damage
	// zones; null when it is gone.
	static ReCON_PlacedDetail Detail(int id)
	{
		foreach (EntityAI e : s_All)
		{
			if (!e || e.GetID() != id)
				continue;
			ReCON_PlacedDetail d = new ReCON_PlacedDetail();
			d.id = id;
			TStringArray zones = new TStringArray();
			e.GetDamageZones(zones);
			foreach (string z : zones)
			{
				if (z == "")
					continue;
				ReCON_Zone zone = new ReCON_Zone();
				zone.name = z;
				zone.health = e.GetHealth01(z, "Health");
				d.zones.Insert(zone);
			}
			ReCON_Inventory.ReadEntity(e, d.items);
			return d;
		}
		return null;
	}

	static EntityAI ByPersistentID(string pid)
	{
		if (pid == "")
			return null;
		foreach (EntityAI e : s_All)
		{
			if (e && ReCON_Vehicles.PersistentID(e) == pid)
				return e;
		}
		return null;
	}
};

modded class ItemBase
{
	// Who placed it (name, SteamID64) and when (unix), from recon-mod 1.0.1.
	string m_ReCON_Placer;
	string m_ReCON_PlacerId;
	int m_ReCON_PlacedAt;

	override void EEInit()
	{
		super.EEInit();
		if (BaseBuildingBase.Cast(this) || TentBase.Cast(this))
			ReCON_PlacedObjects.Add(this);
	}

	override void EEDelete(EntityAI parent)
	{
		ReCON_PlacedObjects.Remove(this);
		super.EEDelete(parent);
	}

	override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
	{
		super.OnPlacementComplete(player, position, orientation);
		if (!g_Game.IsDedicatedServer())
			return;
		PlayerBase pb = PlayerBase.Cast(player);
		if (pb && pb.GetIdentity())
		{
			m_ReCON_Placer = pb.GetIdentity().GetName();
			m_ReCON_PlacerId = pb.GetIdentity().GetPlainId();
		}
		m_ReCON_PlacedAt = CF_Date.Now(true).GetTimestamp();
		ReCON_PlacedObjects.Add(this);
	}

	override void CF_OnStoreSave(CF_ModStorageMap storage)
	{
		super.CF_OnStoreSave(storage);
		CF_ModStorage ctx = storage["ReCON_Mod"];
		if (!ctx)
			return;
		ctx.Write(m_ReCON_Placer);
		ctx.Write(m_ReCON_PlacerId);
		ctx.Write(m_ReCON_PlacedAt);
	}

	override bool CF_OnStoreLoad(CF_ModStorageMap storage)
	{
		if (!super.CF_OnStoreLoad(storage))
			return false;
		CF_ModStorage ctx = storage["ReCON_Mod"];
		if (!ctx)
			return true;
		if (!ctx.Read(m_ReCON_Placer))
			return false;
		if (!ctx.Read(m_ReCON_PlacerId))
			return false;
		if (!ctx.Read(m_ReCON_PlacedAt))
			return false;
		if (m_ReCON_PlacerId != "")
			ReCON_PlacedObjects.Add(this);
		return true;
	}
};
