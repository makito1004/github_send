#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <vector>

enum class sampler_state { point, linear, anisotropic, linear_border_black, linear_border_white, linear_mirror, comparison_linear_border_white };
enum class depth_stencil_state { zt_on_zw_on, zt_on_zw_off, zt_off_zw_on, zt_off_zw_off };
enum class blend_state { none, alpha, add, multiply, alpha_to_coverage };
enum class rasterizer_state { solid, wireframe, cull_none, wireframe_cull_none, cull_front };

class rendering_state
{
	Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_states[7];
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth_stencil_states[4];
	Microsoft::WRL::ComPtr<ID3D11BlendState> blend_states[5];
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_states[5];

public:
	void bind_depth_stencil_state(ID3D11DeviceContext* immediate_context, depth_stencil_state depth_stencil_state, UINT stencil_ref = 0)
	{
		immediate_context->OMSetDepthStencilState(depth_stencil_states[static_cast<size_t>(depth_stencil_state)].Get(), stencil_ref);
	}
	void bind_sampler_states(ID3D11DeviceContext* immediate_context)
	{
		std::vector<ID3D11SamplerState*> samplers;
		for (const Microsoft::WRL::ComPtr<ID3D11SamplerState>& sampler : sampler_states)
		{
			samplers.emplace_back(sampler.Get());
		}
		immediate_context->PSSetSamplers(0, static_cast<UINT>(samplers.size()), samplers.data());
		immediate_context->GSSetSamplers(0, static_cast<UINT>(samplers.size()), samplers.data());
	}
	void bind_blend_state(ID3D11DeviceContext* immediate_context, blend_state blend_state)
	{
		immediate_context->OMSetBlendState(blend_states[static_cast<size_t>(blend_state)].Get(), NULL, 0xFFFFFFFF);
	}
	void bind_rasterizer_state(ID3D11DeviceContext* immediate_context, rasterizer_state rasterizer_state)
	{
		immediate_context->RSSetState(rasterizer_states[static_cast<size_t>(rasterizer_state)].Get());
	}
	rendering_state(ID3D11Device* device);
	virtual ~rendering_state() = default;

	ID3D11DepthStencilState* depth_stencil_state(depth_stencil_state depth_stencil_state)
	{
		return depth_stencil_states[static_cast<size_t>(depth_stencil_state)].Get();
	}
	ID3D11BlendState* blend_state(blend_state blend_state)
	{
		return blend_states[static_cast<size_t>(blend_state)].Get();
	}
	ID3D11RasterizerState* rasterizer_state(rasterizer_state rasterizer_state)
	{
		return rasterizer_states[static_cast<size_t>(rasterizer_state)].Get();
	}

};