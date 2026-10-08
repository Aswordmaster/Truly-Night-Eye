#pragma once

namespace night_eye::limits
{
	struct Range
	{
		float minimum;
		float maximum;
	};
	inline constexpr Range Unit{ 0.0f, 1.0f };
	inline constexpr Range Exposure{ 0.0f, 4.0f };
	inline constexpr Range Gamma{ 0.2f, 2.0f };
	inline constexpr Range ShadowLift{ 0.0f, 0.25f };
	inline constexpr Range Grain{ 0.0f, 0.1f };
	inline constexpr Range Transition{ 0.0f, 2.0f };
	inline constexpr Range Eyeshine{ 0.0f, 8.0f };
	inline constexpr Range Separation{ -32.0f, 32.0f };
	inline constexpr Range ActorBrightness{ 0.0f, 4.0f };
	inline constexpr int MaximumVisionMode = 2;
}
