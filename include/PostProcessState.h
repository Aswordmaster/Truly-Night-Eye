#pragma once

namespace night_eye
{
	template <class Shader>
	struct ShaderState
	{
		Microsoft::WRL::ComPtr<Shader> shader;
		std::array<ID3D11ClassInstance*, D3D11_SHADER_MAX_INTERFACES> instances{};
		UINT count = D3D11_SHADER_MAX_INTERFACES;
		~ShaderState()
		{
			for (auto* instance : instances)
				if (instance)
					instance->Release();
		}
	};

	class PostProcessState
	{
	public:
		explicit PostProcessState(ID3D11DeviceContext* value) :
			context(value)
		{
			context->VSGetShader(vs.shader.GetAddressOf(), vs.instances.data(), &vs.count);
			context->PSGetShader(ps.shader.GetAddressOf(), ps.instances.data(), &ps.count);
			context->GSGetShader(gs.shader.GetAddressOf(), gs.instances.data(), &gs.count);
			context->HSGetShader(hs.shader.GetAddressOf(), hs.instances.data(), &hs.count);
			context->DSGetShader(ds.shader.GetAddressOf(), ds.instances.data(), &ds.count);
			context->IAGetInputLayout(layout.GetAddressOf());
			context->IAGetPrimitiveTopology(&topology);
			context->PSGetConstantBuffers(0, 1, buffer.GetAddressOf());
			context->PSGetConstantBuffers(5, 1, hostBuffer.GetAddressOf());
			context->PSGetShaderResources(0, 1, resource.GetAddressOf());
			context->PSGetSamplers(0, 1, sampler.GetAddressOf());
			context->RSGetState(rasterizer.GetAddressOf());
			context->RSGetViewports(&viewportCount, viewports.data());
			context->OMGetRenderTargets(static_cast<UINT>(targets.size()), targets.data(), depth.GetAddressOf());
			context->OMGetBlendState(blend.GetAddressOf(), blendFactor.data(), &sampleMask);
			context->OMGetDepthStencilState(depthState.GetAddressOf(), &stencil);
		}
		~PostProcessState()
		{
			ID3D11ShaderResourceView* noResource = nullptr;
			context->PSSetShaderResources(0, 1, &noResource);
			context->OMSetRenderTargets(static_cast<UINT>(targets.size()), targets.data(), depth.Get());
			context->OMSetBlendState(blend.Get(), blendFactor.data(), sampleMask);
			context->OMSetDepthStencilState(depthState.Get(), stencil);
			context->RSSetState(rasterizer.Get());
			context->RSSetViewports(viewportCount, viewports.data());
			context->IASetInputLayout(layout.Get());
			context->IASetPrimitiveTopology(topology);
			context->VSSetShader(vs.shader.Get(), vs.instances.data(), vs.count);
			context->PSSetShader(ps.shader.Get(), ps.instances.data(), ps.count);
			context->GSSetShader(gs.shader.Get(), gs.instances.data(), gs.count);
			context->HSSetShader(hs.shader.Get(), hs.instances.data(), hs.count);
			context->DSSetShader(ds.shader.Get(), ds.instances.data(), ds.count);
			auto* savedBuffer = buffer.Get();
			auto* savedResource = resource.Get();
			auto* savedSampler = sampler.Get();
			context->PSSetConstantBuffers(0, 1, &savedBuffer);
			auto* savedHostBuffer = hostBuffer.Get();
			context->PSSetConstantBuffers(5, 1, &savedHostBuffer);
			context->PSSetShaderResources(0, 1, &savedResource);
			context->PSSetSamplers(0, 1, &savedSampler);
			for (auto* target : targets)
				if (target)
					target->Release();
		}
		PostProcessState(const PostProcessState&) = delete;
		PostProcessState& operator=(const PostProcessState&) = delete;

	private:
		ID3D11DeviceContext* context;
		ShaderState<ID3D11VertexShader> vs;
		ShaderState<ID3D11PixelShader> ps;
		ShaderState<ID3D11GeometryShader> gs;
		ShaderState<ID3D11HullShader> hs;
		ShaderState<ID3D11DomainShader> ds;
		Microsoft::WRL::ComPtr<ID3D11InputLayout> layout;
		D3D11_PRIMITIVE_TOPOLOGY topology{};
		Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> hostBuffer;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> resource;
		Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer;
		std::array<D3D11_VIEWPORT, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> viewports{};
		UINT viewportCount = static_cast<UINT>(viewports.size());
		std::array<ID3D11RenderTargetView*, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> targets{};
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depth;
		Microsoft::WRL::ComPtr<ID3D11BlendState> blend;
		std::array<float, 4> blendFactor{};
		UINT sampleMask = 0;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthState;
		UINT stencil = 0;
	};
}
