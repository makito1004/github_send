
#include "sprite.h"
#include "misc.h"

#include <sstream>


#include <functional>


//#include <WICTextureLoader.h>

#include "texture.h"
#include "shader.h"

sprite::sprite(ID3D11Device* device, const wchar_t* filename)
{
	HRESULT hr{ S_OK };

	vertex vertices[]
	{
		{ { -1.0, +1.0, 0 }, { 1, 1, 1, 1 }, { 0, 0 } },
		{ { +1.0, +1.0, 0 }, { 1, 1, 1, 1 }, { 1, 0 } },
		{ { -1.0, -1.0, 0 }, { 1, 1, 1, 1 }, { 0, 1 } },
		{ { +1.0, -1.0, 0 }, { 1, 1, 1, 1 }, { 1, 1 } },
	};

	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = sizeof(vertices);
	buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
	buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	buffer_desc.MiscFlags = 0;
	buffer_desc.StructureByteStride = 0;
	D3D11_SUBRESOURCE_DATA subresource_data{};
	subresource_data.pSysMem = vertices;
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

	//シェーダーリソースビューを作成し、そのテクスチャの情報を取得
	shader_resource_view = texture::_emplace(device, filename);
	texture2d_desc = texture::_texture2d_desc(shader_resource_view.Get());
}
void sprite::blit(ID3D11DeviceContext* immediate_context,

	float dx, float dy, float dw, float dh,
	float r, float g, float b, float a,

	float angle/*degree*/)
{
	
	blit(immediate_context, dx, dy, dw, dh, r, g, b, a, angle, 0.0f, 0.0f, static_cast<float>(texture2d_desc.Width), static_cast<float>(texture2d_desc.Height));
}

void sprite::blit(ID3D11DeviceContext* immediate_context,
	float dx, float dy, float dw, float dh,
	float r, float g, float b, float a,
	float angle/*degree*/,
	float sx, float sy, float sw, float sh)
{

	D3D11_VIEWPORT viewport{};
	UINT num_viewports{ 1 };
	immediate_context->RSGetViewports(&num_viewports, &viewport);

	// 左上
	float x0{ dx };
	float y0{ dy };
	// 右上
	float x1{ dx + dw };
	float y1{ dy };
	// 左下
	float x2{ dx };
	float y2{ dy + dh };
	// 右下
	float x3{ dx + dw };
	float y3{ dy + dh };

#if 1

	std::function<void(float&, float&, float, float, float)> rotate = [](float& x, float& y, float cx, float cy, float angle)
	{
		x -= cx;
		y -= cy;

		float cos{ cosf(DirectX::XMConvertToRadians(angle)) };
		float sin{ sinf(DirectX::XMConvertToRadians(angle)) };
		float tx{ x }, ty{ y };
		x = cos * tx + -sin * ty;
		y = sin * tx + cos * ty;

		x += cx;
		y += cy;
	};
	float cx = dx + dw * 0.5f;
	float cy = dy + dh * 0.5f;
	rotate(x0, y0, cx, cy, angle);
	rotate(x1, y1, cx, cy, angle);
	rotate(x2, y2, cx, cy, angle);
	rotate(x3, y3, cx, cy, angle);
#else
	// Translate sprite's centre to origin (rotate centre)
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

	// Rotate each sprite's vertices by angle
	float tx, ty;
	float cos{ cosf(XMConvertToRadians(angle)) };
	float sin{ sinf(XMConvertToRadians(angle)) };
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

	// Translate sprite's centre to original position
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

	HRESULT hr{ S_OK };
	D3D11_MAPPED_SUBRESOURCE mapped_subresource{};
	hr = immediate_context->Map(vertex_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_subresource);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	vertex* vertices{ reinterpret_cast<vertex*>(mapped_subresource.pData) };
	if (vertices != nullptr)
	{
		vertices[0].position = { x0, y0 , 0 };
		vertices[1].position = { x1, y1 , 0 };
		vertices[2].position = { x2, y2 , 0 };
		vertices[3].position = { x3, y3 , 0 };

		vertices[0].color = vertices[1].color = vertices[2].color = vertices[3].color = { r, g, b, a };


		vertices[0].texcoord = { sx / texture2d_desc.Width, sy / texture2d_desc.Height };
		vertices[1].texcoord = { (sx + sw) / texture2d_desc.Width, sy / texture2d_desc.Height };
		vertices[2].texcoord = { sx / texture2d_desc.Width, (sy + sh) / texture2d_desc.Height };
		vertices[3].texcoord = { (sx + sw) / texture2d_desc.Width, (sy + sh) / texture2d_desc.Height };
	}

	immediate_context->Unmap(vertex_buffer.Get(), 0);

	UINT stride{ sizeof(vertex) };
	UINT offset{ 0 };
	immediate_context->IASetVertexBuffers(0, 1, vertex_buffer.GetAddressOf(), &stride, &offset);

	immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	immediate_context->IASetInputLayout(input_layout.Get());

	immediate_context->VSSetShader(vertex_shader.Get(), nullptr, 0);
	immediate_context->PSSetShader(pixel_shader.Get(), nullptr, 0);

	immediate_context->PSSetShaderResources(0, 1, shader_resource_view.GetAddressOf());

	immediate_context->Draw(4, 0);
}

void sprite::blit(ID3D11DeviceContext* immediate_context, float dx, float dy, float dw, float dh)
{
	blit(immediate_context, dx, dy, dw, dh, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f,
		0.0f, 0.0f, static_cast<float>(texture2d_desc.Width), static_cast<float>(texture2d_desc.Height));
}

sprite::~sprite()
{
}

void sprite::textout(ID3D11DeviceContext* immediate_context, std::string s, float x, float y, float w, float h, float r, float g, float b, float a)
{
	float sw = static_cast<float>(texture2d_desc.Width / 16);
	float sh = static_cast<float>(texture2d_desc.Height / 16);
	float carriage = 0;
	for (const char c : s)
	{
		blit(immediate_context, x + carriage, y, w, h, r, g, b, a, 0, sw * (c & 0x0F), sh * (c >> 4), sw, sh);
		carriage += w;
	}
}
