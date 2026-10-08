#include "GeometryHooks.h"
#include "NightEye.h"
#include "PostProcessState.h"

#include <algorithm>
#include <cstring>
#include <d3dcompiler.h>
#include <sstream>

namespace night_eye
{
	namespace
	{
		using Microsoft::WRL::ComPtr;
		ID3D11Device* device = nullptr;
		ID3D11DeviceContext* context = nullptr;
		ComPtr<ID3D11VertexShader> vertexShader;
		ComPtr<ID3D11PixelShader> pixelShader;
		ComPtr<ID3D11Texture2D> inputTexture;
		ComPtr<ID3D11ShaderResourceView> inputView;
		ComPtr<ID3D11SamplerState> sampler;
		ComPtr<ID3D11BlendState> blend;
		ComPtr<ID3D11RasterizerState> rasterizer;
		ComPtr<ID3D11DepthStencilState> depth;
		ComPtr<ID3D11Buffer> parameters;
		D3D11_TEXTURE2D_DESC inputDescription{};
		DXGI_FORMAT inputFormat = DXGI_FORMAT_UNKNOWN;
		std::atomic<bool> reload{ true };
		std::mutex statusMutex;
		std::string shaderStatus = "waiting";

		struct alignas(16) RenderData
		{
			std::array<float, 4> ExposureGammaLiftStrength;
			std::array<float, 4> Tint;
			std::array<float, 4> VFX;
			std::array<float, 4> BufferDim;
			std::array<float, 4> Anaglyph;
			std::array<float, 4> AnaglyphLeft;
			std::array<float, 4> AnaglyphRight;
		};
		static_assert(sizeof(RenderData) == 112);

		class ShaderIncludes : public ID3DInclude
		{
		public:
			HRESULT STDMETHODCALLTYPE Open(D3D_INCLUDE_TYPE, LPCSTR filename, LPCVOID, LPCVOID* data, UINT* size) override
			{
				const std::filesystem::path relative(filename);
				if (relative.is_absolute() || std::ranges::any_of(relative, [](const auto& part) { return part == ".."; }))
					return E_INVALIDARG;
				std::string text;
				if (relative == "NightEye/HostHDR.hlsli") {
					std::ifstream stream("Data/Shaders/Common/SharedData.hlsli");
					std::string source(std::istreambuf_iterator<char>{ stream }, {});
					const auto begin = source.find("cbuffer SharedData : register(b5)");
					const auto end = source.find("float4 HDRData;", begin);
					if (begin == std::string::npos || end == std::string::npos) {
						SKSE::log::error("Night Eye cannot resolve the host's HDR buffer declaration.");
						return E_FAIL;
					}
					text = "namespace NightEyeHost { " + source.substr(begin, end + std::string_view("float4 HDRData;").size() - begin) + " }; }";
				} else {
					std::ifstream stream(std::filesystem::path("Data/Shaders") / relative, std::ios::binary);
					if (!stream)
						return E_FAIL;
					text.assign(std::istreambuf_iterator<char>{ stream }, {});
				}
				if (text.size() > std::numeric_limits<UINT>::max())
					return E_FAIL;
				auto buffer = std::make_unique<char[]>(text.size());
				std::memcpy(buffer.get(), text.data(), text.size());
				*size = static_cast<UINT>(text.size());
				*data = buffer.release();
				return S_OK;
			}
			HRESULT STDMETHODCALLTYPE Close(LPCVOID data) override
			{
				delete[] static_cast<const char*>(data);
				return S_OK;
			}
		};

		void SetStatus(std::string value)
		{
			std::scoped_lock lock(statusMutex);
			shaderStatus = std::move(value);
		}

		bool PrepareShaders()
		{
			if (!reload.exchange(false))
				return vertexShader && pixelShader;
			ShaderIncludes includes;
			ComPtr<ID3DBlob> vertexCode, pixelCode, errors;
			const D3D_SHADER_MACRO vertexDefines[]{ { "VSHADER", "1" }, { nullptr, nullptr } };
			const D3D_SHADER_MACRO pixelDefines[]{ { "PSHADER", "1" }, { nullptr, nullptr } };
			constexpr UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;
			auto result = D3DCompileFromFile(L"Data/Shaders/NightEye/NightEye.hlsl", vertexDefines, &includes, "mainVS", "vs_5_0", flags, 0, &vertexCode, &errors);
			if (SUCCEEDED(result)) {
				errors.Reset();
				result = D3DCompileFromFile(L"Data/Shaders/NightEye/NightEye.hlsl", pixelDefines, &includes, "mainPS", "ps_5_0", flags, 0, &pixelCode, &errors);
			}
			ComPtr<ID3D11VertexShader> newVertex;
			ComPtr<ID3D11PixelShader> newPixel;
			if (SUCCEEDED(result))
				result = device->CreateVertexShader(vertexCode->GetBufferPointer(), vertexCode->GetBufferSize(), nullptr, &newVertex);
			if (SUCCEEDED(result))
				result = device->CreatePixelShader(pixelCode->GetBufferPointer(), pixelCode->GetBufferSize(), nullptr, &newPixel);
			if (FAILED(result)) {
				SKSE::log::error("Night Eye shader reload failed: 0x{:08X}, {}", static_cast<unsigned int>(result), errors ? static_cast<const char*>(errors->GetBufferPointer()) : "no compiler diagnostics");
				SetStatus("failed");
				return vertexShader && pixelShader;
			}
			SetResourceName(newVertex.Get(), "NightEye::FullscreenVS");
			SetResourceName(newPixel.Get(), "NightEye::PostProcessPS");
			vertexShader = std::move(newVertex);
			pixelShader = std::move(newPixel);
			SetStatus("ready");
			return true;
		}

