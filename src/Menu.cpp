#include "Menu.h"
#include "NightEye.h"
#include "PostProcessState.h"
#include "SettingsLimits.h"
#include "ShaderCompiler.h"

#include <CommCtrl.h>
#include <algorithm>
#include <cctype>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

namespace night_eye::menu
{
	namespace
	{
		using Microsoft::WRL::ComPtr;
		constexpr auto kTranslationDirectory = L"Data/SKSE/Plugins/NightEye/Translations";
		constexpr std::uintmax_t kMaximumTranslationSize = 1024 * 1024;
		constexpr std::size_t kMaximumInputMessages = 512;
		constexpr float kInitialWidth = 650.0f;
		constexpr float kInitialHeight = 720.0f;
		constexpr std::array kShortcutKeys{ VK_F1, VK_F2, VK_F3, VK_F4, VK_F5, VK_F6, VK_F7, VK_F8, VK_F9, VK_F10, VK_F11, VK_F12 };
		constexpr std::array kMouseKeys{ VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2 };
		constexpr REX::EnumSet<RE::ControlMap::UEFlag, std::uint32_t> kBlockedControls{
			RE::ControlMap::UEFlag::kMovement, RE::ControlMap::UEFlag::kLooking,
			RE::ControlMap::UEFlag::kActivate, RE::ControlMap::UEFlag::kMenu,
			RE::ControlMap::UEFlag::kConsole, RE::ControlMap::UEFlag::kPOVSwitch,
			RE::ControlMap::UEFlag::kFighting, RE::ControlMap::UEFlag::kSneaking,
			RE::ControlMap::UEFlag::kMainFour, RE::ControlMap::UEFlag::kWheelZoom,
			RE::ControlMap::UEFlag::kJumping, RE::ControlMap::UEFlag::kVATS
		};

		struct Language
		{
			std::string id;
			std::string name;
		};

		struct WindowMessage
		{
			UINT message;
			WPARAM wParam;
			LPARAM lParam;
		};

		struct MenuState
		{
			std::recursive_mutex mutex;
			ImGuiContext* imgui = nullptr;
			HWND window = nullptr;
			ID3D11DeviceContext* context = nullptr;
			std::atomic<bool> open = false;
			bool initialized = false;
			bool shortcutDown = false;
			bool clearInput = false;
			bool wheelInputInstalled = false;
			std::atomic<float> wheel = 0.0f;
			std::vector<WindowMessage> inputMessages;
			bool controlsLocked = false;
			REX::EnumSet<RE::ControlMap::UEFlag, std::uint32_t> restoreControls;
			std::unordered_map<std::string, std::string> translations;
			std::vector<Language> languages;
			std::string language;
			std::string actionStatus;
		} state;

