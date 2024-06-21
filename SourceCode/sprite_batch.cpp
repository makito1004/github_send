
#include "sprite_batch.h" // 
#include "misc.h"

#include <sstream>


#include <functional>


//#include <WICTextureLoader.h>


#include "texture.h"
#include "shader.h"

sprite_batch::sprite_batch(ID3D11Device* device, const wchar_t* filename, size_t max_sprites) : max_vertices(max_sprites * 6)
{
	HRESULT hr{ S_OK };

	std::unique_ptr<vertex[]> vertices{ std::make_unique<vertex[]>(max_vertices) };

	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = static_cast<UINT>(sizeof(vertex) * max_vertices);
	buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
	buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	buffer_desc.MiscFlags = 0;
	buffer_desc.StructureByteStride = 0;
	D3D11_SUBRESOURCE_DATA subresource_data{};
	subresource_data.pSysMem = vertices.get();
	subresource_data.SysMemPitch = 0;
	subresource_data.SysMemSlicePitch = 0;
	hr = device->CreateBuffer(&buffer_desc, &subresource_data, vertex_buffer.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	D3D11_INPUT_ELEMENT_DESC input_element_desc[]
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	
	vertex_shader = shader<ID3D11VertexShader>::_emplace(device, "sprite_vs.cso", input_layout.GetAddressOf(), input_element_desc, _countof(input_element_desc));
	pixel_shader = shader<ID3D11PixelShader>::_emplace(device, "sprite_ps.cso");

	
	shader_resource_view = texture::_emplace(device, filename);
	texture2d_desc = texture::_texture2d_desc(shader_resource_view.Get());
}
void sprite_batch::render(ID3D11DeviceContext* immediate_context,
	
	float dx, float dy, float dw, float dh,
	float r, float g, float b, float a,

	float angle/*degree*/)
{
	
	render(immediate_context, dx, dy, dw, dh, r, g, b, a, angle, 0.0f, 0.0f, static_cast<float>(texture2d_desc.Width), static_cast<float>(texture2d_desc.Height));
}
void sprite_batch::render(ID3D11DeviceContext* immediate_context,
	
	float dx, float dy, float dw, float dh,
	float r, float g, float b, float a,
	
	float angle/*degree*/,
	
	float sx, float sy, float sw, float sh)
{
	D3D11_VIEWPORT viewport{};
	UINT num_viewports{ 1 };
	immediate_context->RSGetViewports(&num_viewports, &viewport);

	//左上
	float x0{ dx };
	float y0{ dy };
	// 右上
	float x1{ dx + dw };
	float y1{ dy };
	// 左下
	float x2{ dx };
	float y2{ dy + dh };
	// 右上
	float x3{ dx + dw };
	float y3{ dy + dh };

#if 0
#if 0
	std::function<void(float&, float&, float, float, float, float)> rotate{ [](float& x, float& y, float cx, float cy, float sin, float cos)
	{
		x -= cx;
		y -= cy;

		float tx{ x }, ty{ y };
		x = cos * tx + -sin * ty;
		y = sin * tx + cos * ty;

		x += cx;
		y += cy;
	} };
#endif
	// UNIT.09
	float cos{ cosf(DirectX::XMConvertToRadians(angle)) };
	float sin{ sinf(DirectX::XMConvertToRadians(angle)) };

	float cx = dx + dw * 0.5f;
	float cy = dy + dh * 0.5f;
	rotate(x0, y0, cx, cy, sin, cos);
	rotate(x1, y1, cx, cy, sin, cos);
	rotate(x2, y2, cx, cy, sin, cos);
	rotate(x3, y3, cx, cy, sin, cos);
#else
	
	//スプライトの中心を原点に移動する
	float cx = dx + dw * 0.5f;
	float cy = dy + dh * 0.5f;
	x0 -= cx;
	y0 -= cy;
	x1 -= cx;
	y1 -= cy;
	x2 -= cx;
	y2 -= cy;
	x3 -= cx;
	y3 -= cy;

	// 各スプライトの頂点を角度ごとに回転する
	float tx, ty;
	float cos{ cosf(DirectX::XMConvertToRadians(angle)) };
	float sin{ sinf(DirectX::XMConvertToRadians(angle)) };
	tx = x0;
	ty = y0;
	x0 = cos * tx + -sin * ty;
	y0 = sin * tx + cos * ty;
	tx = x1;
	ty = y1;
	x1 = cos * tx + -sin * ty;
	y1 = sin * tx + cos * ty;
	tx = x2;
	ty = y2;
	x2 = cos * tx + -sin * ty;
	y2 = sin * tx + cos * ty;
	tx = x3;
	ty = y3;
	x3 = cos * tx + -sin * ty;
	y3 = sin * tx + cos * ty;

	// スプライトの中心を元の位置に移動
	x0 += cx;
	y0 += cy;
	x1 += cx;
	y1 += cy;
	x2 += cx;
	y2 += cy;
	x3 += cx;
	y3 += cy;
#endif

	x0 = 2.0f * x0 / viewport.Width - 1.0f;
	y0 = 1.0f - 2.0f * y0 / viewport.Height;
	x1 = 2.0f * x1 / viewport.Width - 1.0f;
	y1 = 1.0f - 2.0f * y1 / viewport.Height;
	x2 = 2.0f * x2 / viewport.Width - 1.0f;
	y2 = 1.0f - 2.0f * y2 / viewport.Height;
	x3 = 2.0f * x3 / viewport.Width - 1.0f;
	y3 = 1.0f - 2.0f * y3 / viewport.Height;


	float u0{ sx / texture2d_desc.Width };
	float v0{ sy / texture2d_desc.Height };
	float u1{ (sx + sw) / texture2d_desc.Width };
	float v1{ (sy + sh) / texture2d_desc.Height };
	vertices.push_back({ { x0, y0 , 0 }, { r, g, b, a }, { u0, v0 } });
	vertices.push_back({ { x1, y1 , 0 }, { r, g, b, a }, { u1, v0 } });
	vertices.push_back({ { x2, y2 , 0 }, { r, g, b, a }, { u0, v1 } });
	vertices.push_back({ { x2, y2 , 0 }, { r, g, b, a }, { u0, v1 } });
	vertices.push_back({ { x1, y1 , 0 }, { r, g, b, a }, { u1, v0 } });
	vertices.push_back({ { x3, y3 , 0 }, { r, g, b, a }, { u1, v1 } });
}

void sprite_batch::render(ID3D11DeviceContext* immediate_context, float dx, float dy, float dw, float dh)
{
	render(immediate_context, dx, dy, dw, dh, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f,
		0.0f, 0.0f, static_cast<float>(texture2d_desc.Width), static_cast<float>(texture2d_desc.Height));
}

sprite_batch::~sprite_batch()
{
}

void sprite_batch::begin(ID3D11DeviceContext* immediate_context,
	ID3D11PixelShader* replaced_pixel_shader/*UNIT.10*/, ID3D11ShaderResourceView* replaced_shader_resource_view/*UNIT.10*/)
{
	vertices.clear();
	immediate_context->VSSetShader(vertex_shader.Get(), nullptr, 0);
	replaced_pixel_shader ? immediate_context->PSSetShader(replaced_pixel_shader, nullptr, 0) : immediate_context->PSSetShader(pixel_shader.Get(), nullptr, 0);


	if (replaced_shader_resource_view)
	{
		HRESULT hr{ S_OK };
		Microsoft::WRL::ComPtr<ID3D11Resource> resource;
		replaced_shader_resource_view->GetResource(resource.GetAddressOf());

		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2d;
		hr = resource->QueryInterface<ID3D11Texture2D>(texture2d.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		texture2d->GetDesc(&texture2d_desc);

		immediate_context->PSSetShaderResources(0, 1, &replaced_shader_resource_view);
	}
	else
	{
		immediate_context->PSSetShaderResources(0, 1, shader_resource_view.GetAddressOf());
	}
}

void sprite_batch::end(ID3D11DeviceContext* immediate_context)
{
	HRESULT hr{ S_OK };
	D3D11_MAPPED_SUBRESOURCE mapped_subresource{};
	hr = immediate_context->Map(vertex_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_subresource);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	size_t vertex_count = vertices.size();
	_ASSERT_EXPR(max_vertices >= vertex_count, "Buffer overflow");
	vertex* data{ reinterpret_cast<vertex*>(mapped_subresource.pData) };
	if (data != nullptr)
	{
		const vertex* p = vertices.data();
		memcpy_s(data, max_vertices * sizeof(vertex), p, vertex_count * sizeof(vertex));
	}
	immediate_context->Unmap(vertex_buffer.Get(), 0);

	UINT stride{ sizeof(vertex) };
	UINT offset{ 0 };
	immediate_context->IASetVertexBuffers(0, 1, vertex_buffer.GetAddressOf(), &stride, &offset);
	immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	immediate_context->IASetInputLayout(input_layout.Get());

	immediate_context->Draw(static_cast<UINT>(vertex_count), 0);

	
	Microsoft::WRL::ComPtr<ID3D11Resource> resource;
	shader_resource_view.Get()->GetResource(resource.GetAddressOf());

	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2d;
	hr = resource->QueryInterface<ID3D11Texture2D>(texture2d.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	texture2d->GetDesc(&texture2d_desc);
}
