#include "cascaded_shadow_map.h"

#include <array>

#include "misc.h"

using namespace DirectX;

std::array<XMFLOAT4, 8> _make_frustum_corners_world_space(const XMFLOAT4X4& view, const XMFLOAT4X4& projection)
{
	const XMMATRIX VP_ = XMMatrixInverse(NULL, XMLoadFloat4x4(&view) * XMLoadFloat4x4(&projection));

	std::array<XMFLOAT4, 8> frustum_corners;
	size_t index = 0;
	for (size_t x = 0; x < 2; ++x)
	{
		for (size_t y = 0; y < 2; ++y)
		{
			for (size_t z = 0; z < 2; ++z)
			{
				XMFLOAT4 pt = { 2.0f * x - 1.0f, 2.0f * y - 1.0f, 2.0f * z - 1.0f, 1.0f };
				XMStoreFloat4(&pt, XMVector3TransformCoord(XMLoadFloat4(&pt), VP_));
				frustum_corners.at(index++) = pt;
			}
		}
	}
	return frustum_corners;
}

cascaded_shadow_map::cascaded_shadow_map(ID3D11Device* device, uint32_t width, uint32_t height) : _cascade_count(4)
{
	HRESULT hr = S_OK;

	D3D11_TEXTURE2D_DESC texture2d_desc = {};
	texture2d_desc.Width = width;
	texture2d_desc.Height = height;
	texture2d_desc.MipLevels = 1;
	texture2d_desc.ArraySize = static_cast<UINT>(_cascade_count);
	texture2d_desc.Format = DXGI_FORMAT_R32_TYPELESS;
	texture2d_desc.SampleDesc.Count = 1;
	texture2d_desc.SampleDesc.Quality = 0;
	texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
	texture2d_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	texture2d_desc.CPUAccessFlags = 0;
	texture2d_desc.MiscFlags = 0;
	hr = device->CreateTexture2D(&texture2d_desc, 0, depth_stencil_buffer.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc = {};
	depth_stencil_view_desc.Format = DXGI_FORMAT_D32_FLOAT;
	depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
	depth_stencil_view_desc.Texture2DArray.FirstArraySlice = 0;
	depth_stencil_view_desc.Texture2DArray.ArraySize = static_cast<UINT>(_cascade_count);
	depth_stencil_view_desc.Texture2DArray.MipSlice = 0;
	depth_stencil_view_desc.Flags = 0;
	hr = device->CreateDepthStencilView(depth_stencil_buffer.Get(), &depth_stencil_view_desc, _depth_stencil_view.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc = {};
	shader_resource_view_desc.Format = DXGI_FORMAT_R32_FLOAT; // DXGI_FORMAT_R24_UNORM_X8_TYPELESS : DXGI_FORMAT_R32_FLOAT : DXGI_FORMAT_R16_UNORM
	shader_resource_view_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	shader_resource_view_desc.Texture2DArray.ArraySize = static_cast<UINT>(_cascade_count);
	shader_resource_view_desc.Texture2DArray.MipLevels = 1;
	shader_resource_view_desc.Texture2DArray.FirstArraySlice = 0;
	shader_resource_view_desc.Texture2DArray.MostDetailedMip = 0;
	hr = device->CreateShaderResourceView(depth_stencil_buffer.Get(), &shader_resource_view_desc, _shader_resource_view.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	_viewport.Width = static_cast<float>(width);
	_viewport.Height = static_cast<float>(height);
	_viewport.MinDepth = 0.0f;
	_viewport.MaxDepth = 1.0f;
	_viewport.TopLeftX = 0.0f;
	_viewport.TopLeftY = 0.0f;

	_constants = std::make_unique<constant_buffer<constants>>(device);

}

void cascaded_shadow_map::make(ID3D11DeviceContext* immediate_context,
	const DirectX::XMFLOAT4X4& camera_view,
	const DirectX::XMFLOAT4X4& camera_projection,
	const DirectX::XMFLOAT4& light_direction,
	float critical_depth_value,
	std::function<void()> drawcallback)
{
	D3D11_VIEWPORT cached_viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
	UINT viewport_count = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
	immediate_context->RSGetViewports(&viewport_count, cached_viewports);
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> cached_render_target_view;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> cached_depth_stencil_view;
	immediate_context->OMGetRenderTargets(1, cached_render_target_view.ReleaseAndGetAddressOf(), cached_depth_stencil_view.ReleaseAndGetAddressOf());

	// 透視投影行列からの近・遠値
	float m33 = camera_projection._33;
	float m43 = camera_projection._43;
	float zn = -m43 / m33;
	float zf = (m33 * zn) / (m33 - 1);
	zf = critical_depth_value > 0 ? std::min(zf, critical_depth_value) : zf;

	// view空間における分割面の距離を計算する
	_distances.resize(_cascade_count + 1);
	for (size_t cascade_index = 0; cascade_index < _cascade_count; ++cascade_index)
	{
		float idc = cascade_index / static_cast<float>(_cascade_count);
		float logarithmic_split_scheme = zn * pow(zf / zn, idc);
		float uniform_split_scheme = zn + (zf - zn) * idc;
		_distances.at(cascade_index) = logarithmic_split_scheme * _split_scheme_weight + uniform_split_scheme * (1 - _split_scheme_weight);
	}
	// ボーダーの値が正確であることを確認する
	_distances.at(0) = zn;
	_distances.at(_cascade_count) = zf;

	const bool fit_to_cascade = true; 
	_view_projection.resize(_cascade_count);
	for (size_t cascade_index = 0; cascade_index < _cascade_count; ++cascade_index)
	{
		float _zn = fit_to_cascade ? _distances.at(cascade_index) : zn;
		float _zf = _distances.at(cascade_index + 1);

		DirectX::XMFLOAT4X4 cascaded_projection = camera_projection;
		cascaded_projection._33 = _zf / (_zf - _zn);
		cascaded_projection._43 = -_zn * _zf / (_zf - _zn);

		std::array<XMFLOAT4, 8> corners = _make_frustum_corners_world_space(camera_view, cascaded_projection);

		DirectX::XMFLOAT4 center = { 0, 0, 0, 1 };
		for (decltype(corners)::const_reference v : corners)
		{
			center.x += v.x;
			center.y += v.y;
			center.z += v.z;
		}
		center.x /= corners.size();
		center.y /= corners.size();
		center.z /= corners.size();

		XMMATRIX V;
		V = XMMatrixLookAtLH(
			XMVectorSet(center.x - light_direction.x, center.y - light_direction.y, center.z - light_direction.z, 1.0f),
			XMVectorSet(center.x, center.y, center.z, 1.0f),
			XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

		float min_x = std::numeric_limits<float>::max();
		float max_x = std::numeric_limits<float>::lowest();
		float min_y = std::numeric_limits<float>::max();
		float max_y = std::numeric_limits<float>::lowest();
		float min_z = std::numeric_limits<float>::max();
		float max_z = std::numeric_limits<float>::lowest();
		for (decltype(corners)::const_reference v : corners)
		{
			XMFLOAT4 _v;
			XMStoreFloat4(&_v, XMVector3TransformCoord(XMLoadFloat4(&v), V));
			min_x = std::min(min_x, _v.x);
			max_x = std::max(max_x, _v.x);
			min_y = std::min(min_y, _v.y);
			max_y = std::max(max_y, _v.y);
			min_z = std::min(min_z, _v.z);
			max_z = std::max(max_z, _v.z);
		}

#if 1
		// 実際の投影行列を作成する前に、光のフラストラムの近・遠平面がカバーする空間のサイズを大きくする。
		constexpr float z_mult = 50.0f;
		if (min_z < 0)
		{
			min_z *= z_mult;
		}
		else
		{
			min_z /= z_mult;
		}
		if (max_z < 0)
		{
			max_z /= z_mult;
		}
		else
		{
			max_z *= z_mult;
		}
#endif

		XMMATRIX P = XMMatrixOrthographicOffCenterLH(min_x, max_x, min_y, max_y, min_z, max_z);
		XMStoreFloat4x4(&_view_projection.at(cascade_index), V * P);
	}

	_constants->data.view_projection_matrices[0] = _view_projection.at(0);
	_constants->data.view_projection_matrices[1] = _view_projection.at(1);
	_constants->data.view_projection_matrices[2] = _view_projection.at(2);
	_constants->data.view_projection_matrices[3] = _view_projection.at(3);

	_constants->data.cascade_plane_distances[0] = _distances.at(1);
	_constants->data.cascade_plane_distances[1] = _distances.at(2);
	_constants->data.cascade_plane_distances[2] = _distances.at(3);
	_constants->data.cascade_plane_distances[3] = _distances.at(4);

	_constants->activate(immediate_context, 13, cb_usage::vp);

	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> null_render_target_view;
	immediate_context->ClearDepthStencilView(_depth_stencil_view.Get(), D3D11_CLEAR_DEPTH, 1, 0);
	immediate_context->OMSetRenderTargets(1, null_render_target_view.GetAddressOf(), _depth_stencil_view.Get());
	immediate_context->RSSetViewports(1, &_viewport);

	drawcallback();

	immediate_context->RSSetViewports(viewport_count, cached_viewports);
	immediate_context->OMSetRenderTargets(1, cached_render_target_view.GetAddressOf(), cached_depth_stencil_view.Get());
}
