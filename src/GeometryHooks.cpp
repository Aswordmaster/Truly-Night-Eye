#include "GeometryHooks.h"

#include "Menu.h"
#include "NightEye.h"
#include "ShaderCompiler.h"

#include <cstring>
#include <detours/detours.h>

namespace night_eye
{
	namespace
	{
		using Microsoft::WRL::ComPtr;
		using SetupGeometryFunction = void (*)(RE::BSShader*, RE::BSRenderPass*, std::uint32_t);
		using PresentFunction = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
		constexpr std::size_t kSetupGeometryIndex = 6;
		constexpr std::size_t kPresentIndex = 8;

		ComPtr<ID3D11Device> device;
		ComPtr<ID3D11DeviceContext> context;
		ComPtr<ID3D11Buffer> geometryBuffer;
		IDXGISwapChain* gameSwapChain = nullptr;
		SetupGeometryFunction originalLighting = nullptr;
		SetupGeometryFunction originalEffect = nullptr;
		PresentFunction originalPresent = nullptr;
		GeometryData previousData{};
		bool previousDataValid = false;
		bool geometryEnabled = true;
		bool installed = false;
		bool menuFailureLogged = false;
		bool frameFailureLogged = false;

		struct OutputMergerState
		{
			ID3D11DeviceContext* context;
			std::array<ComPtr<ID3D11RenderTargetView>, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> targets;
			std::array<ComPtr<ID3D11UnorderedAccessView>, D3D11_PS_CS_UAV_REGISTER_COUNT> unordered;
			ComPtr<ID3D11DepthStencilView> depth;

			explicit OutputMergerState(ID3D11DeviceContext* value) :
				context(value)
			{
				std::array<ID3D11RenderTargetView*, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> targetPointers{};
				std::array<ID3D11UnorderedAccessView*, D3D11_PS_CS_UAV_REGISTER_COUNT> unorderedPointers{};
				context->OMGetRenderTargets(static_cast<UINT>(targets.size()), targetPointers.data(), depth.GetAddressOf());
				context->OMGetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, 0,
					static_cast<UINT>(unordered.size()), unorderedPointers.data());
				for (std::size_t index = 0; index < targets.size(); ++index)
					targets[index].Attach(targetPointers[index]);
				for (std::size_t index = 0; index < unordered.size(); ++index)
					unordered[index].Attach(unorderedPointers[index]);
			}

			~OutputMergerState()
			{
				std::array<ID3D11RenderTargetView*, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> targetPointers{};
				std::array<ID3D11UnorderedAccessView*, D3D11_PS_CS_UAV_REGISTER_COUNT> unorderedPointers{};
				std::array<UINT, D3D11_PS_CS_UAV_REGISTER_COUNT> counters{};
				counters.fill(D3D11_KEEP_UNORDERED_ACCESS_VIEWS);
				UINT targetCount = 0;
				for (std::size_t index = 0; index < targets.size(); ++index) {
					targetPointers[index] = targets[index].Get();
					if (targetPointers[index])
						targetCount = static_cast<UINT>(index + 1);
				}
				for (std::size_t index = 0; index < unordered.size(); ++index)
					unorderedPointers[index] = unordered[index].Get();
				const auto unorderedCount = static_cast<UINT>(unordered.size()) - targetCount;
				context->OMSetRenderTargetsAndUnorderedAccessViews(targetCount, targetPointers.data(), depth.Get(),
					targetCount, unorderedCount, unorderedCount ? unorderedPointers.data() + targetCount : nullptr,
					unorderedCount ? counters.data() + targetCount : nullptr);
			}
		};

