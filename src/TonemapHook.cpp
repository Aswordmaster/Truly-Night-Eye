#include "NightEye.h"
#include "RuntimeCompatibility.h"

#include <algorithm>
#include <span>

namespace night_eye
{
	namespace
	{
		constexpr std::uintptr_t kAETonemapCallOffset = 0x178;
		constexpr unsigned char kRelativeCallOpcode = 0xE8;
		constexpr std::array<unsigned char, 8> kAETonemapCallArguments{ 0xBA, 0x22, 0x00, 0x00, 0x00, 0x48, 0x8B, 0xCB };
		constexpr std::array<unsigned char, 7> kSEAlternateTonemapCallArguments{ 0x48, 0x8B, 0x90, 0xF0, 0x00, 0x00, 0x00 };
		constexpr std::array<unsigned char, 10> kSETonemapCallArguments{ 0x48, 0x8B, 0x90, 0x10, 0x01, 0x00, 0x00, 0x48, 0x8B, 0xCD };
		struct TonemapCall
		{
			std::uintptr_t offset;
			std::span<const unsigned char> arguments;
		};
		constexpr std::array kSETonemapCalls{
			TonemapCall{ 0x1EA, kSEAlternateTonemapCallArguments },
			TonemapCall{ 0x230, kSETonemapCallArguments }
		};
		constexpr UINT kHostSharedBufferSlot = 5;
		using AERenderEffect = void (*)(RE::ImageSpaceManager*, std::uint32_t, std::uint32_t, std::uint32_t, RE::ImageSpaceShaderParam*);
		using SERenderEffect = void (*)(RE::ImageSpaceManager*, RE::ImageSpaceEffect*, std::uint32_t, std::uint32_t, RE::ImageSpaceShaderParam*);
		AERenderEffect originalAE = nullptr;
		std::array<SERenderEffect, kSETonemapCalls.size()> originalSE{};

		template <class Effect>
		void RenderAfterTonemap(void (*render)(RE::ImageSpaceManager*, Effect, std::uint32_t, std::uint32_t, RE::ImageSpaceShaderParam*), RE::ImageSpaceManager* manager, Effect effect, std::uint32_t input, std::uint32_t output, RE::ImageSpaceShaderParam* parameters)
		{
			Microsoft::WRL::ComPtr<ID3D11Buffer> hostSharedData;
			auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
			if (renderer) {
				auto* context = reinterpret_cast<ID3D11DeviceContext*>(renderer->GetRuntimeData().context);
				if (context)
					context->PSGetConstantBuffers(kHostSharedBufferSlot, 1, hostSharedData.GetAddressOf());
			}
			render(manager, effect, input, output, parameters);
			RenderPostProcess(static_cast<RE::RENDER_TARGET>(output), hostSharedData.Get());
		}

		void AfterAETonemap(RE::ImageSpaceManager* manager, std::uint32_t effectIndex, std::uint32_t input, std::uint32_t output, RE::ImageSpaceShaderParam* parameters)
		{
			RenderAfterTonemap(originalAE, manager, effectIndex, input, output, parameters);
		}

		template <std::size_t Index>
		void AfterSETonemap(RE::ImageSpaceManager* manager, RE::ImageSpaceEffect* effect, std::uint32_t input, std::uint32_t output, RE::ImageSpaceShaderParam* parameters)
		{
			RenderAfterTonemap(originalSE[Index], manager, effect, input, output, parameters);
		}

		bool ValidateTonemapCall(std::uintptr_t call, std::span<const unsigned char> arguments)
		{
			const auto* bytes = reinterpret_cast<const unsigned char*>(call);
			if (*bytes != kRelativeCallOpcode || !std::equal(arguments.begin(), arguments.end(), bytes - arguments.size())) {
				SKSE::log::error("Night Eye tone-mapping call at {:X} has unsupported instructions.", call);
				return false;
			}
			return true;
		}
	}

	void InitializePostProcessHook()
	{
		if (!IsSupportedRuntime(REL::Module::get().version())) {
			SKSE::log::error("Night Eye postprocessing is unavailable for this unverified runtime.");
			return;
		}
		const auto caller = REL::RelocationID(99023, 105674).address();
		if (REL::Module::get().version() == SKSE::RUNTIME_SSE_1_5_97) {
			for (const auto& call : kSETonemapCalls) {
				if (!ValidateTonemapCall(caller + call.offset, call.arguments))
					return;
			}
			constexpr std::array<SERenderEffect, kSETonemapCalls.size()> callbacks{ AfterSETonemap<0>, AfterSETonemap<1> };
			for (std::size_t index = 0; index < kSETonemapCalls.size(); ++index) {
				const auto call = caller + kSETonemapCalls[index].offset;
				originalSE[index] = reinterpret_cast<SERenderEffect>(SKSE::GetTrampoline().write_call<5>(call, callbacks[index]));
				SKSE::log::info("Night Eye chained the existing SE tone-mapping renderer at {:X}.", call);
			}
			return;
		}
		const auto call = caller + kAETonemapCallOffset;
		if (!ValidateTonemapCall(call, kAETonemapCallArguments))
			return;
		originalAE = reinterpret_cast<AERenderEffect>(SKSE::GetTrampoline().write_call<5>(call, AfterAETonemap));
		SKSE::log::info("Night Eye chained the existing tone-mapping renderer at {:X}.", call);
	}
}
