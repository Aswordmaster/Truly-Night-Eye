#include "NightEye.h"
#include "SettingsLimits.h"

#include <algorithm>
#include <cmath>
#include <cctype>

namespace night_eye
{
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
		Settings, enabled, exposure, gamma, shadowLift, tintColor, tintAmount,
		glow, removeNightEyeBlur, vignette, grain, transitionSeconds, eyeshineAmount, eyeshineColor,
		customVFX, customAmount, anaglyphEnabled, anaglyphAmount, anaglyphHorizontal,
		anaglyphVertical, anaglyphDesaturation, anaglyphLeftColor, anaglyphRightColor,
		anaglyphSwap, visionMode, actorVisionAmount, actorVisionBrightness, enemyColor,
		allyColor, neutralColor, menuKey, menuScale, language)

	namespace
	{
		constexpr auto kSettingsPath = L"Data/SKSE/Plugins/NightEye/Settings.json";
		constexpr auto kLegacySettingsPath = L"Data/SKSE/Plugins/CommunityShaders/SettingsUser.json";
		constexpr std::uintmax_t kMaximumSettingsSize = 1024 * 1024;
		constexpr std::size_t kMaximumLanguageLength = 32;
		constexpr int kSettingsVersion = 1;
		std::mutex settingsMutex;
		Settings currentSettings;

		struct ScalarRange
		{
			float Settings::* field;
			limits::Range range;
		};
		constexpr std::array kScalarRanges{
			ScalarRange{ &Settings::exposure, limits::Exposure },
			ScalarRange{ &Settings::gamma, limits::Gamma },
			ScalarRange{ &Settings::shadowLift, limits::ShadowLift },
			ScalarRange{ &Settings::tintAmount, limits::Unit },
			ScalarRange{ &Settings::glow, limits::Unit },
			ScalarRange{ &Settings::vignette, limits::Unit },
			ScalarRange{ &Settings::grain, limits::Grain },
			ScalarRange{ &Settings::transitionSeconds, limits::Transition },
			ScalarRange{ &Settings::eyeshineAmount, limits::Eyeshine },
			ScalarRange{ &Settings::customAmount, limits::Unit },
			ScalarRange{ &Settings::anaglyphAmount, limits::Unit },
			ScalarRange{ &Settings::anaglyphHorizontal, limits::Separation },
			ScalarRange{ &Settings::anaglyphVertical, limits::Separation },
			ScalarRange{ &Settings::anaglyphDesaturation, limits::Unit },
			ScalarRange{ &Settings::actorVisionAmount, limits::Unit },
			ScalarRange{ &Settings::actorVisionBrightness, limits::ActorBrightness },
			ScalarRange{ &Settings::menuScale, limits::MenuScale }
		};

		void Sanitize(Settings& settings)
		{
			const Settings defaults;
			for (const auto& range : kScalarRanges) {
				auto& value = settings.*range.field;
				value = std::isfinite(value) ? std::clamp(value, range.range.minimum, range.range.maximum) : defaults.*range.field;
			}
			settings.visionMode = std::clamp(settings.visionMode, 0, limits::MaximumVisionMode);
			for (auto field : { &Settings::tintColor, &Settings::eyeshineColor, &Settings::anaglyphLeftColor,
					 &Settings::anaglyphRightColor, &Settings::enemyColor, &Settings::allyColor, &Settings::neutralColor }) {
				auto& color = settings.*field;
				for (size_t component = 0; component < color.size(); ++component)
					color[component] = std::isfinite(color[component]) ? std::clamp(color[component], limits::Unit.minimum, limits::Unit.maximum) : (defaults.*field)[component];
			}
			if (settings.menuKey < VK_F1 || settings.menuKey > VK_F12)
				settings.menuKey = defaults.menuKey;
			if (settings.language.empty() || settings.language.size() > kMaximumLanguageLength ||
				!std::ranges::all_of(settings.language, [](unsigned char character) { return std::isalnum(character) || character == '-' || character == '_'; }))
				settings.language = defaults.language;
		}

		nlohmann::json ReadSettings(const std::filesystem::path& path)
		{
			if (!std::filesystem::exists(path))
				return {};
			if (std::filesystem::file_size(path) > kMaximumSettingsSize)
				throw std::runtime_error("Settings file exceeds the size limit");
			std::ifstream file(path);
			if (!file)
				throw std::runtime_error("Settings file could not be opened");
			return nlohmann::json::parse(file);
		}
	}

	Settings GetSettings()
	{
		const std::lock_guard lock(settingsMutex);
		return currentSettings;
	}

	void UpdateSettings(const Settings& settings)
	{
		auto sanitized = settings;
		Sanitize(sanitized);
		const std::lock_guard lock(settingsMutex);
		currentSettings = std::move(sanitized);
	}

	bool LoadSettings()
	{
		try {
			if (std::filesystem::exists(kSettingsPath)) {
				const auto settings = ReadSettings(kSettingsPath);
				if (!settings.is_object() || !settings.contains("Version") || !settings.at("Version").is_number_integer() || settings.at("Version") != kSettingsVersion)
					throw std::runtime_error("Unsupported settings document version");
				UpdateSettings(settings.at("Settings").get<Settings>());
				SKSE::log::info("Loaded standalone Night Eye settings");
				return true;
			}
			const auto legacy = ReadSettings(kLegacySettingsPath);
			if (legacy.is_object() && legacy.contains("Night Eye")) {
				UpdateSettings(legacy.at("Night Eye").get<Settings>());
				SKSE::log::info("Imported Night Eye settings from the existing core configuration without modifying it");
				return true;
			}
			ResetSettings();
			return true;
		} catch (const std::exception& error) {
			SKSE::log::error("Night Eye settings load failed: {}", error.what());
			ResetSettings();
			return false;
		}
	}

	bool SaveSettings()
	{
		try {
			const std::filesystem::path path(kSettingsPath);
			std::filesystem::create_directories(path.parent_path());
			const auto temporary = std::filesystem::path(path).replace_extension(L"json.tmp");
			{
				std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
				output.exceptions(std::ios::badbit | std::ios::failbit);
				output << nlohmann::json{ { "Version", kSettingsVersion }, { "Settings", GetSettings() } }.dump(2) << '\n';
			}
			if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
				SKSE::log::error("Night Eye settings save failed: Windows error {}", GetLastError());
				return false;
			}
			SKSE::log::info("Saved standalone Night Eye settings");
			return true;
		} catch (const std::exception& error) {
			SKSE::log::error("Night Eye settings save failed: {}", error.what());
			return false;
		}
	}

	void ResetSettings()
	{
		UpdateSettings(Settings{});
	}
}
