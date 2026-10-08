#if !defined(DEPTH_WRITE_DECALS)
	float nightEyeActorStrength = NightEyeStandalone::GetActorStrength();
	[branch] if (nightEyeActorStrength > 0.0)
	{
		float3 nightEyeActorColor = NightEyeStandalone::GetActorColor(Color::IrradianceToLinear(color.xyz) + specularColor, material.BaseColor);
		[branch] if ((Permutation::ExtraShaderDescriptor & Permutation::ExtraFlags::AdditiveLighting) != 0)
			nightEyeActorColor *= saturate(Color::RGBToLuminance(material.BaseColor * max(vertexColor, 0.0)));
		color.xyz = lerp(color.xyz, Color::IrradianceToGamma(nightEyeActorColor), nightEyeActorStrength);
		specularColor *= 1.0 - nightEyeActorStrength;
		indirectLobeWeights.diffuse *= 1.0 - nightEyeActorStrength;
		indirectLobeWeights.specular *= 1.0 - nightEyeActorStrength;
#if defined(WETNESS_EFFECTS)
		wetnessReflectance *= 1.0 - nightEyeActorStrength;
#if NIGHT_EYE_ADAPTER_OS217 && defined(CHARACTER_RAIN_SURFACE)
		characterSpotReflectance *= 1.0 - nightEyeActorStrength;
#endif
#endif
	}
#endif
	[branch] if (NightEyeStandalone::Eye.w > 0.0 && NightEyeStandalone::Flags.x != 0 && NightEyeStandalone::IsWorld())
	{
#if defined(EYE)
		float3 nightEyeNormal = input.EyeNormal;
#else
		float3 nightEyeNormal = worldNormal.xyz;
#endif
		float nightEyeFacing = saturate(dot(normalize(nightEyeNormal), viewDirection));
		float nightEyeFacingSquared = nightEyeFacing * nightEyeFacing;
		float nightEyeFacingFourth = nightEyeFacingSquared * nightEyeFacingSquared;
		float nightEyeFacingEighth = nightEyeFacingFourth * nightEyeFacingFourth;
		float nightEyeMask = 1.0;
		[branch] if (NightEyeStandalone::Flags.y != 0)
			nightEyeMask = NightEyeStandaloneMask.SampleLevel(SampColorSampler, uvOriginal, 0.0);
		specularColor += NightEyeStandalone::Eye.xyz * (NightEyeStandalone::Eye.w * nightEyeMask * nightEyeFacingEighth);
	}

