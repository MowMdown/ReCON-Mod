// ReCON_Inventory reads everything a player carries: the item in their
// hands, what is attached to them (clothes, backpack, weapons on the
// shoulder), and inside each item its attachments and its cargo, all the
// way down.
class ReCON_Inventory
{
	// MAX_ITEMS caps one player's list, so a hoarder can't blow up a sync.
	static const int MAX_ITEMS = 600;

	static void Read(PlayerBase p, array<ref ReCON_Item> list)
	{
		int budget = MAX_ITEMS;
		EntityAI hands = p.GetItemInHands();
		if (hands)
		{
			ReCON_Item h = Item(hands, budget);
			h.slot = "Hands";
			list.Insert(h);
		}
		Attachments(p, list, budget);
	}

	// ReadEntity reads what is attached to e and in its cargo, all the way
	// down (a vehicle's parts and trunk).
	static void ReadEntity(EntityAI e, array<ref ReCON_Item> list)
	{
		int budget = MAX_ITEMS;
		Attachments(e, list, budget);
		Cargo(e, list, budget);
	}

	protected static void Attachments(EntityAI parent, array<ref ReCON_Item> list, inout int budget)
	{
		GameInventory inv = parent.GetInventory();
		if (!inv)
			return;
		int n = inv.AttachmentCount();
		for (int i = 0; i < n && budget > 0; i++)
		{
			EntityAI e = inv.GetAttachmentFromIndex(i);
			if (!e)
				continue;
			ReCON_Item it = Item(e, budget);
			InventoryLocation il = new InventoryLocation();
			if (e.GetInventory().GetCurrentInventoryLocation(il))
				it.slot = InventorySlots.GetSlotName(il.GetSlot());
			list.Insert(it);
		}
	}

	protected static void Cargo(EntityAI parent, array<ref ReCON_Item> list, inout int budget)
	{
		GameInventory inv = parent.GetInventory();
		if (!inv)
			return;
		CargoBase cargo = inv.GetCargo();
		if (!cargo)
			return;
		int n = cargo.GetItemCount();
		for (int i = 0; i < n && budget > 0; i++)
		{
			EntityAI e = cargo.GetItem(i);
			if (!e)
				continue;
			ReCON_Item it = Item(e, budget);
			int row, col;
			cargo.GetItemRowCol(i, row, col);
			it.row = row;
			it.col = col;
			list.Insert(it);
		}
	}

	protected static ReCON_Item Item(EntityAI e, inout int budget)
	{
		budget--;
		ReCON_Item it = new ReCON_Item();
		it.id = e.GetID();
		it.type = e.GetType();
		it.name = e.GetDisplayName();
		it.row = -1;
		it.col = -1;
		it.health = e.GetHealth("", "Health");
		it.healthMax = e.GetMaxHealth("", "Health");
		it.healthLevel = e.GetHealthLevel("");
		it.temperature = e.GetTemperature();

		Magazine mag = Magazine.Cast(e);
		ItemBase ib = ItemBase.Cast(e);
		if (mag)
		{
			it.quantity = mag.GetAmmoCount();
			it.quantityMax = mag.GetAmmoMax();
			it.unit = "rounds";
		}
		else if (ib && ib.HasQuantity())
		{
			it.quantity = ib.GetQuantity();
			it.quantityMax = ib.GetQuantityMax();
			if (ib.IsLiquidContainer())
				it.unit = "ml";
		}
		if (ib)
			it.wet = ib.GetWet();

		Attachments(e, it.attachments, budget);
		Cargo(e, it.cargo, budget);
		return it;
	}
};
