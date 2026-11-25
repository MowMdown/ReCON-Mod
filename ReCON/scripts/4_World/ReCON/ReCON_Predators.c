// ReCON_Predators keeps the animals that hunt players (bears, wolves and
// guard dogs) as DayZ makes and deletes them, for the Live Map's
// Predators layer. Dead ones aren't sent.
class ReCON_Predators
{
	protected static ref array<EntityAI> s_All = new array<EntityAI>();

	// Kind is "bear", "wolf" or "dog" for a predator, "" for any other animal.
	static string Kind(EntityAI a)
	{
		if (a.IsKindOf("Animal_UrsusArctos"))
			return "bear";
		if (a.IsKindOf("Animal_CanisLupus"))
			return "wolf";
		if (a.IsKindOf("Animal_CanisFamiliaris_Dobermann_ColorBase") || a.IsKindOf("Animal_CanisFamiliaris_GermanShepherd_ColorBase"))
			return "dog";
		return "";
	}

	static void Add(EntityAI a)
	{
		if (g_Game.IsDedicatedServer() && Kind(a) != "" && s_All.Find(a) < 0)
			s_All.Insert(a);
	}

	static void Remove(EntityAI a)
	{
		int i = s_All.Find(a);
		if (i >= 0)
			s_All.Remove(i);
	}

	// Delete removes the predator with that id (ReCON_Predator.id).
	static string Delete(string idText)
	{
		int id = idText.ToInt();
		foreach (EntityAI a : s_All)
		{
			if (a && a.GetID() == id)
			{
				g_Game.ObjectDelete(a);
				return "";
			}
		}
		return "that animal isn't on the server any more";
	}

	static void Read(array<ref ReCON_Predator> list)
	{
		foreach (EntityAI a : s_All)
		{
			if (!a || !a.IsAlive())
				continue;
			ReCON_Predator p = new ReCON_Predator();
			p.id = a.GetID();
			p.type = a.GetType();
			p.kind = Kind(a);
			vector pos = a.GetPosition();
			p.x = pos[0];
			p.z = pos[2];
			vector dir = a.GetDirection();
			p.heading = Math.Atan2(dir[0], dir[2]) * Math.RAD2DEG;
			if (p.heading < 0)
				p.heading += 360;
			p.health = a.GetHealth01("", "");
			DayZAnimal da = DayZAnimal.Cast(a);
			if (da)
			{
				DayZAnimalInputController ic = da.GetInputController();
				if (ic)
				{
					p.attacking = ic.IsAttack();
				}
			}
			list.Insert(p);
		}
	}
};

modded class AnimalBase
{
	override void EEInit()
	{
		super.EEInit();
		ReCON_Predators.Add(this);
	}

	override void EEDelete(EntityAI parent)
	{
		ReCON_Predators.Remove(this);
		super.EEDelete(parent);
	}
};
