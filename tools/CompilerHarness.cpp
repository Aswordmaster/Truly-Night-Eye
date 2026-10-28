#include "PCH.h"
#include "../src/ShaderCompiler.cpp"

#include <iostream>
#include <cstring>

namespace
{
	using Microsoft::WRL::ComPtr;

	class TrackingIncludes final : public ID3DInclude
	{
	public:
		std::vector<std::string> paths;

		HRESULT __stdcall Open(D3D_INCLUDE_TYPE, LPCSTR filename, LPCVOID, LPCVOID* data, UINT* bytes) override
		{
			try {
				const std::filesystem::path path = std::filesystem::path("Data/Shaders") / filename;
				std::ifstream stream(path, std::ios::binary);
				const auto size = std::filesystem::file_size(path);
				auto buffer = std::make_unique<char[]>(static_cast<std::size_t>(size));
				if (!stream.read(buffer.get(), static_cast<std::streamsize>(size)))
					return E_FAIL;
				*data = buffer.release();
				*bytes = static_cast<UINT>(size);
				paths.push_back(path.generic_string());
				return S_OK;
			} catch (...) {
				return E_FAIL;
			}
		}

		HRESULT __stdcall Close(LPCVOID data) override
		{
			delete[] static_cast<const char*>(data);
			return S_OK;
		}
	};

	void Require(bool condition, std::string_view message)
	{
		if (!condition)
			throw std::runtime_error(std::string(message));
	}

	std::string BlobHash(ID3DBlob* blob)
	{
		return night_eye::SHA256(std::string_view(static_cast<const char*>(blob->GetBufferPointer()), blob->GetBufferSize()));
	}

	nlohmann::json Bindings(ID3DBlob* blob)
	{
		ComPtr<ID3D11ShaderReflection> reflection;
		Require(SUCCEEDED(D3DReflect(blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(reflection.GetAddressOf()))), "Reflection failed");
		D3D11_SHADER_DESC description{};
		Require(SUCCEEDED(reflection->GetDesc(&description)), "Reflection description failed");
		nlohmann::json bindings = nlohmann::json::array();
		for (UINT index = 0; index < description.BoundResources; ++index) {
			D3D11_SHADER_INPUT_BIND_DESC binding{};
			Require(SUCCEEDED(reflection->GetResourceBindingDesc(index, &binding)), "Resource reflection failed");
			nlohmann::json item{ { "name", binding.Name }, { "type", binding.Type }, { "slot", binding.BindPoint }, { "count", binding.BindCount } };
			if (binding.Type == D3D_SIT_CBUFFER) {
				D3D11_SHADER_BUFFER_DESC buffer{};
				Require(SUCCEEDED(reflection->GetConstantBufferByName(binding.Name)->GetDesc(&buffer)), "Buffer reflection failed");
				item["bytes"] = buffer.Size;
			}
			bindings.push_back(std::move(item));
		}
		return bindings;
	}

	ComPtr<ID3DBlob> Compile(const std::wstring& path, TrackingIncludes& includes, bool original = false, bool multiplicative = false)
	{
		const D3D_SHADER_MACRO definitions[] = { { "PSHADER", "1" }, { multiplicative ? "MULTBLEND" : nullptr, "1" }, { nullptr, nullptr } };
		ComPtr<ID3DBlob> output;
		ComPtr<ID3DBlob> errors;
		const auto function = original ? night_eye::originalCompile : &D3DCompileFromFile;
		const auto result = function(path.c_str(), definitions, &includes, "main", "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
			output.GetAddressOf(), errors.GetAddressOf());
		Require(SUCCEEDED(result) && output, "Compilation failed: " + night_eye::BlobText(errors.Get()));
		return output;
	}