		void BindGeometry(RE::BSRenderPass* pass)
		{
			if (!context || !geometryBuffer)
				return;
			if (!geometryEnabled)
				return;
			auto data = GetGeometryData(pass);
			auto* mask = data.Eye[3] > 0.0f ? GetEyeshineMask(pass) : nullptr;
			data.Flags[0] = mask || (data.Eye[3] > 0.0f && IsEyeGeometry(pass)) ? 1u : 0u;
			data.Flags[1] = mask ? 1u : 0u;
			if (!previousDataValid || std::memcmp(&previousData, &data, sizeof(data)) != 0) {
				D3D11_MAPPED_SUBRESOURCE mapped{};
				const auto result = context->Map(geometryBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
				if (FAILED(result)) {
					SKSE::log::error("Night Eye geometry buffer map failed, HRESULT {:08X}; geometry effects disabled.",
						static_cast<std::uint32_t>(result));
					ID3D11Buffer* emptyBuffer = nullptr;
					ID3D11ShaderResourceView* emptyMask = nullptr;
					context->PSSetConstantBuffers(kGeometryBufferSlot, 1, &emptyBuffer);
					context->PSSetShaderResources(kEyeshineMaskSlot, 1, &emptyMask);
					geometryEnabled = false;
					return;
				}
				std::memcpy(mapped.pData, &data, sizeof(data));
				context->Unmap(geometryBuffer.Get(), 0);
				previousData = data;
				previousDataValid = true;
			}
			auto* buffer = geometryBuffer.Get();
			context->PSSetConstantBuffers(kGeometryBufferSlot, 1, &buffer);
			context->PSSetShaderResources(kEyeshineMaskSlot, 1, &mask);
		}

		void LightingSetup(RE::BSShader* shader, RE::BSRenderPass* pass, std::uint32_t flags)
		{
			originalLighting(shader, pass, flags);
			if (IsGeometryAdapterSupported(RE::BSShader::Type::Lighting))
				BindGeometry(pass);
		}

		void EffectSetup(RE::BSShader* shader, RE::BSRenderPass* pass, std::uint32_t flags)
		{
			originalEffect(shader, pass, flags);
			if (IsGeometryAdapterSupported(RE::BSShader::Type::Effect))
				BindGeometry(pass);
		}

		void DrawMenu(IDXGISwapChain* swapChain)
		{
			if (!menu::IsOpen())
				return;
			ComPtr<ID3D11Texture2D> backBuffer;
			ComPtr<ID3D11RenderTargetView> target;
			auto result = swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
			if (SUCCEEDED(result))
				result = device->CreateRenderTargetView(backBuffer.Get(), nullptr, target.GetAddressOf());
			if (FAILED(result)) {
				if (!menuFailureLogged)
					SKSE::log::error("Night Eye menu target unavailable, HRESULT {:08X}.", static_cast<std::uint32_t>(result));
				menuFailureLogged = true;
				return;
			}
			SetResourceName(target.Get(), "NightEye::MenuTarget RTV");
			OutputMergerState saved(context.Get());
			auto* view = target.Get();
			context->OMSetRenderTargets(1, &view, nullptr);
			menu::Render();
		}

		HRESULT STDMETHODCALLTYPE Present(IDXGISwapChain* swapChain, UINT interval, UINT flags)
		{
			if (swapChain != gameSwapChain || (flags & DXGI_PRESENT_TEST))
				return originalPresent(swapChain, interval, flags);
			try {
				menu::PollInput();
				DrawMenu(swapChain);
			} catch (const std::exception& error) {
				if (!menuFailureLogged)
					SKSE::log::error("Night Eye menu rendering failed: {}", error.what());
				menuFailureLogged = true;
			}
			const auto result = originalPresent(swapChain, interval, flags);
			try {
				BeginFrame();
			} catch (const std::exception& error) {
				if (!frameFailureLogged)
					SKSE::log::error("Night Eye frame update failed: {}", error.what());
				frameFailureLogged = true;
			}
			return result;
		}
	}

	void InitializeGeometryHooks(ID3D11Device* graphicsDevice, ID3D11DeviceContext* graphicsContext, IDXGISwapChain* swapChain, HWND window)
	{
		if (installed || !graphicsDevice || !graphicsContext || !swapChain || !window)
			return;
		if (graphicsContext->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE) {
			SKSE::log::error("Night Eye requires the game's immediate graphics context.");
			return;
		}
		D3D11_BUFFER_DESC description{};
		description.ByteWidth = sizeof(GeometryData);
		description.Usage = D3D11_USAGE_DYNAMIC;
		description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		const auto created = graphicsDevice->CreateBuffer(&description, nullptr, geometryBuffer.GetAddressOf());
		if (FAILED(created)) {
			SKSE::log::error("Night Eye geometry buffer creation failed, HRESULT {:08X}.", static_cast<std::uint32_t>(created));
			return;
		}
		SetResourceName(geometryBuffer.Get(), "NightEye::GeometryData");
		REL::Relocation<std::uintptr_t> lighting{ RE::VTABLE_BSLightingShader[0] };
		REL::Relocation<std::uintptr_t> effect{ RE::VTABLE_BSEffectShader[0] };
		originalPresent = reinterpret_cast<PresentFunction>((*reinterpret_cast<void***>(swapChain))[kPresentIndex]);
		auto result = DetourTransactionBegin();
		if (result == NO_ERROR) {
			result = DetourUpdateThread(GetCurrentThread());
			if (result == NO_ERROR)
				result = DetourAttach(reinterpret_cast<PVOID*>(&originalPresent), reinterpret_cast<PVOID>(Present));
			if (result == NO_ERROR)
				result = DetourTransactionCommit();
			else
				DetourTransactionAbort();
		}
		if (result != NO_ERROR) {
			geometryBuffer.Reset();
			SKSE::log::error("Night Eye Present hook installation failed, code {}.", result);
			return;
		}
		device = graphicsDevice;
		context = graphicsContext;
		gameSwapChain = swapChain;
		originalLighting = reinterpret_cast<SetupGeometryFunction>(lighting.write_vfunc(kSetupGeometryIndex, LightingSetup));
		originalEffect = reinterpret_cast<SetupGeometryFunction>(effect.write_vfunc(kSetupGeometryIndex, EffectSetup));
		installed = true;
		try {
			menu::Initialize(window, graphicsDevice, graphicsContext);
		} catch (const std::exception& error) {
			SKSE::log::error("Night Eye menu initialization failed: {}", error.what());
			menuFailureLogged = true;
		}
		SKSE::log::info("Night Eye chained Lighting/Effect SetupGeometry and game Present; geometry b{}, mask t{}.",
			kGeometryBufferSlot, kEyeshineMaskSlot);
	}
}
