#include "NightEye.h"
#include <DDSTextureLoader.h>
#include <algorithm>
#include <cctype>
#include <optional>
#include <ranges>

namespace night_eye
{
	namespace
	{
		struct EyeshineMask
		{
			std::string filename;
			std::vector<RE::BSFixedString> geometryNames;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
			bool matchLogged = false;
		};
		std::vector<EyeshineMask> eyeshineMasks;
		std::unordered_map<std::string, size_t> eyeshineMaskAliases;
		std::unordered_map<RE::BSFixedString, size_t, RE::BSCRC32<RE::BSFixedString>> eyeshineMaskCache;
		ID3D11ShaderResourceView* currentEyeshineMask = nullptr;
	}
	namespace
	{
		constexpr auto kEyeWords = std::to_array<std::string_view>({ "eye", "eyes", "eyeball", "eyeballs", "eyegeo", "eyesgeo", "eyemesh", "eyesmesh",
			"maleeyes", "femaleeyes", "eyesmale", "eyesfemale", "facegeneyes" });
		constexpr auto kNonEyeWords = std::to_array<std::string_view>({ "brow", "brows", "lash", "lashes", "lid", "lids", "body", "torso", "head",
			"teeth", "tooth", "socket", "sockets" });

		bool StartsNameWord(std::string_view name, size_t index)
		{
			const auto current = static_cast<unsigned char>(name[index]);
			const auto previous = static_cast<unsigned char>(name[index - 1]);
			if (std::isdigit(current) != std::isdigit(previous))
				return true;
			const bool acronymEnd = std::isupper(previous) && index + 1 < name.size() && std::islower(static_cast<unsigned char>(name[index + 1]));
			return std::isupper(current) && (std::islower(previous) || acronymEnd);
		}

		bool IsEyeName(std::string_view name)
		{
			bool hasEyeWord = false;
			for (size_t begin = 0; begin < name.size();) {
				if (!std::isalnum(static_cast<unsigned char>(name[begin]))) {
					++begin;
					continue;
				}
				size_t end = begin + 1;
				while (end < name.size() && std::isalnum(static_cast<unsigned char>(name[end])) && !StartsNameWord(name, end))
					++end;
				std::string word(name.substr(begin, end - begin));
				std::ranges::transform(word, word.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
				if (std::ranges::find(kNonEyeWords, word) != kNonEyeWords.end())
					return false;
				hasEyeWord |= std::ranges::find(kEyeWords, word) != kEyeWords.end();
				begin = end;
			}
			return hasEyeWord;
		}
	}

	std::optional<bool> GetEyeshineOverride(RE::BSGeometry* geometry)
	{
		if (!geometry)
			return std::nullopt;
		static const RE::BSFixedString eyeshineTag = "NightEyeEyeshine";
		if (auto extraData = geometry->GetExtraData(eyeshineTag)) {
			return extraData->GetRTTI() == REL::Relocation<const RE::NiRTTI*>{ RE::NiRTTI_NiIntegerExtraData }.get() &&
			       static_cast<const RE::NiIntegerExtraData*>(extraData)->value > 0;
		}
		return std::nullopt;
	}

	bool IsEyeGeometry(RE::BSRenderPass* pass)
	{
		if (!pass || !pass->geometry)
			return false;

		auto geometry = pass->geometry;
		auto property = geometry->GetGeometryRuntimeData().shaderProperty.get();
		if (!property)
			return false;
		const bool lightingProperty = property->GetRTTI() == REL::Relocation<const RE::NiRTTI*>{ RE::NiRTTI_BSLightingShaderProperty }.get();
		if (!lightingProperty && property->GetRTTI() != REL::Relocation<const RE::NiRTTI*>{ RE::NiRTTI_BSEffectShaderProperty }.get())
			return false;

		if (const auto override = GetEyeshineOverride(geometry); override.has_value())
			return *override;
		if (lightingProperty) {
			auto lightProperty = static_cast<RE::BSLightingShaderProperty*>(property);
			if (lightProperty->flags.any(RE::BSShaderProperty::EShaderPropertyFlag::kEyeReflect) ||
				(lightProperty->material && lightProperty->material->GetFeature() == RE::BSShaderMaterial::Feature::kEye))
				return true;
		}

		const auto& name = geometry->name;
		if (name.empty())
			return false;
		static thread_local std::unordered_map<RE::BSFixedString, bool, RE::BSCRC32<RE::BSFixedString>> eyeNames;
		auto [entry, inserted] = eyeNames.try_emplace(name, false);
		if (inserted) {
			entry->second = IsEyeName(std::string_view(name.c_str(), name.length()));
			if (entry->second)
				SKSE::log::info("Night Eye: recognized {} eye geometry {}", lightingProperty ? "lighting" : "effect", name);
		}
		return entry->second;
	}

	namespace
	{

		constexpr size_t kMaximumEyeMasks = 64;
		constexpr UINT kMaximumMaskDimension = 2048;
		constexpr auto kMaskDirectory = "Data\\Shaders\\NightEye\\Eyeshine";

		std::string NormalizeTexturePath(std::string path)
		{
			for (auto& character : path)
				character = character == '/' ? '\\' : static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
			if (path.starts_with("data\\"))
				path.erase(0, 5);
			if (!path.starts_with("textures\\"))
				path.insert(0, "textures\\");
			return path;
		}
	}

