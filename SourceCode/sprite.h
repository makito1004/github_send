#pragma once

#include <d3d11.h>
#include <directxmath.h>


#include <wrl.h>
#include <string>

class sprite
{
private:
	
	Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel_shader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> input_layout;
	Microsoft::WRL::ComPtr<ID3D11Buffer> vertex_buffer;

	
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view;

public:
	D3D11_TEXTURE2D_DESC texture2d_desc;
	struct vertex
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT4 color;
		DirectX::XMFLOAT2 texcoord;
	};

	sprite(ID3D11Device *device, const wchar_t* filename);
	virtual ~sprite();

	void blit(ID3D11DeviceContext* immediate_context, float dx, float dy, float dw, float dh, float r, float g, float b, float a, float angle/*degree*/);
	void blit(ID3D11DeviceContext* immediate_context, float dx, float dy, float dw, float dh, float r, float g, float b, float a, float angle/*degree*/, float sx, float sy, float sw, float sh);
	void blit(ID3D11DeviceContext* immediate_context, float dx, float dy, float dw, float dh);
	void textout(ID3D11DeviceContext* immediate_context, std::string s, float x, float y, float w, float h, float r, float g, float b, float a);
};