		class WheelInputSink : public RE::BSTEventSink<RE::InputEvent*>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* events, RE::BSTEventSource<RE::InputEvent*>*) override
			{
				if (events && state.open.load()) {
					for (auto event = *events; event; event = event->next) {
						if (event->GetDevice() != RE::INPUT_DEVICE::kMouse)
							continue;
						if (const auto button = event->AsButtonEvent(); button && button->IsPressed()) {
							const auto key = button->GetIDCode();
							if (key == RE::BSWin32MouseDevice::Key::kWheelUp || key == RE::BSWin32MouseDevice::Key::kWheelDown)
								state.wheel.fetch_add(key == RE::BSWin32MouseDevice::Key::kWheelUp ? button->Value() : -button->Value());
						}
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		} wheelInput;

		class ScopedImGuiContext
		{
		public:
			explicit ScopedImGuiContext(ImGuiContext* context) : previous(ImGui::GetCurrentContext()) { ImGui::SetCurrentContext(context); }
			~ScopedImGuiContext() { ImGui::SetCurrentContext(previous); }
			ScopedImGuiContext(const ScopedImGuiContext&) = delete;
			ScopedImGuiContext& operator=(const ScopedImGuiContext&) = delete;

		private:
			ImGuiContext* previous;
		};

		template <class Shader, auto GetShader, auto SetShader>
		class ScopedShader
		{
		public:
			explicit ScopedShader(ID3D11DeviceContext* context) : context(context) { (context->*GetShader)(saved.shader.GetAddressOf(), saved.instances.data(), &saved.count); }
			~ScopedShader() { (context->*SetShader)(saved.shader.Get(), saved.instances.data(), saved.count); }
			ScopedShader(const ScopedShader&) = delete;
			ScopedShader& operator=(const ScopedShader&) = delete;

		private:
			ID3D11DeviceContext* context;
			ShaderState<Shader> saved;
		};

		const char* T(std::string_view key, const char* fallback)
		{
			const auto translated = state.translations.find(std::string(key));
			return translated != state.translations.end() ? translated->second.c_str() : fallback;
		}

		nlohmann::json ReadTranslation(const std::filesystem::path& path)
		{
			if (std::filesystem::file_size(path) > kMaximumTranslationSize)
				throw std::runtime_error("Translation file exceeds the size limit");
			std::ifstream file(path);
			return nlohmann::json::parse(file);
		}

		void LoadTranslations(const std::string& language)
		{
			state.language = language;
			state.translations.clear();
			for (const auto& id : { std::string("en"), language }) {
				try {
					const auto path = std::filesystem::path(kTranslationDirectory) / (id + ".json");
					if (!std::filesystem::exists(path))
						continue;
					const auto translations = ReadTranslation(path);
					if (translations.is_object())
						for (const auto& [key, value] : translations.items())
							if (value.is_string())
							state.translations.insert_or_assign(key, value.get<std::string>());
				} catch (const std::exception& error) {
					SKSE::log::warn("Night Eye translation load failed for {}: {}", id, error.what());
				}
			}
		}

		void DiscoverLanguages()
		{
			state.languages = { { "en", "English" } };
			try {
				if (!std::filesystem::exists(kTranslationDirectory))
					return;
				for (const auto& entry : std::filesystem::directory_iterator(kTranslationDirectory)) {
					if (!entry.is_regular_file() || entry.path().extension() != ".json")
						continue;
					const auto id = entry.path().stem().string();
					if (id == "en" || id.size() > 32 || !std::ranges::all_of(id, [](unsigned char character) { return std::isalnum(character) || character == '_' || character == '-'; }))
						continue;
					const auto translations = ReadTranslation(entry.path());
					state.languages.push_back({ id, translations.value("language.name", id) });
				}
				std::ranges::sort(state.languages, {}, &Language::id);
			} catch (const std::exception& error) {
				SKSE::log::warn("Night Eye language discovery failed: {}", error.what());
			}
		}

		void UpdateControlLock()
		{
			if (auto tasks = SKSE::GetTaskInterface())
				tasks->AddTask([] {
					if (auto controls = RE::ControlMap::GetSingleton()) {
						const bool open = state.open.load();
						if (open && !state.controlsLocked) {
							state.restoreControls = controls->GetRuntimeData().enabledControls & kBlockedControls;
							controls->ToggleControls(kBlockedControls.get(), false, false);
							state.controlsLocked = true;
						} else if (!open && state.controlsLocked) {
							controls->ToggleControls(state.restoreControls.get(), true, false);
							state.restoreControls.reset();
							state.controlsLocked = false;
						}
					}
				});
		}

		void SetOpen(bool open)
		{
			if (state.open.exchange(open) != open) {
				const std::lock_guard lock(state.mutex);
				state.clearInput = true;
				state.inputMessages.clear();
				state.wheel.store(0.0f);
				UpdateControlLock();
			}
		}

		bool IsInputMessage(UINT message)
		{
			return (message >= WM_KEYFIRST && message <= WM_KEYLAST) ||
				(message >= WM_MOUSEFIRST && message <= WM_MOUSELAST) || message == WM_CHAR;
		}

		LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id, DWORD_PTR)
		{
			if (message == WM_NCDESTROY) {
				SetOpen(false);
				RemoveWindowSubclass(window, WindowProcedure, id);
				return DefSubclassProc(window, message, wParam, lParam);
			}
			{
				const std::lock_guard lock(state.mutex);
				if (state.initialized) {
					const auto toggleKey = GetSettings().menuKey;
					if ((message == WM_KEYDOWN || message == WM_SYSKEYDOWN) && wParam == toggleKey)
						return 0;
					if ((message == WM_KEYUP || message == WM_SYSKEYUP) && wParam == toggleKey)
						return 0;
					if (message == WM_KILLFOCUS)
						SetOpen(false);
					if (state.open.load()) {
						if (IsInputMessage(message)) {
							const bool wheelHandled = state.wheelInputInstalled && message == WM_MOUSEWHEEL;
							if (!wheelHandled) {
								if (state.inputMessages.size() == kMaximumInputMessages) {
									state.inputMessages.clear();
									state.clearInput = true;
								}
								state.inputMessages.push_back({ message, wParam, lParam });
							}
							return 0;
						}
					}
				}
			}
			return DefSubclassProc(window, message, wParam, lParam);
		}

