#if !defined(MULTBLEND) && !defined(MULTBLEND_DECAL)
	[branch] if (NightEyeStandalone::Eye.w > 0.0 && NightEyeStandalone::Flags.x != 0 && NightEyeStandalone::IsWorld())
	{
		float nightEyeShape = saturate(Color::RGBToLuminance(baseTexColor.xyz)) * saturate(alpha);
#if !defined(MOTIONVECTORS_NORMALS)
#if defined(EXP_HEIGHT_FOG)
		nightEyeShape *= (1.0 - saturate(vanillaFogFactor)) * (1.0 - saturate(expFogFactor));
#else
		nightEyeShape *= 1.0 - saturate(fogFactor);
#endif
#endif
		blendedColor += NightEyeStandalone::Eye.xyz * (NightEyeStandalone::Eye.w * nightEyeShape);
	}
#endif

