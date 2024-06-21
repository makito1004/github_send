#pragma once
#include "scene.h"

#include <d3d11.h>
#include <wrl.h>

#include "sprite.h"

#include "framebuffer.h"
#include "fullscreen_quad.h"
#include "rendering_state.h"

#include "constant_buffer.h"
#include "shader.h"
#include "texture.h"
#include "audio.h"

#include "geometric_substance.h"
#include "framebuffer.h"
#include "fullscreen_quad.h"
#include "cascaded_shadow_map.h"

#include "bloom.h"
#include "collision_detection.h"
#include "collision_mesh.h"
#include "husk_particles.h"
#include "snowfall_particles.h"

#include "avatar.h"
#include "monster.h"
#include "camera.h"

#include "geometric_primitive.h"
#include "gamepad.h"
#include "rendering_state.h"

class main_scene : public scene
{
	enum class cb_slot { object, bone, material, scene, grass, shadow_map, bloom, atmosphere, post_effect };
	struct scene_constants
	{
		DirectX::XMFLOAT4X4 view;
		DirectX::XMFLOAT4X4 projection;
		DirectX::XMFLOAT4X4 view_projection;
		DirectX::XMFLOAT4X4 inverse_projection;
		DirectX::XMFLOAT4X4 inverse_view_projection;
		DirectX::XMFLOAT4 directional_light_direction[1] = { { 0, -1, 0, 0 } };
		DirectX::XMFLOAT4 directional_light_color[1]; // ｗは「強度」
		DirectX::XMFLOAT4 omni_light_position[1] = { { 0, 0, 0, 1 } };
		DirectX::XMFLOAT4 omni_light_color[1] = { { 0.171f, 0.616f, 0.679f, 1.000f } }; 
		DirectX::XMFLOAT4 rimlight_color = { 0.390f, 0.405f, 0.456f, 1.000f }; 
		DirectX::XMFLOAT4 camera_position = { 0.0f, 0.0f, -10.0f, 1.0f };
		DirectX::XMFLOAT4 camera_focus;
		DirectX::XMFLOAT4 avatar_position;
		DirectX::XMFLOAT4 avatar_direction;
		float time = 0;
		float delta_time = 0;
		float wind_frequency = 22.388f;
		float wind_strength = 0.5f;
		float rimlight_factor = 0.6f;
		float snow_factor = 0.0f;
	};
	float snow_factor = 0.0f;
	std::unique_ptr<constant_buffer<scene_constants>> cb_scene;

	float omni_light_intensity = 1;

	struct post_effect_constants
	{
		float brightness = 0.0f;
		float contrast = 0.10f;
		float hue = 0.000f;
		float saturation = 0.0f;

		float bokeh_aperture = 0.018f;
		float bokeh_focus = 0.824f;

		float exposure = 1.8f;
		float post_effect_options;

		// 投影テクスチャマッピング
		DirectX::XMFLOAT4 projection_texture_intensity = { 1.0f, 1.0f, 1.0f, 1.0f }; 
		DirectX::XMFLOAT4X4 projection_texture_transforms = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };

