#if defined(NIGHT_EYE_STANDALONE_ACTOR_EFFECT)
	float nightEyeActorStrength = NightEyeStandalone::GetActorStrength();
	[branch] if (nightEyeActorStrength > 0.0)
	{
		float3 nightEyeActorColor = NightEyeStandalone::GetActorColor(NightEyeStandalone::EffectToLinear(lightColor), baseColor.xyz);
		nightEyeActorColor *= saturate(Color::RGBToLuminance(baseColor.xyz));
		lightColor = lerp(lightColor, NightEyeStandalone::EffectFromLinear(nightEyeActorColor), nightEyeActorStrength);
	}
#endif