		bool PrepareInput(ID3D11Texture2D* output, ID3D11RenderTargetView* view)
		{
			D3D11_TEXTURE2D_DESC description{};
			D3D11_RENDER_TARGET_VIEW_DESC viewDescription{};
			output->GetDesc(&description);
			view->GetDesc(&viewDescription);
			if (description.SampleDesc.Count != 1 || description.ArraySize != 1 || viewDescription.ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2D || viewDescription.Texture2D.MipSlice != 0)
				return false;
			if (std::memcmp(&description, &inputDescription, sizeof(description)) == 0 && inputFormat == viewDescription.Format)
				return inputTexture && inputView;
			ComPtr<ID3D11Texture2D> texture;
			ComPtr<ID3D11ShaderResourceView> newView;
			auto copyDescription = description;
			copyDescription.Usage = D3D11_USAGE_DEFAULT;
			copyDescription.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			copyDescription.CPUAccessFlags = 0;
			copyDescription.MiscFlags = 0;
			auto result = device->CreateTexture2D(&copyDescription, nullptr, &texture);
			D3D11_SHADER_RESOURCE_VIEW_DESC srv{};
			srv.Format = viewDescription.Format;
			srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srv.Texture2D.MipLevels = 1;
			if (SUCCEEDED(result))
				result = device->CreateShaderResourceView(texture.Get(), &srv, &newView);
			if (FAILED(result)) {
				SKSE::log::error("Night Eye scene copy failed: 0x{:08X}, {}x{}, format {}", static_cast<unsigned int>(result), description.Width, description.Height, static_cast<unsigned int>(description.Format));
				return false;
			}
			SetResourceName(texture.Get(), "NightEye::SceneCopy");
			SetResourceName(newView.Get(), "NightEye::SceneCopy SRV");
			inputDescription = description;
			inputFormat = viewDescription.Format;
			inputTexture = std::move(texture);
			inputView = std::move(newView);
			return true;
		}
	}

