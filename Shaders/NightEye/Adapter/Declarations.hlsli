namespace NightEyeStandalone
{
	cbuffer GeometryParameters : register(b13)
	{
		float4 Eye;
		float4 Actor;
		float4 Enemy;
		float4 Ally;
		float4 Neutral;
		uint4 Flags;
	};

	static const float kInfraredMode = 1.0;
	static const float kPredatorMode = 2.0;
	static const float kEnemyAffiliation = 1.0;
	static const float kAllyAffiliation = 2.0;
	static const float kMinimumBrightness = 0.35;
	static const float3 kInfraredCool = float3(0.8, 0.01, 0.0);
	static const float3 kInfraredWarm = float3(1.0, 0.35, 0.01);
	static const float3 kInfraredHot = float3(1.0, 0.95, 0.65);

	bool IsWorld()
	{
		return (Permutation::ExtraShaderDescriptor & Permutation::ExtraFlags::InWorld) != 0 &&
			(Permutation::ExtraShaderDescriptor & Permutation::ExtraFlags::InReflection) == 0;
	}

	float GetActorStrength()
	{
		bool active = Flags.z != 0 && IsWorld() && (Actor.x == kInfraredMode || Actor.x == kPredatorMode);
		return active ? saturate(Actor.y) : 0.0;
	}

	float3 GetActorColor(float3 linearLighting, float3 surfaceColor)
	{
		float detail = 0.5 * (saturate(Color::RGBToLuminance(max(linearLighting, 0.0))) +
			Color::RGBToLuminance(saturate(surfaceColor)));
		float3 palette;
		[branch] if (Actor.x == kInfraredMode) {
			palette = lerp(kInfraredCool, kInfraredWarm, saturate(detail * 2.0));
			palette = lerp(palette, kInfraredHot, saturate(detail * 2.0 - 1.0));
		} else {
			palette = Neutral.rgb;
			if (Actor.w == kEnemyAffiliation)
				palette = Enemy.rgb;
			else if (Actor.w == kAllyAffiliation)
				palette = Ally.rgb;
		}
		return saturate(palette) * max(Actor.z, 0.0) * lerp(kMinimumBrightness, 1.0, detail);
	}

	float3 EffectToLinear(float3 color)
	{
#if NIGHT_EYE_ADAPTER_OS217
		return Color::SceneGammaToLinear(color);
#else
		return Color::IrradianceToLinear(color);
#endif
	}

	float3 EffectFromLinear(float3 color)
	{
#if NIGHT_EYE_ADAPTER_OS217
		return Color::SceneLinearToGamma(color);
#else
		return Color::IrradianceToGamma(color);
#endif
	}
}

#if defined(LIGHTING)
Texture2D<float> NightEyeStandaloneMask : register(t127);
#endif

#if !defined(MULTBLEND) && !defined(MULTBLEND_DECAL) && !defined(PROJECTED_UV) && !defined(BLOOD) && !defined(IS_VOLUMETRIC_FOG)
#define NIGHT_EYE_STANDALONE_ACTOR_EFFECT 1
#endif

