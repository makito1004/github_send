#pragma once

// UNIT.32
#include <d3d11.h>
#include <wrl.h>
#include <cstdint>

#include <directxmath.h>

class framebuffer
{
public:
	enum class usage
	{
		color = 0x01,
		depth = 0x02,
		stencil = 0x04,
		color_depth_stencil = color | depth | stencil,
		color_depth = color | depth,
		depth_stencil = depth | stencil,
	};

	framebuffer(ID3D11Device* device, uint32_t width, uint32_t height, usage flags = usage::color_depth_stencil/*UNIT.99*/, bool enable_msaa = false/*UNIT.99*/, int subsamples = 1/*UNIT.99*/, bool generate_mips = false/*UNIT.99*/);
	virtual ~framebuffer() = default;

	void clear(ID3D11DeviceContext* immediate_context, usage flags = usage::color_depth_stencil/*UNIT.99*/, DirectX::XMFLOAT4 color = { 0, 0, 0, 1 }, float depth = 1, uint8_t stencil = 0/*UNIT.99*/);

	void activate(ID3D11DeviceContext* immediate_context, usage flags = usage::color_depth_stencil/*UNIT.99*/);
	void deactivate(ID3D11DeviceContext* immediate_context);


	const Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& color_map() { return shader_resource_views[0]; }
	const Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& depth_map() { return shader_resource_views[1]; }

	//SRVに対して、ミップマップを生成する処理
	void generate_mips(ID3D11DeviceContext* immediate_context);
	// 描画処理を行うための準備をする関数
	void resolve(ID3D11DeviceContext* immediate_context, framebuffer* destination);

protected:
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target_view;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depth_stencil_view;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_views[2];
	D3D11_VIEWPORT viewport;

private:
	UINT viewport_count{ D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE };
	D3D11_VIEWPORT cached_viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> cached_render_target_view;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> cached_depth_stencil_view;

	
	Microsoft::WRL::ComPtr<ID3D11VertexShader> embedded_vertex_shader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> embedded_pixel_shader;
};
