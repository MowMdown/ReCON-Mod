// Tells ReCON_Module about deaths, with who or what did it.
modded class PlayerBase
{
	override void EEKilled(Object killer)
	{
		super.EEKilled(killer);
		if (!g_Game.IsDedicatedServer())
			return;
		ReCON_Module recon = ReCON_Module.Get();
		if (recon)
			recon.OnKilled(this, killer);
	}
};