	void InitializeEyeshineMasks(ID3D11Device* device)
	{
		eyeshineMasks.clear();
		eyeshineMaskAliases.clear();
		eyeshineMaskCache.clear();
		currentEyeshineMask = nullptr;

		const auto configurationPath = std::filesystem::path(kMaskDirectory) / "Masks.json";
		std::ifstream stream(configurationPath);
		if (!stream.is_open()) {
			SKSE::log::warn("Night Eye: eyeshine mask configuration unavailable: {}", configurationPath.string());
			return;
		}

		try {
			const auto configuration = nlohmann::json::parse(stream);
			if (!configuration.is_array() || configuration.size() > kMaximumEyeMasks)
				throw std::runtime_error("Eyeshine mask configuration must be an array with at most 64 entries");
			eyeshineMasks.reserve(configuration.size());
			for (const auto& entry : configuration) {
				const auto maskFile = std::filesystem::path(entry.at("mask").get<std::string>());
				if (maskFile.empty() || maskFile.has_parent_path() || maskFile.extension() != ".dds")
					throw std::runtime_error("Eyeshine masks must name a DDS file inside the Eyeshine directory");
				EyeshineMask mask;
				mask.filename = maskFile.string();
				for (const auto& name : entry.at("geometry"))
					mask.geometryNames.emplace_back(name.get<std::string>());
				if (mask.geometryNames.empty())
					throw std::runtime_error("Eyeshine masks require explicit geometry names");

				const auto maskPath = std::filesystem::path(kMaskDirectory) / maskFile;
				Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
				Microsoft::WRL::ComPtr<ID3D11Resource> resource;
				if (FAILED(DirectX::CreateDDSTextureFromFile(device, maskPath.c_str(), resource.ReleaseAndGetAddressOf(), mask.view.ReleaseAndGetAddressOf())) || FAILED(resource.As(&texture))) {
					SKSE::log::warn("Night Eye: unable to load eyeshine mask {}", maskPath.string());
					continue;
				}
				D3D11_TEXTURE2D_DESC description{};
				texture->GetDesc(&description);
				if (description.Format != DXGI_FORMAT_R8_UNORM || description.ArraySize != 1 || description.SampleDesc.Count != 1 || description.Width > kMaximumMaskDimension || description.Height > kMaximumMaskDimension) {
					SKSE::log::warn("Night Eye: unsupported eyeshine mask dimensions in {}", maskPath.string());
					continue;
				}
				SetResourceName(texture.Get(), std::format("NightEye::EyeshineMask[{}]", maskFile.string()));
				SetResourceName(mask.view.Get(), std::format("NightEye::EyeshineMask[{}] SRV", maskFile.string()));
				const auto index = eyeshineMasks.size();
				eyeshineMasks.push_back(std::move(mask));
				for (const auto& alias : entry.at("diffuse"))
					eyeshineMaskAliases.insert_or_assign(NormalizeTexturePath(alias.get<std::string>()), index);
			}
			SKSE::log::info("Night Eye: loaded {} selective eyeshine masks", eyeshineMasks.size());
		} catch (const std::exception& error) {
			SKSE::log::error("Night Eye: failed to load {}: {}", configurationPath.string(), error.what());
		}
	}

	bool UpdateEyeshineMask(RE::BSRenderPass* pass)
	{
		currentEyeshineMask = nullptr;
		if (!pass || !pass->geometry || !pass->shaderProperty || eyeshineMasks.empty())
			return false;
		if (const auto override = GetEyeshineOverride(pass->geometry); override && !*override)
			return false;
		const auto* shaderMaterial = pass->shaderProperty->material;
		if (!shaderMaterial || shaderMaterial->GetType() != RE::BSShaderMaterial::Type::kLighting)
			return false;
		const auto* material = static_cast<const RE::BSLightingShaderMaterialBase*>(shaderMaterial);
		if (!material->diffuseTexture)
			return false;
		const auto& name = material->diffuseTexture->name;
		if (name.empty())
			return false;

		auto found = eyeshineMaskCache.find(name);
		if (found == eyeshineMaskCache.end()) {
			const auto alias = eyeshineMaskAliases.find(NormalizeTexturePath(name.c_str()));
			const auto index = alias == eyeshineMaskAliases.end() ? std::numeric_limits<size_t>::max() : alias->second;
			found = eyeshineMaskCache.emplace(name, index).first;
		}
		if (found->second >= eyeshineMasks.size())
			return false;
		auto& mask = eyeshineMasks[found->second];
		if (std::ranges::find(mask.geometryNames, pass->geometry->name) == mask.geometryNames.end())
			return false;
		currentEyeshineMask = mask.view.Get();
		if (!mask.matchLogged) {
			SKSE::log::info("Night Eye: eyeshine mask {} matches geometry {}, texture {}", mask.filename, pass->geometry->name, name);
			mask.matchLogged = true;
		}
		return true;
	}

	ID3D11ShaderResourceView* GetEyeshineMask(RE::BSRenderPass* pass)
	{
		return UpdateEyeshineMask(pass) ? currentEyeshineMask : nullptr;
	}
}