		void InstallInputSubclass()
		{
			const std::lock_guard lock(state.mutex);
			if (!SetWindowSubclass(state.window, WindowProcedure, reinterpret_cast<UINT_PTR>(&state), 0))
				SKSE::log::error("Night Eye menu input initialization failed: Windows error {}", GetLastError());
			if (auto input = RE::BSInputDeviceManager::GetSingleton()) {
				input->AddEventSink(&wheelInput);
				state.wheelInputInstalled = true;
			}
		}

		bool Slider(std::string_view key, const char* fallback, float& value, limits::Range range, const char* format = "%.2f")
		{
			return ImGui::SliderFloat(T(key, fallback), &value, range.minimum, range.maximum, format);
		}

		void DrawGeometryStatus()
		{
			const auto compiler = GetCompilerState();
			if (!compiler.installed || (!compiler.lightingRecognized && !compiler.effectRecognized))
				ImGui::TextWrapped("%s", T("geometry.unavailable", "Eyeshine and actor vision are unavailable with the installed graphics shaders. Check NightEye.log for details."));
			else if (!compiler.lightingRecognized || !compiler.effectRecognized)
				ImGui::TextWrapped("%s", T("geometry.partial", "Some installed graphics shaders are unsupported. Eyeshine and actor vision may be incomplete. Check NightEye.log for details."));
			else
				ImGui::TextWrapped("%s", T("geometry.available", "Eyeshine and actor vision shader support is available."));
			if (compiler.fallbacks > 0)
				ImGui::TextWrapped("%s", T("geometry.fallback", "Some graphics shaders fell back to their original version. Eyeshine or actor vision may be incomplete. Check NightEye.log for details."));
		}

