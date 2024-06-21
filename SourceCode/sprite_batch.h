#pragma once

#include <d3d11.h>

#include <wrl.h>
#include <directxmath.h>


#include <vector>


class sprite_batch
{
private:
	Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel_shader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> input_layout;
	Microsoft::WRL::ComPtr<ID3D11Buffer> vertex_buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view;
	D3D11_TEXTURE2D_DESC texture2d_desc;

public:
	struct vertex
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT4 color;
		DirectX::XMFLOAT2 texcoord;
	};
private:
	
	const size_t max_vertices;
	std::vector<vertex> vertices;

public:
	sprite_batch(ID3D11Device *device, const wchar_t* filename, size_t max_sprites);
	virtual ~sprite_batch();

	void render(ID3D11DeviceContext* immediate_context, float dx, float dy, float dw, float dh, float r, float g, float b, float a, float angle/*degree*/);
	void render(ID3D11DeviceContext* immediate_context, float dx, float dy, float dw, float dh, float r, float g, float b, float a, float angle/*degree*/, float sx, float sy, float sw, float sh);
	void render(ID3D11DeviceContext* immediate_context, float dx, float dy, float dw, float dh);


	void begin(ID3D11DeviceContext* immediate_context, 
		ID3D11PixelShader* replaced_pixel_shader = nullptr/*UNIT.10*/, ID3D11ShaderResourceView* replaced_shader_resource_view = nullptr/*UNIT.10*/);
	void end(ID3D11DeviceContext* immediate_context);

private:
	static void rotate(float& x, float& y, float cx, float cy, float sin, float cos)
	{
		x -= cx;
		y -= cy;

		float tx{ x }, ty{ y };
		x = cos * tx + -sin * ty;
		y = sin * tx + cos * ty;

		x += cx;
		y += cy;
	}
};




