#pragma once

namespace night_eye
{
	inline constexpr std::initializer_list<REL::Version> kSupportedRuntimes{
		SKSE::RUNTIME_SSE_1_5_97, SKSE::RUNTIME_SSE_1_6_1130,
		SKSE::RUNTIME_SSE_1_6_1170, SKSE::RUNTIME_SSE_1_7_104
	};

	inline bool IsSupportedRuntime(const REL::Version& version)
	{
		return std::ranges::find(kSupportedRuntimes, version) != kSupportedRuntimes.end();
	}
}