		float colorize[3] = { 1, 1, 1 };
	};
	std::unique_ptr<constant_buffer<post_effect_constants>> cb_post_effect;

	struct shadow_constants
	{
		float shadow_depth_bias = 0.000f;
		float shadow_color;
		float shadow_filter_radius = 4.000f;
		int shadow_sample_count = 32;
		int colorize_cascaded_layer = false;
	};
	float shadow_color = 0.2f;
	std::unique_ptr<constant_buffer<shadow_constants>> cb_shadow_map;

	struct bloom_constants
	{
		float bloom_extraction_threshold = 0.800f;
		float blur_convolution_intensity = 0.200f;
	};
	std::unique_ptr<constant_buffer<bloom_constants>> cb_bloom;
	bool enable_bloom = true;

	struct atmosphere_constants
	{
		float mist_color[4] = { 0.226f, 0.273f, 0.344f, 1.000f };
		float mist_density[2] = { 0.020f, 0.020f }; //x:消光、y:散乱
		float mist_height_falloff[2] = { 1000.000f, 1000.000f }; 
		float height_mist_offset[2] = { 244.300f, 335.505f };

		float mist_cutoff_distance = 0.0f;

		float mist_flow_speed = 118.123f;
		float mist_flow_noise_scale_factor = 0.015f;
		float mist_flow_density_lower_limit = 0.330f;

		float distance_to_sun = 500.0f;
		float sun_highlight_exponential_factor = 38.000f; // 太陽のハイライトの影響範囲に影響。
		float sun_highlight_intensity = 1.200f;
	};
	std::unique_ptr<constant_buffer<atmosphere_constants>> cb_atmosphere;

	enum class model { terrain, t1, sky_cube };
	std::unique_ptr<geometric_substance> geometric_substances[8];

	enum class offscreen { scene_msaa, scene_resolved, post_processed };
	std::unique_ptr<framebuffer> framebuffers[8];
	std::unique_ptr<fullscreen_quad> bit_block_transfer;

	enum class t_slot { t0, t1, t2, t3, environment, distortion, projection_texture, ramp, noise };
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_views[9];

	Microsoft::WRL::ComPtr<ID3D11PixelShader> cast_shadow_ps;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> post_effect_ps;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> tone_map_ps;
	

	Microsoft::WRL::ComPtr<ID3D11VertexShader> skymap_vs;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> skymap_ps;
	
	std::unique_ptr<rendering_state> rendering_state;

	float hour = 8.486f;
	float time_scale = 0.1f;
	float culmination_altitude = 133.260f;
	float black_point = 0.300f;
	float white_point = 1.500f;
	DirectX::XMFLOAT4 sunlight_dc_bias{ 0.500f, 0.200f, -0.200f, 1.0f };
	DirectX::XMFLOAT4 sunlight_amplitude{ 3.500f, 1.500f, 2.000f, 1.0f };

	std::unique_ptr<cascaded_shadow_map> _cascaded_shadow_map;
	float _critical_depth_value = 300;

	std::unique_ptr<bloom> bloom_effect;

	DirectX::XMFLOAT4X4 terrain_world_transform{ -0.01f, 0.0f, 0.0f, 0.0f, 0.0f, 
																			0.01f, 0.0f, 0.0f, 0.0f, 0.0f,
																			0.01f, 0.0f, 0.0f, 0.0f, 0.0f, 
																			1.0f };
	std::unique_ptr<collision_mesh> terrain_collision;


	bool enable_cast_shadow = true;
	bool enable_post_effects = true;

	
	bool enable_frustum_culling = true;

	bool visible_collision_shapes = false;

	bool enable_husk_particles = false;
	bool has_amassed_husk_particles = false;
	std::unique_ptr<husk_particles> particles;

	bool enable_projection_texture = true;
	float projection_texture_rotation = 0.0f;
	float projection_texture_altitude = 100.0f;
	float projection_texture_fovy = 3.0f;
	float projection_texture_intensity = 2.0f;

	

	SIZE framebuffer_dimensions;

	std::shared_ptr<avatar> nico;
	std::shared_ptr<boss> plantune;
	std::shared_ptr<camera> eye_view_camera;
	int detected_latha_count = 0;

	float nico_magic_wand_sphere_radius = 0.2f;
	float nico_root_sphere_radius = 1.2f;
	float nico_breadth = 0.9f;
	float nico_stature = 1.9f;
	float plantune_right_paw_sphere_radius = 2.0f;
	float plantune_core_sphere_radiuse = 1.4f;
	float plantune_breadth = 3.0f;
	float plantune_stature = 3.0f;
	std::shared_ptr<geometric_primitive> sphere;
	std::shared_ptr<geometric_primitive> cylinder;

	std::unique_ptr<sprite> logo;
	std::unique_ptr<sprite> latah_icon;
	std::unique_ptr<sprite> heart;
	std::unique_ptr<sprite> skull;
	std::unique_ptr<sprite> life_bar;
	std::unique_ptr<sprite> plantune_name;

	std::shared_ptr<audio> _audios[8];

	gamepad gamepad;

	

	bool initialize(ID3D11Device* device, UINT64 width, UINT height, const std::unordered_map<std::string, std::string>& props) override;
	void update(ID3D11DeviceContext* immediate_context, float delta_time) override;
	void render(ID3D11DeviceContext* immediate_context, float delta_time) override;
	bool uninitialize(ID3D11Device* device) override;
	bool on_size_changed(ID3D11Device* device, UINT64 width, UINT height) override;

	void draw_terrain(ID3D11DeviceContext* immediate_context, float delta_time);
	void draw_ui(ID3D11DeviceContext* immediate_context, float delta_time);
	void draw_collision_shape(ID3D11DeviceContext* immediate_context, float delta_time);
};
