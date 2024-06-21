#pragma once

#include <d3d11.h>
#include <wrl.h>

#include <vector>
#include <directxmath.h>


class geometric_primitive
{
public:
	enum class shape { cube, sphere, cylinder, plane, dodecahedron };
	std::vector<DirectX::XMFLOAT4> vertices;

	Microsoft::WRL::ComPtr<ID3D11Buffer> vertex_buffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> index_buffer;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel_shader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> input_layout;
	Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer;

	struct constants
	{
		DirectX::XMFLOAT4X4 world;
		DirectX::XMFLOAT4 color;
	};
	
	UINT index_count= 0;

	geometric_primitive(ID3D11Device* device, shape shape, int slices, int stacks);

	void draw(ID3D11DeviceContext* immediate_context, const DirectX::XMFLOAT4& position, const DirectX::XMFLOAT4& scale, const DirectX::XMFLOAT4& rotation, const DirectX::XMFLOAT4& color);
	void draw(ID3D11DeviceContext* immediate_context, const DirectX::XMFLOAT4& position, float scale, const DirectX::XMFLOAT4& color)
	{
		draw(immediate_context, position, { scale, scale, scale, 0 }, { 0, 0, 0, 0 }, color);
	}
};