	void InstallHooks()
	{
		night_eye::LoadRecipes();
		Require(DetourTransactionBegin() == NO_ERROR, "Detours begin failed");
		Require(DetourUpdateThread(GetCurrentThread()) == NO_ERROR, "Detours thread failed");
		Require(DetourAttach(reinterpret_cast<PVOID*>(&night_eye::originalCompile), reinterpret_cast<PVOID>(night_eye::CompileFromFile)) == NO_ERROR, "Compile hook failed");
		Require(DetourAttach(reinterpret_cast<PVOID*>(&night_eye::originalRead), reinterpret_cast<PVOID>(night_eye::ReadFileToBlob)) == NO_ERROR, "Read hook failed");
		Require(DetourAttach(reinterpret_cast<PVOID*>(&night_eye::originalWrite), reinterpret_cast<PVOID>(night_eye::WriteBlobToFile)) == NO_ERROR, "Write hook failed");
		Require(DetourTransactionCommit() == NO_ERROR, "Detours commit failed");
		night_eye::installed = true;
	}
}

int main()
{
	try {
		nlohmann::json report;
		report["scope"] = "Isolated workspace fixture using the real public compiler/cache hooks and stock shaders";
		std::filesystem::create_directories("Data/ShaderCache/Lighting");
		std::filesystem::create_directories("Data/ShaderCache/Effect");
		InstallHooks();
		Require(!night_eye::IsGeometryAdapterSupported(RE::BSShader::Type::ImageSpace), "Non-geometry shader family was marked supported");
		for (const auto* family : { "Lighting", "Effect" }) {
			const std::wstring wideFamily(family, family + std::strlen(family));
			const auto sourcePath = L"Data/Shaders/" + wideFamily + L".hlsl";
			const auto hostCache = L"Data/ShaderCache/" + wideFamily + L"/ABC.pso";
			TrackingIncludes originalIncludes;
			auto baseline = Compile(sourcePath, originalIncludes, true);
			Require(SUCCEEDED(night_eye::originalWrite(baseline.Get(), hostCache.c_str(), TRUE)), "Cannot create host cache fixture");
			ComPtr<ID3DBlob> bypass;
			Require(FAILED(D3DReadFileToBlob(hostCache.c_str(), bypass.GetAddressOf())) && !bypass, "Target host cache read was not bypassed");
			const auto shaderType = std::string_view(family) == "Lighting" ? RE::BSShader::Type::Lighting : RE::BSShader::Type::Effect;
			Require(night_eye::IsGeometryAdapterSupported(shaderType), "Recognized family binding gate remained disabled");
			const auto before = night_eye::GetCompilerState();
			TrackingIncludes firstIncludes;
			auto first = Compile(sourcePath, firstIncludes);
			const auto firstState = night_eye::GetCompilerState();
			Require(firstState.compiled == before.compiled + 1 && firstState.fallbacks == before.fallbacks, "Adapted compile did not succeed");
			TrackingIncludes secondIncludes;
			auto second = Compile(sourcePath, secondIncludes);
			Require(night_eye::GetCompilerState().cacheHits == firstState.cacheHits + 1, "Addon cache did not hit");
			Require(BlobHash(first.Get()) == BlobHash(second.Get()), "Addon cache bytecode changed");
			Require(!firstIncludes.paths.empty() && firstIncludes.paths == secondIncludes.paths, "Caller include tracking was not preserved on cache hit");
			const auto bindings = Bindings(first.Get());
			Require(std::ranges::any_of(bindings, [](const auto& item) { return item.at("type") == D3D_SIT_CBUFFER && item.at("slot") == 13 && item.at("bytes") == 96; }), "b13 contract is not 96 bytes");
			if (std::string_view(family) == "Lighting")
				Require(std::ranges::any_of(bindings, [](const auto& item) { return item.at("type") == D3D_SIT_TEXTURE && item.at("slot") == 127; }), "Lighting mask t127 is missing");
			ComPtr<ID3DBlob> stripped;
			Require(SUCCEEDED(D3DStripShader(first->GetBufferPointer(), first->GetBufferSize(), D3DCOMPILER_STRIP_REFLECTION_DATA, stripped.GetAddressOf())), "Shader stripping failed");
			Require(FAILED(D3DWriteBlobToFile(stripped.Get(), hostCache.c_str(), TRUE)), "Suppressed host write reported success and would refresh the host manifest");
			ComPtr<ID3DBlob> hostAfter;
			Require(SUCCEEDED(night_eye::originalRead(hostCache.c_str(), hostAfter.GetAddressOf())) && BlobHash(hostAfter.Get()) == BlobHash(baseline.Get()), "Patched stripped shader polluted the host cache");
			TrackingIncludes multOriginalIncludes;
			TrackingIncludes multAdaptedIncludes;
			auto multBaseline = Compile(sourcePath, multOriginalIncludes, true, true);
			auto multResult = Compile(sourcePath, multAdaptedIncludes, false, true);
			Require(BlobHash(multBaseline.Get()) == BlobHash(multResult.Get()), "MULTBLEND did not retain exact original bytecode");
			report[family] = { { "baselineSHA256", BlobHash(baseline.Get()) }, { "adaptedSHA256", BlobHash(first.Get()) },
				{ "cacheByteIdentical", true }, { "callerIncludesTracked", firstIncludes.paths }, { "bindings", bindings },
				{ "hostCacheUnchangedAfterStripping", true }, { "hostManifestUpdatePrevented", true }, { "multiplicativeByteIdentical", true } };
			if (std::string_view(family) == "Lighting") {
				const auto cachedFile = std::filesystem::directory_iterator(night_eye::kAddonCacheDirectory)->path();
				{
					std::ofstream stream(cachedFile, std::ios::binary | std::ios::trunc);
					stream << "invalid shader cache fixture";
				}
				const auto beforeRepair = night_eye::GetCompilerState();
				TrackingIncludes repairIncludes;
				auto repaired = Compile(sourcePath, repairIncludes);
				Require(BlobHash(repaired.Get()) == BlobHash(first.Get()) && night_eye::GetCompilerState().compiled == beforeRepair.compiled + 1,
					"Corrupt addon cache entry was not safely recompiled");
				report["corruptCacheRecompiled"] = true;
			}
		}

		const auto includePath = std::filesystem::path("Data/Shaders/Common/Color.hlsli");
		const auto includeOriginal = night_eye::ReadText(includePath);
		const auto compiledBefore = night_eye::GetCompilerState().compiled;
		{
			std::ofstream stream(includePath, std::ios::binary | std::ios::app);
			stream << "\n#ifndef NIGHT_EYE_HARNESS_INCLUDE_PROBE\n#define NIGHT_EYE_HARNESS_INCLUDE_PROBE\nstatic const float kNightEyeHarnessIncludeProbe = 0.125;\n#endif\n";
		}
		TrackingIncludes changedIncludes;
		Compile(L"Data/Shaders/Lighting.hlsl", changedIncludes);
		Require(night_eye::GetCompilerState().compiled == compiledBefore + 1, "Changed include did not invalidate addon cache");
		{
			std::ofstream stream(includePath, std::ios::binary | std::ios::trunc);
			stream << includeOriginal;
		}
		report["includeCacheInvalidation"] = true;

		const auto sourcePath = std::filesystem::path("Data/Shaders/Lighting.hlsl");
		const auto originalSource = night_eye::ReadText(sourcePath);
		for (const auto* fixture : { "// unknown source fixture", "// NIGHT_EYE legacy source fixture" }) {
			{
				std::ofstream stream(sourcePath, std::ios::binary | std::ios::trunc);
				stream << originalSource << '\n' << fixture << '\n';
			}
			TrackingIncludes baselineIncludes;
			TrackingIncludes fallbackIncludes;
			auto baseline = Compile(sourcePath.native(), baselineIncludes, true);
			auto fallback = Compile(sourcePath.native(), fallbackIncludes);
			Require(BlobHash(baseline.Get()) == BlobHash(fallback.Get()), "Unrecognized source did not pass through unchanged");
			Require(!night_eye::GetCompilerState().lightingRecognized, "Unrecognized source status remained recognized");
			Require(!night_eye::IsGeometryAdapterSupported(RE::BSShader::Type::Lighting), "Unknown shader family still permitted geometry binding");
			Require(night_eye::IsGeometryAdapterSupported(RE::BSShader::Type::Effect), "Unknown Lighting source incorrectly disabled the recognized Effect family");
			Require(!night_eye::BypassHostCache(L"Data/ShaderCache/Lighting/ABC.pso"), "Unknown source still bypassed host cache");
		}
		{
			std::ofstream stream(sourcePath, std::ios::binary | std::ios::trunc);
			stream << originalSource;
		}
		report["unknownAndLegacyPassThroughByteIdentical"] = true;
		report["geometryBindingsOnlyForRecognizedFamilies"] = true;
		const auto activeRecipe = std::ranges::find_if(night_eye::recipes, [&](const auto& recipe) { return recipe.family == "Lighting" && recipe.sourceHash == night_eye::SHA256(originalSource); });
		Require(activeRecipe != night_eye::recipes.end(), "Failure fixture cannot identify the recognized recipe");
		const auto originalBody = activeRecipe->insertions.front().body;
		activeRecipe->insertions.front().body += "\ninvalid_adaptation_failure_fixture;\n";
		night_eye::sourceCache.clear();
		const auto beforeFailure = night_eye::GetCompilerState();
		TrackingIncludes failureBaselineIncludes;
		TrackingIncludes failureFallbackIncludes;
		auto failureBaseline = Compile(sourcePath.native(), failureBaselineIncludes, true);
		auto failureFallback = Compile(sourcePath.native(), failureFallbackIncludes);
		Require(BlobHash(failureBaseline.Get()) == BlobHash(failureFallback.Get()) && night_eye::GetCompilerState().fallbacks == beforeFailure.fallbacks + 1,
			"Invalid adaptation did not recover with exact original bytecode");
		Require(night_eye::IsGeometryAdapterSupported(RE::BSShader::Type::Lighting), "Failed variant disabled geometry binding for valid recognized Lighting variants");
		Require(night_eye::IsGeometryAdapterSupported(RE::BSShader::Type::Effect), "Failed Lighting adaptation incorrectly disabled the recognized Effect family");
		activeRecipe->insertions.front().body = originalBody;
		night_eye::sourceCache.clear();
		TrackingIncludes recoveredIncludes;
		auto recovered = Compile(sourcePath.native(), recoveredIncludes);
		const auto recoveredBindings = Bindings(recovered.Get());
		Require(std::ranges::any_of(recoveredBindings, [](const auto& item) {
			return item.at("name") == "NightEyeStandalone::GeometryParameters" &&
				item.at("type") == D3D_SIT_CBUFFER && item.at("slot") == 13 && item.at("bytes") == 96;
		}), "Recovered adaptation lost its b13 geometry buffer contract");
		Require(night_eye::IsGeometryAdapterSupported(RE::BSShader::Type::Lighting), "Recovered Lighting adaptation cannot receive fresh geometry bindings");
		report["adaptationFailureFallsBackByteIdentical"] = true;
		report["adaptationFailurePreservesBindingsForValidRecognizedVariants"] = true;
		report["adaptationRetryRecoversGeometryBufferContract"] = true;
		report["passed"] = true;
		report["status"] = night_eye::GetCompilerStatus();
		std::ofstream("CompilerHarnessValidation.json") << report.dump(2) << '\n';
		std::cout << report.dump(2) << '\n';
		return 0;
	} catch (const std::exception& exception) {
		std::cerr << exception.what() << '\n';
		std::ofstream("CompilerHarnessValidation.json") << nlohmann::json{ { "passed", false }, { "error", exception.what() } }.dump(2) << '\n';
		return 1;
	}
}
