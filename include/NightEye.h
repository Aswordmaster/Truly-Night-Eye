#pragma once

namespace night_eye
{
	struct Settings
	{
		bool enabled = true;
		float exposure = 1.0f;
		float gamma = 0.8f;
		float shadowLift = 0.01f;
		std::array<float, 3> tintColor{ 0.55f, 0.75f, 1.0f };
		float tintAmount = 0.0f;
		float glow = 0.0f;
		bool removeNightEyeBlur = false;
		float vignette = 0.0f;
		float grain = 0.0f;
		float transitionSeconds = 0.2f;
		float eyeshineAmount = 0.0f;
		std::array<float, 3> eyeshineColor{ 0.65f, 1.0f, 0.8f };
		bool customVFX = false;
		float customAmount = 1.0f;
		bool anaglyphEnabled = false;
		float anaglyphAmount = 1.0f;
		float anaglyphHorizontal = 4.0f;
		float anaglyphVertical = 0.0f;
		float anaglyphDesaturation = 0.8f;
		std::array<float, 3> anaglyphLeftColor{ 1.0f, 0.0f, 0.0f };
		std::array<float, 3> anaglyphRightColor{ 0.0f, 1.0f, 1.0f };
		bool anaglyphSwap = false;
		int visionMode = 0;
		float actorVisionAmount = 1.0f;
		float actorVisionBrightness = 1.0f;
		std::array<float, 3> enemyColor{ 1.0f, 0.03f, 0.01f };
		std::array<float, 3> allyColor{ 0.02f, 1.0f, 0.05f };
		std::array<float, 3> neutralColor{ 0.7f, 0.7f, 0.7f };
		unsigned int menuKey = VK_F10;
		float menuScale = 1.0f;
		std::string language = "en";
	};

	struct alignas(16) GeometryData
	{
		std::array<float, 4> Eye{};
		std::array<float, 4> Actor{};
		std::array<float, 4> Enemy{};
		std::array<float, 4> Ally{};
		std::array<float, 4> Neutral{};
		std::array<std::uint32_t, 4> Flags{};
	};
	static_assert(sizeof(GeometryData) == 96);
	inline constexpr UINT kGeometryBufferSlot = 13;
	inline constexpr UINT kEyeshineMaskSlot = 127;

	Settings GetSettings();
	void UpdateSettings(const Settings& settings);
	bool LoadSettings();
	bool SaveSettings();
	void ResetSettings();
	std::string GetShaderStatus();
	void RequestShaderReload();
	bool IsEffectActive();
	void InitializeCompiler();
	void Initialize();
	void OnGameLoaded();
	void BeginFrame();
	float GetActivation();
	float GetElapsed();
	GeometryData GetGeometryData(RE::BSRenderPass* pass);
	void InitializeEyeshineMasks(ID3D11Device* device);
	ID3D11ShaderResourceView* GetEyeshineMask(RE::BSRenderPass* pass);
	bool IsEyeGeometry(RE::BSRenderPass* pass);
	void SetResourceName(ID3D11DeviceChild* resource, std::string_view name);
	void InitializeRenderer();
	void RenderPostProcess(RE::RENDER_TARGET output, ID3D11Buffer* hostSharedData = nullptr);
	void InitializePostProcessHook();
}