		void DrawControls(Settings& settings)
		{
			ImGui::Checkbox(T("enable", "Enable Night Eye"), &settings.enabled);
			ImGui::TextUnformatted(IsEffectActive() ? T("active", "Night Eye is active") : T("inactive", "Night Eye is inactive"));
			if (ImGui::BeginTabBar("NightEyeTabs")) {
				if (ImGui::BeginTabItem(T("tab.visibility", "Visibility"))) {
					Slider("exposure", "Exposure (stops)", settings.exposure, limits::Exposure);
					Slider("gamma", "Shadow Gamma", settings.gamma, limits::Gamma);
					Slider("shadow_lift", "Shadow Lift", settings.shadowLift, limits::ShadowLift, "%.3f");
					Slider("transition", "Transition (seconds)", settings.transitionSeconds, limits::Transition);
					ImGui::ColorEdit3(T("tint_color", "Tint Color"), settings.tintColor.data());
					Slider("tint_amount", "Tint Amount", settings.tintAmount, limits::Unit);
					Slider("glow", "Glow", settings.glow, limits::Unit);
					Slider("vignette", "Vignette", settings.vignette, limits::Unit);
					Slider("grain", "Grain", settings.grain, limits::Grain, "%.3f");
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(T("tab.eyes", "Eyeshine"))) {
					DrawGeometryStatus();
					Slider("eyeshine_amount", "Eyeshine Strength", settings.eyeshineAmount, limits::Eyeshine);
					ImGui::ColorEdit3(T("eyeshine_color", "Eyeshine Color"), settings.eyeshineColor.data());
					ImGui::TextWrapped("%s", T("eye_compatibility", "Affects human and creature eye meshes, plus supported creature body atlases. Custom combined meshes need an eyeshine mask."));
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(T("tab.anaglyph", "Anaglyph"))) {
					ImGui::Checkbox(T("anaglyph_enable", "Enable Colour Split"), &settings.anaglyphEnabled);
					Slider("anaglyph_amount", "Colour Split Amount", settings.anaglyphAmount, limits::Unit);
					Slider("anaglyph_horizontal", "Horizontal Separation (pixels)", settings.anaglyphHorizontal, limits::Separation, "%.1f");
					Slider("anaglyph_vertical", "Vertical Separation (pixels)", settings.anaglyphVertical, limits::Separation, "%.1f");
					Slider("anaglyph_desaturation", "Colour Split Desaturation", settings.anaglyphDesaturation, limits::Unit);
					ImGui::ColorEdit3(T("anaglyph_left", "Left Channel Colour"), settings.anaglyphLeftColor.data());
					ImGui::ColorEdit3(T("anaglyph_right", "Right Channel Colour"), settings.anaglyphRightColor.data());
					ImGui::Checkbox(T("anaglyph_swap", "Swap Channels"), &settings.anaglyphSwap);
					ImGui::TextWrapped("%s", T("anaglyph_description", "Splits one camera image into two adjustable colour channels. Separation is measured at the final output resolution."));
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(T("tab.actors", "Actor Vision"))) {
					DrawGeometryStatus();
					const std::array modes{ T("vision_off", "Off"), T("vision_infrared", "Infrared Style"), T("vision_predator", "Predator") };
					ImGui::Combo(T("vision_mode", "Vision Mode"), &settings.visionMode, modes.data(), static_cast<int>(modes.size()));
					Slider("actor_amount", "Actor Highlight Amount", settings.actorVisionAmount, limits::Unit);
					Slider("actor_brightness", "Actor Highlight Brightness", settings.actorVisionBrightness, limits::ActorBrightness);
					ImGui::ColorEdit3(T("enemy_color", "Enemy Colour"), settings.enemyColor.data());
					ImGui::ColorEdit3(T("ally_color", "Ally Colour"), settings.allyColor.data());
					ImGui::ColorEdit3(T("neutral_color", "Neutral Colour"), settings.neutralColor.data());
					ImGui::TextWrapped("%s", T("vision_description", "Highlights visible living actors using their game relationships, including modded creatures. Predator uses the editable relationship colours; Infrared Style uses simulated heat colours. Global tint is bypassed while actor vision is enabled."));
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(T("tab.custom", "Custom VFX"))) {
					ImGui::Checkbox(T("custom_enable", "Enable Custom VFX"), &settings.customVFX);
					Slider("custom_amount", "Custom VFX Amount", settings.customAmount, limits::Unit);
					ImGui::TextWrapped("%s", T("custom_file", "Edit Data/Shaders/NightEye/CustomVFX.hlsli, then reload the shader."));
					if (ImGui::Button(T("reload_shader", "Reload Night Eye Shader")))
						RequestShaderReload();
					const auto shaderStatus = GetShaderStatus();
					if (shaderStatus == "ready")
						ImGui::TextWrapped("%s", T("shader.ready", "Night Eye shader is ready."));
					else if (shaderStatus == "failed")
						ImGui::TextWrapped("%s", T("shader.failed", "The shader did not compile. Correct the file and reload. The previous shader is retained when available. Check NightEye.log for details."));
					else
						ImGui::TextWrapped("%s", T("shader.waiting", "Waiting to prepare the Night Eye shader."));
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(T("tab.menu", "Menu"))) {
					const auto shortcutName = [](int key) { return std::format("F{}", key - VK_F1 + 1); };
					if (ImGui::BeginCombo(T("menu_key", "Menu Shortcut"), shortcutName(static_cast<int>(settings.menuKey)).c_str())) {
						for (const auto key : kShortcutKeys) {
							if (ImGui::Selectable(shortcutName(key).c_str(), settings.menuKey == static_cast<unsigned int>(key)))
								settings.menuKey = static_cast<unsigned int>(key);
						}
						ImGui::EndCombo();
					}
					const auto language = std::ranges::find(state.languages, settings.language, &Language::id);
					const auto& languageName = language != state.languages.end() ? language->name : settings.language;
					if (ImGui::BeginCombo(T("language", "Language"), languageName.c_str())) {
						for (const auto& item : state.languages)
							if (ImGui::Selectable(item.name.c_str(), settings.language == item.id))
								settings.language = item.id;
						ImGui::EndCombo();
					}
					ImGui::TextWrapped("%s", T("menu_description", "This window controls the Night Eye addon. Changes take effect immediately; Save keeps them for the next game launch."));
					ImGui::EndTabItem();
				}
				ImGui::EndTabBar();
			}
		}

