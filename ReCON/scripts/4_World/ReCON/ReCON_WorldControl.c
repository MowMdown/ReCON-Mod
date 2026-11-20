// ReCON_WorldControl reads and changes the weather and the in-game time,
// modelled on how Community Online Tools' weather module does it (COT, by
// Jacob_Mango and contributors, CC BY-SA 4.0; written here with DayZ's own
// script API, no COT code copied; see NOTICE): each weather part
// is given a forecast to reach over some seconds and keep for at least
// some seconds, and the weather as a whole follows the map's rules
// ("world"), only what was set ("forecast"), or stays as it is ("static").
class ReCON_WorldControl
{
	protected static bool s_TimeFrozen;

	static void Read(ReCON_World w)
	{
		Weather weather = g_Game.GetWeather();
		if (!weather)
			return;
		w.overcast = weather.GetOvercast().GetActual();
		w.rain = weather.GetRain().GetActual();
		w.fog = weather.GetFog().GetActual();
		w.snow = weather.GetSnowfall().GetActual();
		w.windSpeed = weather.GetWindMagnitude().GetActual();
		w.windDir = ToDegrees(weather.GetWindDirection().GetActual());
		w.overcastTarget = weather.GetOvercast().GetForecast();
		w.rainTarget = weather.GetRain().GetForecast();
		w.fogTarget = weather.GetFog().GetForecast();
		w.snowTarget = weather.GetSnowfall().GetForecast();
		w.windSpeedTarget = weather.GetWindMagnitude().GetForecast();
		w.windDirTarget = ToDegrees(weather.GetWindDirection().GetForecast());
		w.mode = "world";
		if (weather.GetWeatherUpdateFrozen())
			w.mode = "static";
		else if (weather.GetMissionWeather())
			w.mode = "forecast";
		w.timeFrozen = s_TimeFrozen;
	}

	static bool IsWorldAction(string action)
	{
		return action == "weather" || action == "weatherMode" || action == "storm" || action == "rainThresholds" || action == "date" || action == "freezeTime";
	}

	// Run carries out a world action; "" when done, else why not.
	static string Run(ReCON_Command c)
	{
		Weather weather = g_Game.GetWeather();
		if (!weather)
			return "no weather on this server";
		switch (c.action)
		{
			case "weather":
				return SetPhenomenon(weather, c.stat, c.value, c.transition, c.hold);
			case "weatherMode":
				return SetMode(weather, c.text);
			case "storm":
				weather.SetStorm(Math.Clamp(c.value, 0, 1), Math.Clamp(c.min, 0, 1), Math.Max(c.hold, 1));
				return "";
			case "rainThresholds":
				if (c.min > c.max)
					return "the lowest overcast is above the highest";
				weather.SetRainThresholds(Math.Clamp(c.min, 0, 1), Math.Clamp(c.max, 0, 1), Math.Max(c.transition, 0));
				return "";
			case "date":
				if (c.year < 1 || c.month < 1 || c.month > 12 || c.day < 1 || c.day > 31 || c.hour < 0 || c.hour > 23 || c.minute < 0 || c.minute > 59)
					return "not a date and time";
				g_Game.GetWorld().SetDate(c.year, c.month, c.day, c.hour, c.minute);
				return "";
			case "freezeTime":
				s_TimeFrozen = c.value != 0;
				if (s_TimeFrozen)
					g_Game.GetWorld().SetTimeMultiplier(0);
				else
					g_Game.GetWorld().SetTimeMultiplier(-1); // the server's own speed
				return "";
		}
		return "unknown action " + c.action;
	}

	protected static string SetPhenomenon(Weather weather, string part, float value, float transition, float hold)
	{
		if (transition < 0)
			transition = 0;
		if (hold < 0)
			hold = 0;
		switch (part)
		{
			case "overcast":
				weather.GetOvercast().Set(Math.Clamp(value, 0, 1), transition, hold);
				return "";
			case "rain":
				weather.GetRain().Set(Math.Clamp(value, 0, 1), transition, hold);
				return "";
			case "fog":
				weather.GetFog().Set(Math.Clamp(value, 0, 1), transition, hold);
				return "";
			case "snow":
				weather.GetSnowfall().Set(Math.Clamp(value, 0, 1), transition, hold);
				return "";
			case "windSpeed":
				weather.GetWindMagnitude().Set(Math.Max(value, 0), transition, hold);
				return "";
			case "windDir":
				weather.GetWindDirection().Set(ToRadians(value), transition, hold);
				return "";
		}
		return "unknown weather " + part;
	}

	// SetMode: world follows the map's weather rules; forecast keeps only
	// what was set; static freezes the weather as it is.
	protected static string SetMode(Weather weather, string mode)
	{
		switch (mode)
		{
			case "world":
				weather.MissionWeather(false);
				weather.SetWeatherUpdateFreeze(false);
				return "";
			case "forecast":
				weather.MissionWeather(true);
				weather.SetWeatherUpdateFreeze(false);
				return "";
			case "static":
				weather.MissionWeather(false); // the freeze only works without mission weather
				weather.SetWeatherUpdateFreeze(true);
				return "";
		}
		return "unknown weather mode " + mode;
	}

	// The wind's angle is radians in -PI..PI; the pages use 0-360 degrees.
	protected static float ToDegrees(float rad)
	{
		float d = rad * Math.RAD2DEG;
		if (d < 0)
			d += 360;
		return d;
	}

	protected static float ToRadians(float deg)
	{
		while (deg > 180)
			deg -= 360;
		while (deg < -180)
			deg += 360;
		return deg * Math.DEG2RAD;
	}
};
