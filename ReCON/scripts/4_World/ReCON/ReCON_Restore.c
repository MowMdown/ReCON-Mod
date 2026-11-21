// ReCON_Restore gives a player back items ReCON saved when they died (the
// ones the admin picked): each with its attachments and contents, its
// condition, quantity (rounds for a magazine) and wetness. Clothing and
// gear go into their slot if it's free, else anywhere free in their
// inventory, else on the ground at their feet; attachments onto their parent
// and contents into their container. A class the server doesn't have is
// skipped and named in the result.
class ReCON_Restore
{
	protected static ref TStringArray s_Skipped;

	static string Run(PlayerBase p, string json)
	{
		array<ref ReCON_Item> items;
		JsonSerializer js = new JsonSerializer();
		string err;
		if (!js.ReadFromString(items, json, err) || !items)
			return "couldn't read the items: " + err;
		s_Skipped = new TStringArray();
		int n = items.Count();
		for (int i = 0; i < n; i++)
			Make(p, null, items[i], true, i, n);
		if (s_Skipped.Count() > 0)
			return "not made (not on this server): " + JoinSkipped();
		return "";
	}

	protected static string JoinSkipped()
	{
		string names = "";
		foreach (string t : s_Skipped)
		{
			if (names != "")
				names += ", ";
			names += t;
		}
		return names;
	}

	// Make creates it (on parent when there is one: attached, or in its
	// cargo), then what was on and in it.
	protected static EntityAI Make(PlayerBase p, EntityAI parent, ReCON_Item it, bool attach, int i, int n)
	{
		if (!ReCON_Catalog.Spawnable(it.type))
		{
			s_Skipped.Insert(it.type);
			return null;
		}
		EntityAI e;
		if (parent)
		{
			if (attach)
				e = parent.GetInventory().CreateAttachment(it.type);
			if (!e)
				e = parent.GetInventory().CreateEntityInCargo(it.type);
		}
		else if (it.slot != "" && it.slot != "Hands")
		{
			e = p.GetInventory().CreateAttachment(it.type);
		}
		if (!e)
			e = ReCON_Actions.InInventory(p, it.type);
		if (!e)
			e = ReCON_Actions.OnGround(p, it.type, i, n);
		if (!e)
		{
			s_Skipped.Insert(it.type);
			return null;
		}
		Apply(e, it);
		if (it.attachments)
		{
			foreach (ReCON_Item a : it.attachments)
				Make(p, e, a, true, 0, 1);
		}
		if (it.cargo)
		{
			foreach (ReCON_Item c : it.cargo)
				Make(p, e, c, false, 0, 1);
		}
		return e;
	}

	protected static void Apply(EntityAI e, ReCON_Item it)
	{
		if (it.healthMax > 0)
			e.SetHealth("", "", it.health);
		ItemBase ib = ItemBase.Cast(e);
		if (!ib)
			return;
		Magazine mag = Magazine.Cast(e);
		if (it.quantityMax > 0)
		{
			if (mag)
				mag.ServerSetAmmoCount((int)it.quantity);
			else if (ib.HasQuantity())
				ib.SetQuantity(it.quantity);
		}
		if (it.wet > 0)
			ib.SetWet(it.wet);
	}
};
