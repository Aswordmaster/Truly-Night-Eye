#include "NightEye.h"

namespace night_eye
{
	namespace
	{
		constexpr std::uintptr_t kAETonemapCallOffset = 0x178;
		constexpr unsigned char kRelativeCallOpcode = 0xE8;
		constexpr UINT kHostSharedBufferSlot = 5;
		using RenderEffect = void (*)(RE::ImageSpaceManager*, std::uint32_t, std::uint32_t, std::uint32_t, RE::ImageSpaceShaderParam*);
		RenderEffect original = nullptr;

		void AfterTonemap(RE::ImageSpaceManager* manager, std::uint32_t effectIndex, std::uint32_t input, std::uint32_t output, RE::ImageSpaceShaderParam* parameters)
		{
			Microsoft::WRL::ComPtr<ID3D11Buffer> hostSharedData;
			auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
			if (renderer) {
				auto* context = reinterpret_cast<ID3D11DeviceContext*>(renderer->GetRuntimeData().context);
				if (context)
					context->PSGetConstantBuffers(kHostSharedBufferSlot, 1, hostSharedData.GetAddressOf());
			}
			original(manager, effectIndex, input, output, parameters);
			RenderPostProcess(static_cast<RE::RENDER_TARGET>(output), hostSharedData.Get());
		}
	}

	void InitializePostProcessHook()
	{
		if (REL::Module::get().version() != SKSE::RUNTIME_SSE_1_6_1170) {
			SKSE::log::error("Night Eye postprocessing is unavailable for this unverified runtime.");
			return;
		}
		const auto call = REL::RelocationID(99023, 105674).address() + kAETonemapCallOffset;
		if (*reinterpret_cast<const unsigned char*>(call) != kRelativeCallOpcode) {
			SKSE::log::error("Night Eye tone-mapping call at {:X} is not a supported relative CALL.", call);
			return;
		}
		original = reinterpret_cast<RenderEffect>(SKSE::GetTrampoline().write_call<5>(call, AfterTonemap));
		SKSE::log::info("Night Eye chained the existing tone-mapping renderer at {:X}.", call);
	}
}