		void NameBackendResources(const ImDrawList*, const ImDrawCmd*)
		{
			const auto context = state.context;
			ComPtr<ID3D11VertexShader> vertexShader;
			ComPtr<ID3D11PixelShader> pixelShader;
			ComPtr<ID3D11Buffer> vertexBuffer;
			ComPtr<ID3D11Buffer> indexBuffer;
			ComPtr<ID3D11InputLayout> inputLayout;
			ComPtr<ID3D11RasterizerState> rasterizer;
			ComPtr<ID3D11BlendState> blend;
			ComPtr<ID3D11DepthStencilState> depth;
			ComPtr<ID3D11ShaderResourceView> textureView;
			ComPtr<ID3D11Resource> texture;
			UINT stride = 0, offset = 0;
			DXGI_FORMAT format{};
			context->VSGetShader(vertexShader.GetAddressOf(), nullptr, nullptr);
			context->PSGetShader(pixelShader.GetAddressOf(), nullptr, nullptr);
			context->IAGetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
			context->IAGetIndexBuffer(indexBuffer.GetAddressOf(), &format, &offset);
			context->IAGetInputLayout(inputLayout.GetAddressOf());
			context->RSGetState(rasterizer.GetAddressOf());
			context->OMGetBlendState(blend.GetAddressOf(), nullptr, nullptr);
			context->OMGetDepthStencilState(depth.GetAddressOf(), nullptr);
			context->PSGetShaderResources(0, 1, textureView.GetAddressOf());
			if (textureView)
				textureView->GetResource(texture.GetAddressOf());
			SetResourceName(vertexShader.Get(), "NightEye::Menu VS");
			SetResourceName(pixelShader.Get(), "NightEye::Menu PS");
			SetResourceName(vertexBuffer.Get(), "NightEye::Menu VertexBuffer");
			SetResourceName(indexBuffer.Get(), "NightEye::Menu IndexBuffer");
			SetResourceName(inputLayout.Get(), "NightEye::Menu InputLayout");
			SetResourceName(rasterizer.Get(), "NightEye::Menu Rasterizer");
			SetResourceName(blend.Get(), "NightEye::Menu BlendState");
			SetResourceName(depth.Get(), "NightEye::Menu DepthState");
			SetResourceName(textureView.Get(), "NightEye::Menu Font SRV");
			SetResourceName(texture.Get(), "NightEye::Menu Font");
			if (const auto backend = static_cast<ImGui_ImplDX11_RenderState*>(ImGui::GetPlatformIO().Renderer_RenderState)) {
				SetResourceName(backend->VertexConstantBuffer, "NightEye::Menu Parameters");
				SetResourceName(backend->SamplerLinear, "NightEye::Menu LinearSampler");
				SetResourceName(backend->SamplerNearest, "NightEye::Menu NearestSampler");
			}
		}
	}

	void Initialize(HWND window, ID3D11Device* device, ID3D11DeviceContext* context)
	{
		const std::lock_guard lock(state.mutex);
		if (state.initialized || !window || !device || !context)
			return;
		state.window = window;
		state.context = context;
		const auto previousContext = ImGui::GetCurrentContext();
		state.imgui = ImGui::CreateContext();
		ImGui::SetCurrentContext(previousContext);
		ScopedImGuiContext imgui(state.imgui);
		auto& io = ImGui::GetIO();
		io.IniFilename = nullptr;
		io.LogFilename = nullptr;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NoMouseCursorChange;
		io.ConfigInputTrickleEventQueue = false;
		ImGui::StyleColorsDark();
		if (!ImGui_ImplWin32_Init(window) || !ImGui_ImplDX11_Init(device, context)) {
			SKSE::log::error("Night Eye menu backend initialization failed");
			if (io.BackendPlatformUserData)
				ImGui_ImplWin32_Shutdown();
			if (io.BackendRendererUserData)
				ImGui_ImplDX11_Shutdown();
			ImGui::DestroyContext(state.imgui);
			state.imgui = nullptr;
			return;
		}
		DiscoverLanguages();
		LoadTranslations(GetSettings().language);
		state.initialized = true;
		state.shortcutDown = (GetAsyncKeyState(static_cast<int>(GetSettings().menuKey)) & 0x8000) != 0;
		if (GetWindowThreadProcessId(window, nullptr) == GetCurrentThreadId())
			InstallInputSubclass();
		else if (auto tasks = SKSE::GetTaskInterface())
			tasks->AddTask(InstallInputSubclass);
		SKSE::log::info("Initialized independent Night Eye menu, shortcut F{}", GetSettings().menuKey - VK_F1 + 1);
	}

