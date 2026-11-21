// ReCON_WorldEvents keeps the map's dynamic events as DayZ makes and
// deletes them, for the Live Map: helicopter crashes (CrashBase) and areas
// (EffectArea: toxic zones, static, from an airstrike or a grenade).
class ReCON_WorldEvents
{
	protected static ref array<EntityAI> s_All = new array<EntityAI>();

	static void Add(EntityAI e)
	{
		if (g_Game.IsDedicatedServer() && s_All.Find(e) < 0)
			s_All.Insert(e);
	}

	static void Remove(EntityAI e)
	{
		int i = s_All.Find(e);
		if (i >= 0)
			s_All.Remove(i);
	}

	static void Read(array<ref ReCON_WorldEvent> list)
	{
		foreach (EntityAI e : s_All)
		{
			if (!e)
				continue;
			ReCON_WorldEvent w = new ReCON_WorldEvent();
			w.id = e.GetID();
			w.type = e.GetType();
			vector pos = e.GetPosition();
			w.x = pos[0];
			w.z = pos[2];
			EffectArea area = EffectArea.Cast(e);
			if (CrashBase.Cast(e))
				w.kind = "crash";
			else if (ContaminatedArea_Base.Cast(e))
				w.kind = "gas";
			else
				w.kind = "area";
			if (area)
				w.radius = area.m_Radius;
			list.Insert(w);
		}
	}
};

modded class CrashBase
{
	override void EEInit()
	{
		super.EEInit();
		ReCON_WorldEvents.Add(this);
	}

	override void EEDelete(EntityAI parent)
	{
		ReCON_WorldEvents.Remove(this);
		super.EEDelete(parent);
	}
};

modded class EffectArea
{
	override void EEInit()
	{
		super.EEInit();
		ReCON_WorldEvents.Add(this);
	}

	override void EEDelete(EntityAI parent)
	{
		ReCON_WorldEvents.Remove(this);
		super.EEDelete(parent);
	}
};
