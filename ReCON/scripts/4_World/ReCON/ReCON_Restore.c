// ReCON_Restore gives a player back items ReCON saved when they died (the
// ones the admin picked): each with its attachments and contents, its
// condition, quantity (rounds for a magazine) and wetness. Clothing and
// gear go into their slot if it's free, else anywhere free in their
// inventory, else on the ground at their feet; attachments onto their parent
// and contents into their container. A class the server doesn't have is
// skipped and named in the result.
// ReCON_RestoreLater is a restore waiting for the player's inventory to be
// emptied (clear): the deleted items still hold their slots until the
// game has removed them.
class ReCON_RestoreLater
{
	PlayerBase player;
	ref array<ref ReCON_Item> items;
	float health;
};

class ReCON_Restore
{
	protected static ref array<ref ReCON_RestoreLater> s_Later = new array<ref ReCON_RestoreLater>();
	protected static ref TStringArray s_Skipped;
	// The health every item gets, as a fraction of its most (-1: as saved).
	protected static float s_Health = -1;

	// Run gives back items (ReCON 3.0.2 on sends them as a list); json is the
	// older form, the same list as JSON text, still read when there's no list.
	// condition: "worn" (65%) or "pristine" (100%) sets every item's
	// condition; "" keeps the one saved. clear empties their inventory first
	// (what they wear, carry and hold).
	static string Run(PlayerBase p, array<ref ReCON_Item> given, string json, string condition, bool clear)
	{
		array<ref ReCON_Item> items = given;
		if (!items || items.Count() == 0)
		{
			JsonSerializer js = new JsonSerializer();
			string err;
			if (json == "" || !js.ReadFromString(items, json, err) || !items)
				return "couldn't read the items: " + err;
		}
		s_Skipped = new TStringArray();
		s_Health = -1;
		if (condition == "worn")
			s_Health = 0.65;
		else if (condition == "pristine")
			s_Health = 1;
		if (clear)
		{
			ReCON_RestoreLater later = new ReCON_RestoreLater();
			later.player = p;
			later.items = items;
			later.health = s_Health;
			s_Later.Insert(later);
			p.ClearInventory();
			g_Game.GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(ReCON_Restore.RunLater, 1000, false);
			return "";
		}
		MakeAll(p, items);
		if (s_Skipped.Count() > 0)
			return "not made (not on this server): " + JoinSkipped();
		return "";
	}

	protected static void MakeAll(PlayerBase p, array<ref ReCON_Item> items)
	{
		int n = items.Count();
		for (int i = 0; i < n; i++)
			Make(p, null, items[i], true, i, n);
	}

	// RunLater makes the items of the restores that emptied an inventory
	// first (a second ago). A player who died or left meanwhile gets nothing.
	static void RunLater()
	{
		array<ref ReCON_RestoreLater> due = new array<ref ReCON_RestoreLater>();
		foreach (ReCON_RestoreLater l : s_Later)
			due.Insert(l);
		s_Later.Clear();
		foreach (ReCON_RestoreLater d : due)
		{
			if (!d.player || !d.player.IsAlive())
				continue;
			s_Skipped = new TStringArray();
			s_Health = d.health;
			MakeAll(d.player, d.items);
			if (s_Skipped.Count() > 0)
				ReCON_Log.Info("restore: not made (not on this server): " + JoinSkipped());
		}
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
		if (s_Health >= 0)
			e.SetHealth("", "", e.GetMaxHealth("", "") * s_Health);
		else if (it.healthMax > 0)
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
