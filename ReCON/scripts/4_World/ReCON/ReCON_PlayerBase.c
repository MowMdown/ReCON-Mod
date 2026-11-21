// Tells ReCON_Module about deaths, with who or what did it.
modded class PlayerBase
{
	// What last hurt them and when (g_Game.GetTime(), ms), for telling what
	// a death no one caused was: a fall, a fire.
	string m_ReCON_LastAmmo;
	int m_ReCON_LastHitAt;
	// Set by ReCON's Kill action just before it kills them.
	bool m_ReCON_AdminKill;

	override void EEHitBy(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
	{
		super.EEHitBy(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);
		m_ReCON_LastAmmo = ammo;
		m_ReCON_LastHitAt = g_Game.GetTime();
	}

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
