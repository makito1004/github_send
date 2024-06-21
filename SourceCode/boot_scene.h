#pragma once

#include "scene.h"

#include <d3d11.h>
#include <wrl.h>

#include "sprite.h"
#include "rendering_state.h"

class boot_scene : public scene
{
	std::unique_ptr<sprite> splash;
	std::unique_ptr<rendering_state> rendering_state;

	Microsoft::WRL::ComPtr<IDWriteTextFormat> dwrite_text_format;
	Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> d2d_solid_color_brush;

public:
	bool initialize(ID3D11Device* device, UINT64 width, UINT height, const std::unordered_map<std::string, std::string>& props) override
	{
		splash = std::make_unique<sprite>(device, L".\\resources\\logo_s_w.png");
		rendering_state = std::make_unique<decltype(rendering_state)::element_type>(device);
		return true;
	}
	void update(ID3D11DeviceContext* immediate_context, float delta_time) override
	{
#if 0
		if (GetAsyncKeyState(' ') & 1)
#endif
		{
			const char* types[] = { "0", "1" };
			scene::_transition("intermezzo_scene", { std::make_pair("preload", "main_scene"), std::make_pair("type", types[1]) });//‚±‚±‚Åƒ[ƒh‰æ–Ê‚ÌŽí—ÞŒˆ‚ß
		}
	}
	void render(ID3D11DeviceContext* immediate_context, float delta_time) override
	{
#if 0
		D3D11_VIEWPORT viewport;
		UINT num_viewports{ 1 };
		immediate_context->RSGetViewports(&num_viewports, &viewport);

		rendering_state->bind_sampler_states(immediate_context);
		rendering_state->bind_blend_state(immediate_context, blend_state::none);
		rendering_state->bind_depth_stencil_state(immediate_context, depth_stencil_state::zt_off_zw_off);
		rendering_state->bind_rasterizer_state(immediate_context, rasterizer_state::cull_none);

		float w = 165, h = w * 0.2035657865041751f;
		splash->blit(immediate_context, 1680 - w - 20, 715 - h - 20, w, h, 1, 1, 1, 0.75f, 0);
#endif
	}
};