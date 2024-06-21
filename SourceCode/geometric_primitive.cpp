#include "geometric_primitive.h"

#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable : 4996)
#pragma warning(disable : 4244)
#pragma warning(disable : 4305)

#define PAR_SHAPES_IMPLEMENTATION
#include "par_shapes.h"

#include "shader.h"
#include "misc.h"

geometric_primitive::geometric_primitive(ID3D11Device* device, shape shape, int slices, int stacks)
{
	par_shapes_mesh* shapes_mesh = nullptr;
	switch (shape)
	{
	case shape::cube:
		shapes_mesh = par_shapes_create_cube();
		break;
	case shape::sphere:
		shapes_mesh = par_shapes_create_parametric_sphere(slices, stacks);
		break;
	case shape::cylinder:
		shapes_mesh = par_shapes_create_cylinder(slices, stacks);
		break;
	case shape::plane:
		shapes_mesh = par_shapes_create_plane(slices, stacks);
		break;
	case shape::dodecahedron:
		shapes_mesh = par_shapes_create_dodecahedron();
		break;
	}
	HRESULT hr{ S_OK };
	D3D11_BUFFER_DESC buffer_desc{};
	D3D11_SUBRESOURCE_DATA subresource_data{};
	buffer_desc.ByteWidth = static_cast<UINT>(sizeof(float) * shapes_mesh->npoints * 3);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	buffer_desc.CPUAccessFlags = 0;
	buffer_desc.MiscFlags = 0;
	buffer_desc.StructureByteStride = 0;
	subresource_data.pSysMem = shapes_mesh->points;
	subresource_data.SysMemPitch = 0;
	subresource_data.SysMemSlicePitch = 0;
	hr = device->CreateBuffer(&buffer_desc, &subresource_data, vertex_buffer.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	//頂点データの再利用を可能にし、メモリ使用量を削減
	buffer_desc.ByteWidth = static_cast<UINT>(sizeof(uint16_t) * shapes_mesh->ntriangles * 3);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	subresource_data.pSysMem = shapes_mesh->triangles;
	hr = device->CreateBuffer(&buffer_desc, &subresource_data, index_buffer.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	index_count = shapes_mesh->ntriangles * 3;

	D3D11_INPUT_ELEMENT_DESC input_element_desc[]
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	vertex_shader = shader<ID3D11VertexShader>::_emplace(device, "geometric_primitive_vs.cso", input_layout.ReleaseAndGetAddressOf(), input_element_desc, _countof(input_element_desc));
	pixel_shader = shader<ID3D11PixelShader>::_emplace(device, "geometric_primitive_ps.cso");

	buffer_desc.ByteWidth = sizeof(constants);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	hr = device->CreateBuffer(&buffer_desc, nullptr, constant_buffer.ReleaseAndGetAddressOf());


	par_shapes_free_mesh(shapes_mesh);
}

void geometric_primitive::draw(ID3D11DeviceContext* immediate_context, const DirectX::XMFLOAT4& position, const DirectX::XMFLOAT4& scale, const DirectX::XMFLOAT4& rotation, const DirectX::XMFLOAT4& color)
{
	DirectX::XMFLOAT4X4 composed_transform;
	float to_meters_scale = 1.0f;
	const DirectX::XMFLOAT4X4 coordinate_system_transforms[] = {
	{ -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 },	// 0:RHS Y-UP
	{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 },		// 1:LHS Y-UP
	{ 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1 },		// 2:RHS Z-UP
	{ -1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1 },	// 3:LHS Z-UP
	};
	DirectX::XMMATRIX C = DirectX::XMLoadFloat4x4(&coordinate_system_transforms[2]);
	DirectX::XMMATRIX S = DirectX::XMMatrixScaling(scale.x * to_meters_scale, scale.y * to_meters_scale, scale.z * to_meters_scale);
	DirectX::XMMATRIX R = DirectX::XMMatrixRotationRollPitchYaw(rotation.x, rotation.y, rotation.z);
	DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(position.x, position.y, position.z);
	DirectX::XMStoreFloat4x4(&composed_transform, C * S * R * T);

	constants data = { composed_transform, color };
	immediate_context->UpdateSubresource(constant_buffer.Get(), 0, 0, &data, 0, 0);
	immediate_context->VSSetConstantBuffers(0, 1, constant_buffer.GetAddressOf());
	immediate_context->PSSetConstantBuffers(0, 1, constant_buffer.GetAddressOf());

	UINT stride = sizeof(float) * 3;
	UINT offset = 0;
	immediate_context->IASetVertexBuffers(0, 1, vertex_buffer.GetAddressOf(), &stride, &offset);
	immediate_context->IASetIndexBuffer(index_buffer.Get(), DXGI_FORMAT_R16_UINT, 0);
	immediate_context->IASetInputLayout(input_layout.Get());
	immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	immediate_context->VSSetShader(vertex_shader.Get(), NULL, 0);
	immediate_context->PSSetShader(pixel_shader.Get(), NULL, 0);

	immediate_context->DrawIndexed(index_count, 0, 0);

}
