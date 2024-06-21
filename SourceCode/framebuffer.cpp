
#include "framebuffer.h"
#include "misc.h"
#include "shader.h" 

inline bool operator&(framebuffer::usage lhs, framebuffer::usage rhs)
{
	return static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs);
}
framebuffer::framebuffer(ID3D11Device* device, uint32_t width, uint32_t height, usage flags, bool enable_msaa/*UNIT.99*/, int subsamples/*UNIT.99*/, bool generate_mips/*UNIT.99*/)
{
	HRESULT hr{ S_OK };

	DXGI_FORMAT rt_format = DXGI_FORMAT_R16G16B16A16_FLOAT; // DXGI_FORMAT_R16G16B16A16_FLOAT DXGI_FORMAT_R32G32B32A32_FLOAT
	UINT msaa_quality_level;
	UINT sample_count = subsamples;
	hr = device->CheckMultisampleQualityLevels(rt_format, sample_count, &msaa_quality_level);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	//RTVとSRVを作成する処理
	if (flags & usage::color)
	{
		Microsoft::WRL::ComPtr<ID3D11Texture2D> render_target_buffer;
		D3D11_TEXTURE2D_DESC texture2d_desc{};
		texture2d_desc.Width = width;
		texture2d_desc.Height = height;
		texture2d_desc.MipLevels = generate_mips ? 0 : 1; // UNIT.99
		texture2d_desc.ArraySize = 1;
		texture2d_desc.Format = rt_format;
		texture2d_desc.SampleDesc.Count = enable_msaa ? sample_count : 1; // UNIT.99
		texture2d_desc.SampleDesc.Quality = enable_msaa ? msaa_quality_level - 1 : 0; // UNIT.99
		texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
		texture2d_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		texture2d_desc.CPUAccessFlags = 0;
		texture2d_desc.MiscFlags = generate_mips ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0; // UNIT.99
		hr = device->CreateTexture2D(&texture2d_desc, 0, render_target_buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		D3D11_RENDER_TARGET_VIEW_DESC render_target_view_desc{};
		render_target_view_desc.Format = texture2d_desc.Format;
		render_target_view_desc.ViewDimension = enable_msaa ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D; // UNIT.99
		hr = device->CreateRenderTargetView(render_target_buffer.Get(), &render_target_view_desc, render_target_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc{};
		shader_resource_view_desc.Format = texture2d_desc.Format;
		shader_resource_view_desc.ViewDimension = enable_msaa ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D; // UNIT.99
		shader_resource_view_desc.Texture2D.MostDetailedMip = 0;
		shader_resource_view_desc.Texture2D.MipLevels = generate_mips ? -1 : 1; // UNIT.99
		hr = device->CreateShaderResourceView(render_target_buffer.Get(), &shader_resource_view_desc, shader_resource_views[0].GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}
	if (flags & usage::depth_stencil)
	{
		Microsoft::WRL::ComPtr<ID3D11Texture2D> depth_stencil_buffer;
		D3D11_TEXTURE2D_DESC texture2d_desc{};
		texture2d_desc.Width = width;
		texture2d_desc.Height = height;
		texture2d_desc.MipLevels = 1; // UNIT.99:Depth buffers can't have mipmaps.
		texture2d_desc.ArraySize = 1;
		texture2d_desc.Format = flags & usage::stencil ? DXGI_FORMAT_R24G8_TYPELESS : DXGI_FORMAT_R32_TYPELESS; // DXGI_FORMAT_R24G8_TYPELESS : DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_R16_TYPELESS
		texture2d_desc.SampleDesc.Count = enable_msaa ? sample_count : 1; // UNIT.99
		texture2d_desc.SampleDesc.Quality = enable_msaa ? msaa_quality_level - 1 : 0; // UNIT.99
		texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
		texture2d_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		texture2d_desc.CPUAccessFlags = 0;
		texture2d_desc.MiscFlags = 0; // UNIT.99:Depth buffers can't have mipmaps.
		hr = device->CreateTexture2D(&texture2d_desc, 0, depth_stencil_buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
		depth_stencil_view_desc.Format = flags & usage::stencil ? DXGI_FORMAT_D24_UNORM_S8_UINT : DXGI_FORMAT_D32_FLOAT; // DXGI_FORMAT_D24_UNORM_S8_UINT : DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_D16_UNORM
		depth_stencil_view_desc.ViewDimension = enable_msaa ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D; // UNIT.99
		depth_stencil_view_desc.Flags = 0;
		hr = device->CreateDepthStencilView(depth_stencil_buffer.Get(), &depth_stencil_view_desc, depth_stencil_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc{};
		shader_resource_view_desc.Format = flags & usage::stencil ? DXGI_FORMAT_R24_UNORM_X8_TYPELESS : DXGI_FORMAT_R32_FLOAT; // DXGI_FORMAT_R24_UNORM_X8_TYPELESS : DXGI_FORMAT_R32_FLOAT : DXGI_FORMAT_R16_UNORM
		shader_resource_view_desc.ViewDimension = enable_msaa ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D; // UNIT.99
		shader_resource_view_desc.Texture2D.MostDetailedMip = 0;
		shader_resource_view_desc.Texture2D.MipLevels = 1; // UNIT.99:Depth buffers can't have mipmaps.
		hr = device->CreateShaderResourceView(depth_stencil_buffer.Get(), &shader_resource_view_desc, shader_resource_views[1].GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}
	viewport.Width = static_cast<float>(width);
	viewport.Height = static_cast<float>(height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;

	//頂点シェーダとピクセルシェーダを作成する処理
	//シェーダファイルからシェーダを効率的に読み込み、グラフィックスパイプラインで使用する準備、
	embedded_vertex_shader = shader<ID3D11VertexShader>::_emplace(device, "fullscreen_quad_vs.cso", NULL, NULL, 0);
	embedded_pixel_shader = shader<ID3D11PixelShader>::_emplace(device, "resolve_depth_stencil_ps.cso");
}
void framebuffer::clear(ID3D11DeviceContext* immediate_context, usage flags, DirectX::XMFLOAT4 color, float depth, uint8_t stencil/*UNIT.99*/)
{
	if (flags & usage::color && render_target_view) // UNIT.99
	{
		immediate_context->ClearRenderTargetView(render_target_view.Get(), reinterpret_cast<const FLOAT*>(&color));
	}
	if (flags & usage::depth_stencil && depth_stencil_view) // UNIT.99
	{
		immediate_context->ClearDepthStencilView(depth_stencil_view.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, depth, stencil);
	}
}
void framebuffer::activate(ID3D11DeviceContext* immediate_context, usage flags/*UNIT.99*/)
{
	viewport_count = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
	immediate_context->RSGetViewports(&viewport_count, cached_viewports);
	immediate_context->OMGetRenderTargets(1, cached_render_target_view.ReleaseAndGetAddressOf(), cached_depth_stencil_view.ReleaseAndGetAddressOf());

	// レンダーターゲットと深度ステンシルビューを設定する処理
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> null_render_target_view;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> null_depth_stencil_view;
	immediate_context->RSSetViewports(1, &viewport);
	immediate_context->OMSetRenderTargets(1, flags & usage::color ? render_target_view.GetAddressOf() : null_render_target_view.GetAddressOf(), flags & usage::depth_stencil ? depth_stencil_view.Get() : null_depth_stencil_view.Get());
}
void framebuffer::deactivate(ID3D11DeviceContext* immediate_context)
{
	immediate_context->RSSetViewports(viewport_count, cached_viewports);
	immediate_context->OMSetRenderTargets(1, cached_render_target_view.GetAddressOf(), cached_depth_stencil_view.Get());
}
//　SRVに対して、ミップマップを生成する処理
void framebuffer::generate_mips(ID3D11DeviceContext* immediate_context)
{
	immediate_context->GenerateMips(shader_resource_views[0].Get()); 
}

//マルチサンプル（Multisampling）は、コンピュータグラフィックスにおいて、
//アンチエイリアシング（ジャギーの除去）や画質の向上のために使用される技術

// マルチサンプルされたレンダーターゲットから通常のテクスチャにデータをコピーし、その後の描画処理を行うための関数
void framebuffer::resolve(ID3D11DeviceContext* immediate_context, framebuffer* destination)
{
	// レンダーターゲットのサブリソースを解決する
	Microsoft::WRL::ComPtr<ID3D11Resource> resources[2];
	render_target_view->GetResource(resources[0].GetAddressOf());
	destination->render_target_view->GetResource(resources[1].GetAddressOf());

	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2d;
	HRESULT hr = resources[0].Get()->QueryInterface<ID3D11Texture2D>(texture2d.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	D3D11_TEXTURE2D_DESC texture2d_desc;
	texture2d->GetDesc(&texture2d_desc);
	_ASSERT_EXPR(texture2d_desc.SampleDesc.Count > 1, L"source texture must be multisample texture");

	immediate_context->ResolveSubresource(resources[1].Get(), D3D11CalcSubresource(0, 0, 1), resources[0].Get(), D3D11CalcSubresource(0, 0, 1), texture2d_desc.Format);

	// 深度ステンシルのサブリソースを解決する。
	immediate_context->ClearDepthStencilView(destination->depth_stencil_view.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	destination->activate(immediate_context);

	immediate_context->IASetVertexBuffers(0, 0, 0, 0, 0);
	immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	immediate_context->IASetInputLayout(0);

	immediate_context->VSSetShader(embedded_vertex_shader.Get(), 0, 0);
	immediate_context->PSSetShader(embedded_pixel_shader.Get(), 0, 0);

	immediate_context->PSSetShaderResources(0, 1, shader_resource_views[1].GetAddressOf());
	immediate_context->Draw(4, 0);

	destination->deactivate(immediate_context);
}
