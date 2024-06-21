#pragma once
#include <d3d11.h>
#include <wrl.h>
#include <directxmath.h>
#include <vector>

class snowfall_particles
{
public:
	struct snowfall_particle
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 velocity;
	};
	std::vector<snowfall_particle> particles;

	struct particle_constants
	{
		DirectX::XMFLOAT4 current_eye_position;
		DirectX::XMFLOAT4 previous_eye_position;

		//ç≈äOé¸ãOìπÇÃîºåa 
		float outermost_radius;
		// ç~ê·àÊÇÃçÇÇ≥
		float snowfall_area_height;

		float particle_size;
		uint32_t particle_count;
	};
	particle_constants particle_data;

	snowfall_particles(ID3D11Device* device, DirectX::XMFLOAT4 initial_position);
	snowfall_particles(const snowfall_particles&) = delete;
	snowfall_particles& operator=(const snowfall_particles&) = delete;
	snowfall_particles(snowfall_particles&&) noexcept = delete;
	snowfall_particles& operator=(snowfall_particles&&) noexcept = delete;
	virtual ~snowfall_particles() = default;

	void integrate(ID3D11DeviceContext* immediate_context, float delta_time, DirectX::XMFLOAT4 eye_position);
	void render(ID3D11DeviceContext* immediate_context);

private:
	size_t snowfall_particle_count{ 0 };
	Microsoft::WRL::ComPtr<ID3D11Buffer> snowfall_particle_buffer;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> snowfall_particle_buffer_uav;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> snowfall_particle_buffer_srv;

	Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel_shader;
	Microsoft::WRL::ComPtr<ID3D11GeometryShader> geometry_shader;
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> compute_shader;
	Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer;

};