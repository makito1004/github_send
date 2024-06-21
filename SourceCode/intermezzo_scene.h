#pragma once

#include "scene.h"

#include <d3d11.h>
#include <wrl.h>

#include "fullscreen_quad.h"
#include "constant_buffer.h"
#include "sprite.h"
#include "shader.h"
#include "rendering_state.h"

class intermezzo_scene : public scene
{
	std::unique_ptr<sprite> hit_space_key;
	std::unique_ptr<sprite> buckground;

	Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel_shaders[8];
	std::unique_ptr<fullscreen_quad> bit_block_transfer;
	std::string preload_scene;

	std::unique_ptr<rendering_state> rendering_state;

	struct constants
	{
		float params[4];
	};
	std::unique_ptr<constant_buffer<constants>> cbuffer;

public:
	size_t type = 1;
	bool initialize(ID3D11Device* device, UINT64 width, UINT height, const std::unordered_map<std::string, std::string>& props) override
	{

		hit_space_key = std::make_unique<sprite>(device, L".\\resources\\Image\\230x0w.png");
		buckground = std::make_unique<sprite>(device, L".\\resources\\Image\\background.png");
		bit_block_transfer = std::make_unique<fullscreen_quad>(device);

		pixel_shaders[0] = shader<ID3D11PixelShader>::_emplace(device, "disco_tunnel_ps.cso");
		pixel_shaders[1] = shader<ID3D11PixelShader>::_emplace(device, "rounded_loading_spinner_ps.cso");

		rendering_state = std::make_unique<decltype(rendering_state)::element_type>(device);

		cbuffer = std::make_unique<constant_buffer<constants>>(device);

		type = std::stoi(props.at("type"));
		preload_scene = props.at("preload");
		_async_preload_scene(device, width, height, preload_scene);

		return true;
	}
	void update(ID3D11DeviceContext* immediate_context, float delta_time) override
	{
		if (GetAsyncKeyState(' ') & 1)
		{
			if (_has_finished_preloading(preload_scene))
			{
				_transition(preload_scene, {});
			}
		}
	}

	float time = 0;
	void render(ID3D11DeviceContext* immediate_context, float delta_time) override
	{
		D3D11_VIEWPORT viewport;
		UINT num_viewports{ 1 };
		immediate_context->RSGetViewports(&num_viewports, &viewport);

		rendering_state->bind_sampler_states(immediate_context);
		rendering_state->bind_blend_state(immediate_context, blend_state::none);
		rendering_state->bind_depth_stencil_state(immediate_context, depth_stencil_state::zt_off_zw_off);
		rendering_state->bind_rasterizer_state(immediate_context, rasterizer_state::cull_none);

		time += delta_time;
		cbuffer->data.params[0] = time;
		cbuffer->data.params[1] = viewport.Width;
		cbuffer->data.params[2] = viewport.Height;
		cbuffer->activate(immediate_context, 8, cb_usage::vp);

		buckground->blit(immediate_context, 0,0,1920 , 1080);

		if (_has_finished_preloading(preload_scene))
		{
			rendering_state->bind_blend_state(immediate_context, blend_state::add);
			hit_space_key->blit(immediate_context, (viewport.Width - 64) / 2, (viewport.Height - 64) / 2, 64, 64);
		}
		return;
	}
};