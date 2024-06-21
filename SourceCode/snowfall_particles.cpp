#include "snowfall_particles.h"

#include <random>

#include "shader.h"
#include "misc.h"

snowfall_particles::snowfall_particles(ID3D11Device* device, DirectX::XMFLOAT4 initial_position)
{
	// Radius of outermost orbit 
	float outermost_radius{ 30 };
	// Interval between two particles
	float interval{ 0.5f };
	// Height of snowfall area
	float snowfall_area_height{ 20 };
	// Falling speed of snowflake
	float fall_speed{ -0.5f };
	
	// Orbit count
	int orbit_count = static_cast<int>(outermost_radius / interval);

	for (int orbit_index = 1; orbit_index <= orbit_count; ++orbit_index)
	{
		float radius = orbit_index * interval;

		for (float theta = 0; theta < 2 * 3.14159265358979f; theta += interval / radius)
		{
			for (float height = 0; height < snowfall_area_height; height += interval)
			{
				snowfall_particle_count++;
			}
		}
	}
	particles.resize(snowfall_particle_count);

	std::mt19937 mt{ std::random_device{}() };
	std::uniform_real_distribution<float> rand(-interval * 0.5f, +interval * 0.5f);

	size_t index{ 0 };
	for (int orbit_index = 1; orbit_index <= orbit_count; ++orbit_index)
	{
		float radius = orbit_index * interval;

		for (float theta = 0; theta < 2 * 3.14159265358979f; theta += interval / radius)
		{
			const float x{ radius * cosf(theta) };
			const float z{ radius * sinf(theta) };
			for (float height = -snowfall_area_height * 0.5f; height < snowfall_area_height * 0.5f; height += interval)
			{
				particles.at(index).position = { x + initial_position.x + rand(mt), height + initial_position.y + rand(mt), z + initial_position.z + rand(mt) };
				particles.at(index++).velocity = { 0.0f, fall_speed + rand(mt) * (fall_speed * 0.5f), 0.0f };
			}
		}
	}

	HRESULT hr{ S_OK };

	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = static_cast<UINT>(sizeof(snowfall_particle) * snowfall_particle_count);
	buffer_desc.StructureByteStride = sizeof(snowfall_particle);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	buffer_desc.CPUAccessFlags = 0;
	buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	D3D11_SUBRESOURCE_DATA subresource_data{};
	subresource_data.pSysMem = particles.data();
	subresource_data.SysMemPitch = 0;
	subresource_data.SysMemSlicePitch = 0;
	hr = device->CreateBuffer(&buffer_desc, &subresource_data, snowfall_particle_buffer.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	D3D11_UNORDERED_ACCESS_VIEW_DESC unordered_access_view_desc;
	unordered_access_view_desc.Format = DXGI_FORMAT_UNKNOWN;
	unordered_access_view_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	unordered_access_view_desc.Buffer.FirstElement = 0;
	unordered_access_view_desc.Buffer.NumElements = static_cast<UINT>(snowfall_particle_count);
	unordered_access_view_desc.Buffer.Flags = 0;
	hr = device->CreateUnorderedAccessView(snowfall_particle_buffer.Get(), &unordered_access_view_desc, snowfall_particle_buffer_uav.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc;
	shader_resource_view_desc.Format = DXGI_FORMAT_UNKNOWN;
	shader_resource_view_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	shader_resource_view_desc.Buffer.ElementOffset = 0;
	shader_resource_view_desc.Buffer.NumElements = static_cast<UINT>(snowfall_particle_count);
	hr = device->CreateShaderResourceView(snowfall_particle_buffer.Get(), &shader_resource_view_desc, snowfall_particle_buffer_srv.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	buffer_desc.ByteWidth = sizeof(particle_constants);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	buffer_desc.CPUAccessFlags = 0;
	buffer_desc.MiscFlags = 0;
	buffer_desc.StructureByteStride = 0;
	hr = device->CreateBuffer(&buffer_desc, nullptr, constant_buffer.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	//create_vs_from_cso(device, "snowfall_particles_vs.cso", vertex_shader.ReleaseAndGetAddressOf(), nullptr, nullptr, 0);
	//create_ps_from_cso(device, "snowfall_particles_ps.cso", pixel_shader.ReleaseAndGetAddressOf());
	//create_gs_from_cso(device, "snowfall_particles_gs.cso", geometry_shader.ReleaseAndGetAddressOf());
	//create_cs_from_cso(device, "snowfall_particles_cs.cso", compute_shader.ReleaseAndGetAddressOf());

	vertex_shader = shader<ID3D11VertexShader>::_emplace(device, "snowfall_particles_vs.cso", NULL, NULL, 0);
	geometry_shader = shader<ID3D11GeometryShader>::_emplace(device, "snowfall_particles_gs.cso");
	compute_shader = shader<ID3D11ComputeShader>::_emplace(device, "snowfall_particles_cs.cso");
	pixel_shader = shader<ID3D11PixelShader>::_emplace(device, "snowfall_particles_ps.cso");

	particle_data.current_eye_position = initial_position;
	particle_data.previous_eye_position = initial_position;
	particle_data.snowfall_area_height = snowfall_area_height;
	particle_data.outermost_radius = outermost_radius;
	particle_data.particle_size = 0.01f;
	particle_data.particle_count = static_cast<uint32_t>(snowfall_particle_count);
}

static UINT align(UINT num, UINT alignment)
{
	return (num + (alignment - 1)) & ~(alignment - 1);
}
void snowfall_particles::integrate(ID3D11DeviceContext* immediate_context, float delta_time, DirectX::XMFLOAT4 eye_position)
{
	HRESULT hr{ S_OK };

	particle_data.previous_eye_position = particle_data.current_eye_position;
	particle_data.current_eye_position = eye_position;
	immediate_context->UpdateSubresource(constant_buffer.Get(), 0, 0, &particle_data, 0, 0);
	immediate_context->CSSetConstantBuffers(9, 1, constant_buffer.GetAddressOf());

	immediate_context->CSSetShader(compute_shader.Get(), NULL, 0);
	immediate_context->CSSetUnorderedAccessViews(0, 1, snowfall_particle_buffer_uav.GetAddressOf(), nullptr);

	UINT num_threads = align(static_cast<UINT>(snowfall_particle_count), 256);
	immediate_context->Dispatch(num_threads / 256, 1, 1);

	ID3D11UnorderedAccessView* null_unordered_access_view{};
	immediate_context->CSSetUnorderedAccessViews(0, 1, &null_unordered_access_view, nullptr);

}

void snowfall_particles::render(ID3D11DeviceContext* immediate_context)
{
	immediate_context->VSSetShader(vertex_shader.Get(), NULL, 0);
	immediate_context->PSSetShader(pixel_shader.Get(), NULL, 0);
	immediate_context->GSSetShader(geometry_shader.Get(), NULL, 0);
	immediate_context->GSSetShaderResources(9, 1, snowfall_particle_buffer_srv.GetAddressOf());
	immediate_context->GSSetConstantBuffers(9, 1, constant_buffer.GetAddressOf());

	immediate_context->IASetInputLayout(NULL);
	immediate_context->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	immediate_context->IASetIndexBuffer(NULL, DXGI_FORMAT_R32_UINT, 0);
	immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
	immediate_context->Draw(static_cast<UINT>(snowfall_particle_count), 0);

	ID3D11ShaderResourceView* null_shader_resource_view{};
	immediate_context->GSSetShaderResources(9, 1, &null_shader_resource_view);
	immediate_context->VSSetShader(NULL, NULL, 0);
	immediate_context->PSSetShader(NULL, NULL, 0);
	immediate_context->GSSetShader(NULL, NULL, 0);
}