	void PollInput()
	{
		const std::lock_guard lock(state.mutex);
		if (!state.initialized)
			return;
		const auto settings = GetSettings();
		const bool focused = GetForegroundWindow() == state.window;
		const bool shortcutDown = focused && (GetAsyncKeyState(static_cast<int>(settings.menuKey)) & 0x8000) != 0;
		if (shortcutDown && !state.shortcutDown)
			SetOpen(!state.open.load());
		state.shortcutDown = shortcutDown;
		if (!focused)
			SetOpen(false);
	}

	void Render()
	{
		const std::lock_guard lock(state.mutex);
		if (!state.initialized || !state.open.load())
			return;
		auto settings = GetSettings();
		ScopedImGuiContext context(state.imgui);
		auto& io = ImGui::GetIO();
		if (state.clearInput) {
			io.ClearInputKeys();
			io.ClearInputMouse();
			state.clearInput = false;
		}
		for (const auto& message : state.inputMessages)
			ImGui_ImplWin32_WndProcHandler(state.window, message.message, message.wParam, message.lParam);
		state.inputMessages.clear();
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		for (std::size_t button = 0; button < kMouseKeys.size(); ++button)
			io.AddMouseButtonEvent(static_cast<int>(button), (GetAsyncKeyState(kMouseKeys[button]) & 0x8000) != 0);
		if (const auto wheel = state.wheel.exchange(0.0f); wheel != 0.0f)
			io.AddMouseWheelEvent(0.0f, wheel);
		io.MouseDrawCursor = true;
		ImGui::NewFrame();
		if (settings.language != state.language)
			LoadTranslations(settings.language);
		ImGui::SetNextWindowSize({ kInitialWidth, kInitialHeight }, ImGuiCond_FirstUseEver);
		bool open = state.open.load();
		if (ImGui::Begin(T("window.title", "Truly Night Eye"), &open)) {
			DrawControls(settings);
			UpdateSettings(settings);
			ImGui::Separator();
			if (ImGui::Button(T("settings.save", "Save")))
				state.actionStatus = SaveSettings() ? "settings.saved" : "settings.save_failed";
			ImGui::SameLine();
			if (ImGui::Button(T("settings.load", "Load Saved"))) {
				state.actionStatus = LoadSettings() ? "settings.loaded" : "settings.load_failed";
			}
			ImGui::SameLine();
			if (ImGui::Button(T("settings.reset", "Restore Defaults"))) {
				ResetSettings();
				state.actionStatus = "settings.reset_done";
			}
			if (!state.actionStatus.empty())
				ImGui::TextWrapped("%s", T(state.actionStatus, ""));
			ImGui::GetWindowDrawList()->AddCallback(NameBackendResources, nullptr);
		}
		ImGui::End();
		SetOpen(open);
		ImGui::Render();
		ScopedShader<ID3D11VertexShader, &ID3D11DeviceContext::VSGetShader, &ID3D11DeviceContext::VSSetShader> vertex(state.context);
		ScopedShader<ID3D11PixelShader, &ID3D11DeviceContext::PSGetShader, &ID3D11DeviceContext::PSSetShader> pixel(state.context);
		ScopedShader<ID3D11GeometryShader, &ID3D11DeviceContext::GSGetShader, &ID3D11DeviceContext::GSSetShader> geometry(state.context);
		ScopedShader<ID3D11HullShader, &ID3D11DeviceContext::HSGetShader, &ID3D11DeviceContext::HSSetShader> hull(state.context);
		ScopedShader<ID3D11DomainShader, &ID3D11DeviceContext::DSGetShader, &ID3D11DeviceContext::DSSetShader> domain(state.context);
		ScopedShader<ID3D11ComputeShader, &ID3D11DeviceContext::CSGetShader, &ID3D11DeviceContext::CSSetShader> compute(state.context);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	}

	bool IsOpen()
	{
		return state.open.load();
	}

	void Close()
	{
		SetOpen(false);
	}
}