	void SetResourceName(ID3D11DeviceChild* resource, std::string_view name)
	{
		if (resource)
			resource->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(name.size()), name.data());
	}
	std::string GetShaderStatus()
	{
		std::scoped_lock lock(statusMutex);
		return shaderStatus;
	}
	void RequestShaderReload() { reload.store(true); }

	void InitializeRenderer()
	{
		auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
		if (!renderer)
			return;
		auto& data = renderer->GetRuntimeData();
		device = reinterpret_cast<ID3D11Device*>(RE::BSGraphics::Renderer::GetDevice());
		context = reinterpret_cast<ID3D11DeviceContext*>(data.context);
		if (!device || !context)
			return;
		D3D11_SAMPLER_DESC samplerDescription{};
		samplerDescription.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDescription.AddressU = samplerDescription.AddressV = samplerDescription.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDescription.MaxLOD = D3D11_FLOAT32_MAX;
		D3D11_BLEND_DESC blendDescription{};
		blendDescription.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		D3D11_RASTERIZER_DESC rasterizerDescription{};
		rasterizerDescription.FillMode = D3D11_FILL_SOLID;
		rasterizerDescription.CullMode = D3D11_CULL_NONE;
		rasterizerDescription.DepthClipEnable = true;
		D3D11_DEPTH_STENCIL_DESC depthDescription{};
		depthDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		depthDescription.DepthFunc = D3D11_COMPARISON_ALWAYS;
		D3D11_BUFFER_DESC bufferDescription{};
		bufferDescription.ByteWidth = sizeof(RenderData);
		bufferDescription.Usage = D3D11_USAGE_DYNAMIC;
		bufferDescription.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bufferDescription.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		auto result = device->CreateSamplerState(&samplerDescription, &sampler);
		if (SUCCEEDED(result))
			result = device->CreateBlendState(&blendDescription, &blend);
		if (SUCCEEDED(result))
			result = device->CreateRasterizerState(&rasterizerDescription, &rasterizer);
		if (SUCCEEDED(result))
			result = device->CreateDepthStencilState(&depthDescription, &depth);
		if (SUCCEEDED(result))
			result = device->CreateBuffer(&bufferDescription, nullptr, &parameters);
		if (FAILED(result)) {
			SKSE::log::error("Night Eye resource initialization failed: 0x{:08X}", static_cast<unsigned int>(result));
			SetStatus("failed");
			return;
		}
		SetResourceName(sampler.Get(), "NightEye::Sampler");
		SetResourceName(blend.Get(), "NightEye::BlendState");
		SetResourceName(rasterizer.Get(), "NightEye::Rasterizer");
		SetResourceName(depth.Get(), "NightEye::DepthState");
		SetResourceName(parameters.Get(), "NightEye::PostProcessParameters");
		InitializeEyeshineMasks(device);
		PrepareShaders();
		auto& window = data.renderWindows[0];
		InitializeGeometryHooks(device, context, reinterpret_cast<IDXGISwapChain*>(window.swapChain), reinterpret_cast<HWND>(window.hWnd));
		InitializePostProcessHook();
		BeginFrame();
	}

	void RenderPostProcess(RE::RENDER_TARGET output, ID3D11Buffer* hostSharedData)
	{
		if (!context || !parameters || GetActivation() <= 0.0f || static_cast<unsigned int>(output) >= RE::RENDER_TARGETS::kTOTAL || !PrepareShaders())
			return;
		auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
		if (!renderer)
			return;
		auto& target = renderer->GetRuntimeData().renderTargets[output];
		auto* texture = reinterpret_cast<ID3D11Texture2D*>(target.texture);
		auto* view = reinterpret_cast<ID3D11RenderTargetView*>(target.RTV);
		if (!texture || !view || !PrepareInput(texture, view))
			return;
		const auto settings = GetSettings();
		const float direction = settings.anaglyphSwap ? -1.0f : 1.0f;
		const RenderData data{
			{ settings.exposure, settings.gamma, settings.shadowLift, GetActivation() },
			{ settings.tintColor[0], settings.tintColor[1], settings.tintColor[2], settings.visionMode == 0 ? settings.tintAmount : 0.0f },
			{ settings.glow, settings.vignette, settings.grain, settings.customVFX ? settings.customAmount : 0.0f },
			{ 1.0f / static_cast<float>(inputDescription.Width), 1.0f / static_cast<float>(inputDescription.Height), GetElapsed(),
				(inputFormat == DXGI_FORMAT_R16G16B16A16_FLOAT || inputFormat == DXGI_FORMAT_R32G32B32A32_FLOAT) ? 1.0f : 0.0f },
			{ settings.anaglyphEnabled ? settings.anaglyphAmount : 0.0f, direction * settings.anaglyphHorizontal, direction * settings.anaglyphVertical, settings.anaglyphDesaturation },
			{ settings.anaglyphLeftColor[0], settings.anaglyphLeftColor[1], settings.anaglyphLeftColor[2], 0.0f },
			{ settings.anaglyphRightColor[0], settings.anaglyphRightColor[1], settings.anaglyphRightColor[2], 0.0f }
		};
		D3D11_MAPPED_SUBRESOURCE mapped{};
		if (FAILED(context->Map(parameters.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
			return;
		std::memcpy(mapped.pData, &data, sizeof(data));
		context->Unmap(parameters.Get(), 0);
		PostProcessState backup(context);
		context->OMSetRenderTargets(0, nullptr, nullptr);
		ID3D11ShaderResourceView* noResource = nullptr;
		context->PSSetShaderResources(0, 1, &noResource);
		context->CopyResource(inputTexture.Get(), texture);
		const D3D11_VIEWPORT viewport{ 0, 0, static_cast<float>(inputDescription.Width), static_cast<float>(inputDescription.Height), 0, 1 };
		context->RSSetViewports(1, &viewport);
		context->RSSetState(rasterizer.Get());
		context->OMSetRenderTargets(1, &view, nullptr);
		context->OMSetBlendState(blend.Get(), nullptr, D3D11_DEFAULT_SAMPLE_MASK);
		context->OMSetDepthStencilState(depth.Get(), 0);
		context->IASetInputLayout(nullptr);
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		context->GSSetShader(nullptr, nullptr, 0);
		context->HSSetShader(nullptr, nullptr, 0);
		context->DSSetShader(nullptr, nullptr, 0);
		context->VSSetShader(vertexShader.Get(), nullptr, 0);
		context->PSSetShader(pixelShader.Get(), nullptr, 0);
		auto* buffer = parameters.Get();
		auto* passSampler = sampler.Get();
		auto* scene = inputView.Get();
		context->PSSetConstantBuffers(0, 1, &buffer);
		context->PSSetConstantBuffers(5, 1, &hostSharedData);
		context->PSSetSamplers(0, 1, &passSampler);
		context->PSSetShaderResources(0, 1, &scene);
		context->Draw(3, 0);
	}
}
