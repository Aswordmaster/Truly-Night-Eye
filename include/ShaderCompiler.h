#pragma once

namespace night_eye
{
	struct CompilerStatus
	{
		bool installed = false;
		bool lightingRecognized = false;
		bool effectRecognized = false;
		std::uint64_t compiled = 0;
		std::uint64_t cacheHits = 0;
		std::uint64_t fallbacks = 0;
		std::string version;
		std::string diagnostic;
	};

	void InitializeCompiler();
	bool IsGeometryAdapterSupported(RE::BSShader::Type type) noexcept;
	CompilerStatus GetCompilerState();
	std::string GetCompilerStatus();
}
