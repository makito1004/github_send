#include "main_scene.h"

#include <memory>

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

#include "collision_detection.h"

#include "shader.h"
#include "texture.h"
#include "misc.h"

#include "event.h"


using namespace DirectX;

bool main_scene::initialize(ID3D11Device* device, UINT64 width, UINT height, const std::unordered_map<std::string, std::string>& props)
{
	framebuffer_dimensions.cx = static_cast<LONG>(width);
	framebuffer_dimensions.cy = height;

	rendering_state = std::make_unique<decltype(rendering_state)::element_type>(device);

	cb_scene = std::make_unique<constant_buffer<scene_constants>>(device);
	cb_post_effect = std::make_unique<constant_buffer<post_effect_constants>>(device);
	cb_shadow_map = std::make_unique<constant_buffer<shadow_constants>>(device);
	cb_bloom = std::make_unique<constant_buffer<bloom_constants>>(device);
	cb_atmosphere = std::make_unique<constant_buffer<atmosphere_constants>>(device);

#ifdef ENABLE_MSAA
	framebuffers[static_cast<size_t>(offscreen::scene_msaa)] = std::make_unique<framebuffer>(device, framebuffer_dimensions.cx, framebuffer_dimensions.cy, framebuffer::usage::color_depth, true/*enable_msaa*/, 8/*subsamples*/, false/*generate_mips*/);
#endif
	framebuffers[static_cast<size_t>(offscreen::scene_resolved)] = std::make_unique<framebuffer>(device, framebuffer_dimensions.cx, framebuffer_dimensions.cy, framebuffer::usage::color_depth);
	framebuffers[static_cast<size_t>(offscreen::post_processed)] = std::make_unique<framebuffer>(device, framebuffer_dimensions.cx, framebuffer_dimensions.cy, framebuffer::usage::color);

	bit_block_transfer = std::make_unique<fullscreen_quad>(device);

	post_effect_ps = shader<ID3D11PixelShader>::_emplace(device, "post_effect_ps.cso");
	cast_shadow_ps = shader<ID3D11PixelShader>::_emplace(device, "cast_shadow_csm_ps.cso");
	tone_map_ps = shader<ID3D11PixelShader>::_emplace(device, "tone_map_ps.cso");

	//あった方がいいかと思ったが、ない方が好きなため取り入れない
	skymap_vs = shader<ID3D11VertexShader>::_emplace(device, "skymap_vs.cso", NULL, NULL, 0);
	skymap_ps = shader<ID3D11PixelShader>::_emplace(device, "skymap_ps.cso");


	_cascaded_shadow_map = std::make_unique<cascaded_shadow_map>(device, 1024 * 4, 1024 * 4);
	bloom_effect = std::make_unique<bloom>(device, framebuffer_dimensions.cx, framebuffer_dimensions.cy);

	shader_resource_views[static_cast<size_t>(t_slot::environment)] = texture::_emplace(device, L".\\resources\\sky.jpg");
	shader_resource_views[static_cast<size_t>(t_slot::distortion)] = texture::_emplace(device, L".\\resources\\distortion texture.png");
	shader_resource_views[static_cast<size_t>(t_slot::projection_texture)] = texture::_emplace(device, L".\\resources\\The Nephilim Of Cross.jpg");
	shader_resource_views[static_cast<size_t>(t_slot::ramp)] = texture::_emplace(device, L".\\resources\\ramp.png");
	shader_resource_views[static_cast<size_t>(t_slot::noise)] = texture::_emplace(device, L".\\resources\\tv noise.png");

	geometric_substances[static_cast<size_t>(model::terrain)] = std::make_unique<geometric_substance>(device, ".\\resources\\Tr\\ST.fbx");

	//使わなかった
	geometric_substances[static_cast<size_t>(model::sky_cube)] = std::make_unique<geometric_substance>(device, ".\\resources\\cube.000.fbx");

	terrain_collision = std::make_unique<collision_mesh>(device, ".\\resources\\Tr\\ST.fbx");


	nico = actor::_emplace<avatar>("nico", device, XMFLOAT4{ -15.0f, 0.88f + 0.5f, 50.0f, 1.0f });
	plantune = actor::_emplace<boss>("plantune", device);


	eye_view_camera = actor::_emplace<camera>("eye_view_camera", nico->name.c_str(), nico->position(), nico->forward(), 5.0f/*focal_length*/, 1.0f/*height_above_ground*/);
	
	
	particles = std::make_unique<husk_particles>(device, 500000);

	sphere = std::make_shared<geometric_primitive>(device, geometric_primitive::shape::sphere, 16, 8);
	cylinder = std::make_shared<geometric_primitive>(device, geometric_primitive::shape::cylinder, 16, 1);

	logo = std::make_unique<sprite>(device, L".\\resources\\logo_s_w.png");
	latah_icon = std::make_unique<sprite>(device, L".\\resources\\hp.png");
	heart = std::make_unique<sprite>(device, L".\\resources\\gage.png");
	skull = std::make_unique<sprite>(device, L".\\resources\\skull.jpg");
	life_bar = std::make_unique<sprite>(device, L".\\resources\\lifebar.png");
	plantune_name = std::make_unique<sprite>(device, L".\\resources\\plantune.png");

	_audios[0] = audio::_emplace(L".\\resources\\009.wav");
	_audios[1] = audio::_emplace(L".\\resources\\mixkit-strong-wild-wind-in-a-storm-2407.wav"); // explosion-8-bit.wav : mixkit-strong-wild-wind-in-a-storm-2407 : hurricane-storm-nature-sounds-8397
	return true;
}

void main_scene::update(ID3D11DeviceContext* immediate_context, float delta_time)
{
	gamepad.acquire();

	if (gamepad.button_state(gamepad::button::back, trigger_mode::rising_edge))
	{
		scene::_transition("boot_scene", {});
	}

	cb_scene->data.time += delta_time;
	cb_scene->data.delta_time = delta_time;

	hour += time_scale * delta_time;
	hour = hour > 24.0f ? hour - 24.0f : hour;
	const float rotation_angle = XMConvertToRadians(hour / 24.0f * 360.0f);

	// 太陽光の月明かりの方向を計算
	XMVECTOR Z = XMVector3Normalize(XMVectorSet(0, 1, 0, 0)); // Zenith
	XMVECTOR A = XMVector3Transform(Z, XMMatrixRotationX(XMConvertToRadians(90.0f - culmination_altitude))); // The earth's axis
	XMMATRIX R = XMMatrixRotationAxis(XMVector3Transform(Z, XMMatrixRotationX(-culmination_altitude * 0.01745f)), rotation_angle);
	XMVECTOR L = XMVector3Normalize(XMVector3Transform(A, R));
	XMStoreFloat4(&cb_scene->data.directional_light_direction[0], L);

	//太陽光と月光の色を計算。
	XMFLOAT4 daylight = { 1.0f, 1.0f, 1.0f, 1.0f };
	XMFLOAT4 nightlight = { 0.2f, 0.2f, 0.5f, 1.0f };
	float cosine_curve = -cosf(rotation_angle);
	cb_scene->data.directional_light_color[0].x = std::min<float>(daylight.x, std::max<float>(nightlight.x, sunlight_amplitude.x * cosine_curve + sunlight_dc_bias.x));
	cb_scene->data.directional_light_color[0].y = std::min<float>(daylight.y, std::max<float>(nightlight.y, sunlight_amplitude.y * cosine_curve + sunlight_dc_bias.y));
	cb_scene->data.directional_light_color[0].z = std::min<float>(daylight.z, std::max<float>(nightlight.z, sunlight_amplitude.z * cosine_curve + sunlight_dc_bias.z));
	cb_scene->data.directional_light_color[0].w = std::max<float>(black_point, white_point * cosine_curve * (cb_scene->data.snow_factor > 0.0f ? 0.5f : 1.0f));
	omni_light_intensity = std::max<float>(0.2f, std::min(5.0f, 20.0f * -cosine_curve * (cb_scene->data.snow_factor > 0.0f ? 0.5f : 1.0f)));

	

	if (gamepad.button_state(gamepad::button::left_shoulder, trigger_mode::falling_edge))
	{
	}
	if (gamepad.button_state(gamepad::button::right_shoulder, trigger_mode::falling_edge))
	{
		visible_collision_shapes = !visible_collision_shapes;
	}

	event::_dispatch("@trigger_state", { gamepad.trigger_state_l(), gamepad.trigger_state_r() });
	event::_dispatch("@thumb_state_r", { gamepad.thumb_state_rx(), gamepad.thumb_state_ry() });
	event::_dispatch("@thumb_state_l", { gamepad.thumb_state_lx(), gamepad.thumb_state_ly() });
	if (gamepad.button_state(gamepad::button::a))
	{
		event::_dispatch("@button", { "a" });
	}
	if (gamepad.button_state(gamepad::button::b))
	{
		event::_dispatch("@button", { "b" });
	}
	if (gamepad.button_state(gamepad::button::x))
	{
		event::_dispatch("plantune@damaged", { 30.0f });
		XMVECTOR D = XMLoadFloat4(&plantune->position()) - XMLoadFloat4(&nico->position());
		XMVECTOR F = XMVector3Normalize(XMLoadFloat4(&nico->forward()));
		// 魔法攻撃は、プランチューンがニコの前にいるときに発動する。.
		// ニコとプランチューンの距離が30以下のとき、魔法攻撃が発動する。.
		// 発見されたラタの数だけ魔法攻撃が発動する.
	/*	if (XMVectorGetX(XMVector3Dot(XMVector3Normalize(D), F)) > 0.8 && XMVectorGetX(XMVector3Length(D)) < 30.0f && detected_latha_count > 0)
		{
			bool answer;
			event::_dispatch("nico@chant-magic", { &answer });
			if (answer)
			{
				XMFLOAT4  plantune_core_joint = plantune->core_joint();
				magic_target_position.x = plantune_core_joint.x;
				magic_target_position.y = plantune_core_joint.y;
				magic_target_position.z = plantune_core_joint.z;

				detected_latha_count = std::max(0, detected_latha_count - 1);
				enable_husk_particles = true;
			}
		}*/
		event::_dispatch("@button", { "x" });
	}
	if (gamepad.button_state(gamepad::button::y))
	{
		event::_dispatch("@button", { "y" });
		
		//DirectX::XMFLOAT3 lookDirection = { plantune->position().x - eye_view_camera->position().x ,
		//													 plantune->position().y - eye_view_camera->position().y  ,
		//													 plantune->position().z - eye_view_camera->position().z };
		//// カメラの向きを設定
		//DirectX::XMMATRIX viewMatrix = DirectX::XMMatrixLookAtLH(DirectX::XMLoadFloat4(&eye_view_camera->position()), DirectX::XMLoadFloat4(&eye_view_camera->focus()), DirectX::XMLoadFloat4(&eye_view_camera-()));
		//eye_view_camera->focus()= plantune->position();
		//
	}
	/*if (nico->tell_state() == avatar::state::attack)
	{
		event::_dispatch("plantune@damaged", { 30.0f });
	}*/
	//衝突判定
	if (plantune->tell_state() != boss::state::teleportion && plantune->tell_state() != boss::state::death)
	{
		float dx = plantune->position().x - nico->position().x;
		float dy = plantune->position().y - nico->position().y;
		float dz = plantune->position().z - nico->position().z;
		float distance = sqrtf(dx * dx + dz * dz);
		float penetration = distance - (plantune_breadth + nico_breadth) * 0.5f;
		if (penetration < 0)
		{
			event::_dispatch("nico@collided_with_plantune", { penetration });
		}
	}

	//衝突判定
	if (plantune->tell_state() == boss::state::attack)
	{
		XMFLOAT4 plantune_right_paw_joint = plantune->right_paw_joint();
		XMFLOAT4 nico_root_joint = nico->root_joint();
		float dx = nico_root_joint.x - plantune_right_paw_joint.x;
		float dy = nico_root_joint.y - plantune_right_paw_joint.y;
		float dz = nico_root_joint.z - plantune_right_paw_joint.z;
		float distance = sqrtf(dx * dx + dy * dy + dz * dz);
		if (distance < (nico_root_sphere_radius + plantune_right_paw_sphere_radius))
		{
			event::_dispatch("nico@damaged", { float{1} });
		}
	}

	if (nico->tell_state() == avatar::state::attack)
	{
		XMFLOAT4 nico_magic_wand_sphere_joint = nico->magic_wand_sphere_joint();
		XMFLOAT4 plantune_core_joint = plantune->core_joint();
		float dx = nico_magic_wand_sphere_joint.x - plantune_core_joint.x;
		float dy = nico_magic_wand_sphere_joint.y - plantune_core_joint.y;
		float dz = nico_magic_wand_sphere_joint.z - plantune_core_joint.z;
		float distance = sqrtf(dx * dx + dy * dy + dz * dz);
		if (distance < (nico_magic_wand_sphere_radius + plantune_core_sphere_radiuse))
		{
			event::_dispatch("plantune@damaged", { 2.0f });
		}
	}

	nico->collide_with(terrain_collision.get(), terrain_world_transform);
	nico->update(delta_time);
	nico->animation_transition(delta_time);
	nico->audio_transition(delta_time);

#if 0
	plantune->collide_with(terrain_collision.get(), terrain_world_transform);
#endif
	plantune->update(delta_time);
	plantune->animation_transition(delta_time);
	plantune->audio_transition(delta_time);

	
#if 0
	eye_view_camera->collide_with(terrain_collision.get(), terrain_world_transform);
#endif
	eye_view_camera->update(delta_time);


	if (enable_husk_particles && has_amassed_husk_particles)
	{

#if 1
	
#else
		XMFLOAT4  plantune_core_joint = plantune->core_joint();
		particles->particle_data.target_location.x = plantune_core_joint.x;
		particles->particle_data.target_location.y = plantune_core_joint.y;
		particles->particle_data.target_location.z = plantune_core_joint.z;
#endif
		particles->particle_data.emitter_location = nico->position();
		particles->integrate(immediate_context, delta_time);
		
	}


	if (nico->current_location() == "collision_boss_area_mtl")
	{
		_audios[0]->play();
		_audios[0]->volume(0.5f);
	}
#if 0
	else if (_audios[0]->queuing())
	{
		_audios[0]->stop();
	}
#endif


	if (enable_husk_particles)
	{


	}
	
	else if (_audios[1]->queuing())
	{
		_audios[1]->stop();
	}

#ifdef USE_IMGUI

	static bool enable_imgui{ false };
	if (GetAsyncKeyState(VK_F6) & 1)
	{
		enable_imgui = !enable_imgui;
	}
	if (enable_imgui)
	{
		ImGui::Begin("ImGUI");
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
#if 0
		ImGui::Text("Video memory usage %d MB", video_memory_usage());
		ImGui::Text("framebuffer_dimensions : %d, %d", framebuffer_dimensions.cx, framebuffer_dimensions.cy);
#endif
		ImGui::SliderFloat("hour ", &hour, 0.0f, 24.0f);
		ImGui::SliderFloat("time_scale", &time_scale, 0.0f, 1.0f);
		ImGui::SliderFloat("culmination_altitude", &culmination_altitude, 0.0f, 180.0f);
		ImGui::SliderFloat("white_point", &white_point, 0.0f, 3.0f);
		ImGui::SliderFloat("black_point", &black_point, 0.0f, 1.0f);
		ImGui::SliderFloat("exposure", &cb_post_effect->data.exposure, +0.0f, +10.0f);

		ImGui::Checkbox("enable_cast_shadow", &enable_cast_shadow);
		ImGui::Checkbox("enable_post_effects", &enable_post_effects);

		ImGui::Checkbox("enable_frustum_culling", &enable_frustum_culling);

		if (ImGui::CollapsingHeader("avatar configuration"))
		{
			ImGui::Text("avatar's location %.2f, %.2f, %.2f, %.2f", nico->position().x, nico->position().y, nico->position().z, nico->position().w);
			ImGui::Text("avatar's scale %.2f, %.2f, %.2f, %.2f", nico->scale().x, nico->scale().y, nico->scale().z, nico->scale().w);
			ImGui::Text("state : %d", nico->tell_state());
			ImGui::Text("heart_point : %d", nico->heart_point());
			ImGui::Text("current_location : %s", nico->current_location().c_str());

			ImGui::Text("boss_heart_point : %d", plantune->health_point());
		}
		if (ImGui::CollapsingHeader("collision configuration"))
		{
			ImGui::Checkbox("visible_collision_shapes", &visible_collision_shapes);
			
			ImGui::SliderFloat("nico_root_sphere_radius", &nico_root_sphere_radius, 0.0f, 24.0f);
			ImGui::SliderFloat("plantune_right_paw_sphere_radius", &plantune_right_paw_sphere_radius, 0.0f, 24.0f);
			ImGui::SliderFloat("plantune_core_sphere_radiuse", &plantune_core_sphere_radiuse, 0.0f, 24.0f);
		}
		if (ImGui::CollapsingHeader("camera configuration"))
		{
			ImGui::Text("eye_view_camera's location %.2f, %.2f, %.2f, %.2f", eye_view_camera->position().x, eye_view_camera->position().y, eye_view_camera->position().z, eye_view_camera->position().w);
			ImGui::DragFloat("focal_length", &eye_view_camera->_focal_length);
			ImGui::DragFloat("_focus_offset_y", &eye_view_camera->_focus_offset_y);
			ImGui::SliderFloat("field_of_view", &eye_view_camera->_field_of_view, 0.0f, 180.0f);
			ImGui::SliderFloat("near_z", &eye_view_camera->_near_z, 0.01f, 1.0f);
			ImGui::SliderFloat("far_z", &eye_view_camera->_far_z, 100.0f, 1000.0f);
		}
		if (ImGui::CollapsingHeader("directional light configuration"))
		{
			ImGui::SliderFloat("directional_light_direction.x", &cb_scene->data.directional_light_direction[0].x, -1.0f, +1.0f);
			ImGui::SliderFloat("directional_light_direction.y", &cb_scene->data.directional_light_direction[0].y, -1.0f, +1.0f);
			ImGui::SliderFloat("directional_light_direction.z", &cb_scene->data.directional_light_direction[0].z, -1.0f, +1.0f);
			ImGui::ColorEdit3("directional_light_color", reinterpret_cast<float*>(&cb_scene->data.directional_light_color[0]));
			ImGui::SliderFloat("directional_light_color intensity", &cb_scene->data.directional_light_color[0].w, +0.0f, +10.0f);
			ImGui::SliderFloat("black_point", &black_point, +0.0f, +10.0f);
			ImGui::SliderFloat("white_point", &white_point, +0.0f, +10.0f);

			ImGui::DragFloat3("sunlit dc_bias", reinterpret_cast<float*>(&sunlight_dc_bias), 0.001f);
			ImGui::DragFloat3("sunlit amplitude", reinterpret_cast<float*>(&sunlight_amplitude), 0.001f);
		}
		if (ImGui::CollapsingHeader("omni light configuration"))
		{
			ImGui::ColorEdit3("omni_light_color", reinterpret_cast<float*>(&cb_scene->data.omni_light_color[0]));
			ImGui::SliderFloat("omni_light_intensity", &omni_light_intensity, +0.0f, +10.0f);
		}
		if (ImGui::CollapsingHeader("rimlight configuration"))
		{
			ImGui::ColorEdit3("rimlight_color", reinterpret_cast<float*>(&cb_scene->data.rimlight_color));
			ImGui::SliderFloat("rimlight_color intensity", &cb_scene->data.rimlight_color.w, +0.0f, +10.0f);
			ImGui::SliderFloat("rimlight_factor", &cb_scene->data.rimlight_factor, +0.0f, +1.0f);
		}
		if (ImGui::CollapsingHeader("wind configuration"))
		{
			ImGui::SliderFloat("wind_frequency", &cb_scene->data.wind_frequency, +0.0f, +100.0f);
			ImGui::SliderFloat("wind_strength", &cb_scene->data.wind_strength, +0.0f, +2.0f);
		}
		if (ImGui::CollapsingHeader("bloom configuration"))
		{
			ImGui::Checkbox("enable_bloom", &enable_bloom);
			ImGui::SliderFloat("bloom_extraction_threshold", &cb_bloom->data.bloom_extraction_threshold, +0.0f, +10.0f);
			ImGui::SliderFloat("blur_convolution_intensity", &cb_bloom->data.blur_convolution_intensity, +0.0f, +10.0f);
			//ImGui::SliderFloat("gaussianblur", &cb_bloom->data., +0.0f, +10.0f);
		}
		if (ImGui::CollapsingHeader("shadow map configuration"))
		{

			ImGui::DragFloat("split_scheme_weight", &_cascaded_shadow_map->_split_scheme_weight, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("critical_depth_value", &_critical_depth_value, 1.0f, 0.0f, 1000.0f);

			ImGui::Checkbox("colorize_cascaded_layer", reinterpret_cast<bool*>(&cb_shadow_map->data.colorize_cascaded_layer));
			ImGui::SliderFloat("shadow_color", &shadow_color, 0.0f, 1.0f);
			ImGui::SliderFloat("shadow_depth_bias", &cb_shadow_map->data.shadow_depth_bias, 0.0f, 0.005f, "%.8f");
			ImGui::SliderFloat("shadow_filter_radius", &cb_shadow_map->data.shadow_filter_radius, 0.0f, 64.0f);
			ImGui::SliderInt("shadow_sample_count", reinterpret_cast<int*>(&cb_shadow_map->data.shadow_sample_count), 0, 64);
			ImGui::Image(_cascaded_shadow_map->_shader_resource_view.Get(), { 256, 256 });
		}
		
		if (ImGui::CollapsingHeader("atmosphere configuration"))
		{
			ImGui::SliderFloat("extinction:mist_density", &cb_atmosphere->data.mist_density[0], 0.0f, 1.0f);
			ImGui::SliderFloat("extinction:mist_height_falloff", &cb_atmosphere->data.mist_height_falloff[0], -1000.0f, 1000.0f);
			ImGui::SliderFloat("extinction:height_mist_offset", &cb_atmosphere->data.height_mist_offset[0], -1000.0f, 1000.0f);
			ImGui::SliderFloat("inscattering:mist_density", &cb_atmosphere->data.mist_density[1], 0.0f, 1.0f);
			ImGui::SliderFloat("inscattering:mist_height_falloff", &cb_atmosphere->data.mist_height_falloff[1], -1000.0f, 1000.0f);
			ImGui::SliderFloat("inscattering:height_mist_offset", &cb_atmosphere->data.height_mist_offset[1], -1000.0f, 1000.0f);
			ImGui::SliderFloat("mist_cutoff_distance", &cb_atmosphere->data.mist_cutoff_distance, 0.0f, 500.0f);
			ImGui::ColorEdit3("mist_color", cb_atmosphere->data.mist_color);

			ImGui::SliderFloat("mist_flow_speed", &cb_atmosphere->data.mist_flow_speed, 0.0f, 500.0f);
			ImGui::SliderFloat("mist_flow_noise_scale_factor", &cb_atmosphere->data.mist_flow_noise_scale_factor, 0.0f, 0.1f);
			ImGui::SliderFloat("mist_flow_density_lower_limit", &cb_atmosphere->data.mist_flow_density_lower_limit, 0.0f, 1.0f);

			ImGui::SliderFloat("distance_to_sun", &cb_atmosphere->data.distance_to_sun, 0.0f, 1000.0f);
			ImGui::SliderFloat("sun_highlight_exponential_factor", &cb_atmosphere->data.sun_highlight_exponential_factor, 0.0f, 500.0f);
			ImGui::SliderFloat("sun_highlight_intensity", &cb_atmosphere->data.sun_highlight_intensity, 0.0f, 5.0f);
		}
		if (ImGui::CollapsingHeader("bokeh configuration"))
		{
			ImGui::SliderFloat("bokeh_aperture", &cb_post_effect->data.bokeh_aperture, 0.0f, 0.1f);
			ImGui::SliderFloat("bokeh_focus", &cb_post_effect->data.bokeh_focus, 0.0f, 1.0f);
		}
		if (ImGui::CollapsingHeader("color adjustment"))
		{
			ImGui::SliderFloat("brightness", &cb_post_effect->data.brightness, -1.0f, +1.0f);
			ImGui::SliderFloat("contrast", &cb_post_effect->data.contrast, -1.0f, +1.0f);
			ImGui::SliderFloat("hue", &cb_post_effect->data.hue, -1.0f, +1.0f);
			ImGui::SliderFloat("saturation", &cb_post_effect->data.saturation, -1.0f, +1.0f);
			ImGui::ColorEdit3("colorize", cb_post_effect->data.colorize);
		}
		if (ImGui::CollapsingHeader("projection texture configuration"))
		{
			ImGui::Checkbox("enable_projection_texture", &enable_projection_texture);
			ImGui::SliderFloat("altitude", &projection_texture_altitude, +0.0f, +100.0f);
			ImGui::SliderFloat("fovy", &projection_texture_fovy, +0.0f, +180.0f);
			ImGui::SliderFloat("intensity", &projection_texture_intensity, +0.0f, +10.0f);
		}

		ImGui::End();
	}
#endif

}

void main_scene::render(ID3D11DeviceContext* immediate_context, float delta_time)
{
	rendering_state->bind_sampler_states(immediate_context);

	D3D11_VIEWPORT viewport;
	UINT num_viewports = 1;
	immediate_context->RSGetViewports(&num_viewports, &viewport);
	const float aspect_ratio{ viewport.Width / viewport.Height };

	XMMATRIX P = XMLoadFloat4x4(&eye_view_camera->perspective_projection_matrix(aspect_ratio));
	XMMATRIX V = XMLoadFloat4x4(&eye_view_camera->view_matrix());
	XMStoreFloat4x4(&cb_scene->data.view, V);
	XMStoreFloat4x4(&cb_scene->data.projection, P);
	XMStoreFloat4x4(&cb_scene->data.view_projection, V * P);
	XMStoreFloat4x4(&cb_scene->data.inverse_projection, XMMatrixInverse(NULL, P));
	XMStoreFloat4x4(&cb_scene->data.inverse_view_projection, XMMatrixInverse(NULL, V * P));
	cb_scene->data.camera_position = eye_view_camera->position();
	cb_scene->data.camera_focus = eye_view_camera->focus();
	cb_scene->data.avatar_position = nico->position();
	cb_scene->data.avatar_direction = nico->forward();
	if (enable_husk_particles)
	{
		
		cb_scene->data.omni_light_position[0].x = (particles->particle_data.emitter_location.x + particles->particle_data.target_location.x) * 0.5f;
		cb_scene->data.omni_light_position[0].y = (particles->particle_data.emitter_location.y + particles->particle_data.target_location.y) * 0.5f + 20.0f;
		cb_scene->data.omni_light_position[0].z = (particles->particle_data.emitter_location.z + particles->particle_data.target_location.z) * 0.5f;
		cb_scene->data.omni_light_position[0].w = 1.0f;
		cb_scene->data.omni_light_color[0] = { cb_scene->data.directional_light_color[0].x, cb_scene->data.directional_light_color[0].y, cb_scene->data.directional_light_color[0].z, 10.0f * (sinf(cb_scene->data.time * 5.0f) + 1.0f) };
	}
	else
	{
		// オムニライトの位置と色をシーン・コンスタント・バッファに設定する。
	
		cb_scene->data.omni_light_color[0].w = omni_light_intensity * (white_point - cb_scene->data.directional_light_color[0].w) * (cb_scene->data.snow_factor > 0.0f ? 0.5f : 1.0f);
	}
	if (nico->current_location() == "collision_cave_mtl")
	{
		cb_scene->data.snow_factor = 0.0f;
		cb_shadow_map->data.shadow_color = 1.0f;
	}

	else
	{
		cb_scene->data.snow_factor = snow_factor;
		cb_shadow_map->data.shadow_color = shadow_color;
	}
	cb_scene->activate(immediate_context, static_cast<size_t>(cb_slot::scene), cb_usage::vhdgpc);
	cb_post_effect->activate(immediate_context, static_cast<size_t>(cb_slot::post_effect), cb_usage::p);
	cb_bloom->activate(immediate_context, static_cast<size_t>(cb_slot::bloom), cb_usage::p);
	cb_atmosphere->activate(immediate_context, static_cast<size_t>(cb_slot::atmosphere), cb_usage::p);

	// シャドウマップを作る
	rendering_state->bind_blend_state(immediate_context, blend_state::none);
	rendering_state->bind_depth_stencil_state(immediate_context, depth_stencil_state::zt_on_zw_on);
	rendering_state->bind_rasterizer_state(immediate_context, rasterizer_state::cull_front);
	_cascaded_shadow_map->make(immediate_context, cb_scene->data.view, cb_scene->data.projection, cb_scene->data.directional_light_direction[0], _critical_depth_value, [&]() {
		if (!has_amassed_husk_particles)
		{
			nico->cast_shadow(immediate_context);
		}

		plantune->cast_shadow(immediate_context);
		geometric_substances[static_cast<size_t>(model::terrain)]->cast_shadow(immediate_context, terrain_world_transform, nullptr);
		});

#ifdef ENABLE_MSAA
	framebuffers[static_cast<size_t>(offscreen::scene_msaa)]->clear(immediate_context);
	framebuffers[static_cast<size_t>(offscreen::scene_msaa)]->activate(immediate_context);
#else
	framebuffers[static_cast<size_t>(offscreen::scene_resolved)]->clear(immediate_context);
	framebuffers[static_cast<size_t>(offscreen::scene_resolved)]->activate(immediate_context);

#endif

	// 環境マップをバインド
	immediate_context->PSSetShaderResources(static_cast<UINT>(t_slot::environment), 1, shader_resource_views[static_cast<size_t>(t_slot::environment)].GetAddressOf());
	// ランプマップをバインド
	immediate_context->PSSetShaderResources(static_cast<UINT>(t_slot::ramp), 1, shader_resource_views[static_cast<size_t>(t_slot::ramp)].GetAddressOf());
	//ノイズマップをバインド
	immediate_context->PSSetShaderResources(static_cast<UINT>(t_slot::noise), 1, shader_resource_views[static_cast<size_t>(t_slot::noise)].GetAddressOf());
	//distortion mapをバインド
	immediate_context->PSSetShaderResources(static_cast<UINT>(t_slot::distortion), 1, shader_resource_views[static_cast<size_t>(t_slot::distortion)].GetAddressOf());
	// 投影マップをバインド
	immediate_context->PSSetShaderResources(static_cast<UINT>(t_slot::projection_texture), 1, shader_resource_views[static_cast<size_t>(t_slot::projection_texture)].GetAddressOf());
	

	rendering_state->bind_blend_state(immediate_context, blend_state::alpha);
	rendering_state->bind_depth_stencil_state(immediate_context, depth_stencil_state::zt_on_zw_on);
	rendering_state->bind_rasterizer_state(immediate_context, rasterizer_state::solid);
	if (!enable_husk_particles)
	{
		nico->render(immediate_context);
	}
	plantune->render(immediate_context);

	draw_terrain(immediate_context, delta_time);


	if (visible_collision_shapes)
	{
		draw_collision_shape(immediate_context, delta_time);
	}


	if (enable_husk_particles)
	{
		// Husk particlesプロセス
		if (!has_amassed_husk_particles)
		{
			rendering_state->bind_blend_state(immediate_context, blend_state::none);
			rendering_state->bind_depth_stencil_state(immediate_context, depth_stencil_state::zt_on_zw_on);
			rendering_state->bind_rasterizer_state(immediate_context, rasterizer_state::cull_none);
			particles->amass_husk_particles(immediate_context, [&](ID3D11PixelShader* accumulate_husk_particles_ps) {
				nico->render(immediate_context, accumulate_husk_particles_ps);
				});
			has_amassed_husk_particles = true;
		}

		// Husk particles 描画
		rendering_state->bind_depth_stencil_state(immediate_context, depth_stencil_state::zt_on_zw_on);
		rendering_state->bind_rasterizer_state(immediate_context, rasterizer_state::solid);
		switch (particles->state)
		{
		case 0:
		case 1:
			particles->particle_data.particle_size = 0.01f;
			particles->particle_data.streak_factor = 0.00f;
			rendering_state->bind_blend_state(immediate_context, blend_state::add);
			break;
		case 2:
		case 3:
			particles->particle_data.particle_size = 0.01f;
			particles->particle_data.streak_factor = 0.5f;
			rendering_state->bind_blend_state(immediate_context, blend_state::add);
			break;
		case 4:
			particles->particle_data.particle_size = 0.01f;
			particles->particle_data.streak_factor = 0.0f;
			rendering_state->bind_blend_state(immediate_context, blend_state::add);
			break;
		case 5:
			particles->reset(immediate_context);
			
			enable_husk_particles = has_amassed_husk_particles = false;
			break;
		default:
			break;
		}
		particles->render(immediate_context);
	}

#ifdef ENABLE_MSAA
	framebuffers[static_cast<size_t>(offscreen::scene_msaa)]->deactivate(immediate_context);

	rendering_state->bind_blend_state(immediate_context, blend_state::alpha);
	rendering_state->bind_depth_stencil_state(immediate_context, depth_stencil_state::zt_on_zw_on);
	rendering_state->bind_rasterizer_state(immediate_context, rasterizer_state::cull_none);
	framebuffers[static_cast<size_t>(offscreen::scene_msaa)]->resolve(immediate_context, framebuffers[static_cast<size_t>(offscreen::scene_resolved)].get());
#else
	framebuffers[static_cast<size_t>(offscreen::scene_resolved)]->deactivate(immediate_context);
#endif

	// 影の描写.
	if (enable_cast_shadow)
	{
		cb_shadow_map->activate(immediate_context, static_cast<size_t>(cb_slot::shadow_map), cb_usage::p);
		framebuffers[static_cast<size_t>(offscreen::scene_resolved)]->activate(immediate_context, framebuffer::usage::color);
		rendering_state->bind_blend_state(immediate_context, blend_state::multiply);
		rendering_state->bind_depth_stencil_state(immediate_context, depth_stencil_state::zt_off_zw_off);
		rendering_state->bind_rasterizer_state(immediate_context, rasterizer_state::cull_none);
		//ID3D11ShaderResourceView* cast_shadow_views[] = { framebuffers[static_cast<size_t>(offscreen::scene_resolved)]->depth_map().Get(), double_speed_z->depth_map().Get() };
		ID3D11ShaderResourceView* cast_shadow_views[] = { framebuffers[static_cast<size_t>(offscreen::scene_resolved)]->depth_map().Get(), _cascaded_shadow_map->_shader_resource_view.Get() };
		bit_block_transfer->blit(immediate_context, cast_shadow_views, 0, _countof(cast_shadow_views), cast_shadow_ps.Get());
		framebuffers[static_cast<size_t>(offscreen::scene_resolved)]->deactivate(immediate_context);
	}

	// スカイマップ描画処理
	framebuffers[static_cast<size_t>(offscreen::scene_resolved)]->activate(immediate_context, framebuffer::usage::color_depth_stencil);
	rendering_state->bind_blend_state(immediate_context, blend_state::alpha);
	rendering_state->bind_depth_stencil_state(immediate_context, depth_stencil_state::zt_on_zw_on);
	rendering_state->bind_rasterizer_state(immediate_context, rasterizer_state::solid);
	const float dimension{ 350.0f };
	geometric_substances[static_cast<size_t>(model::sky_cube)]->render(immediate_context, { dimension, 0.0f, 0.0f, 0.0f, 0.0f, dimension, 0.0f, 0.0f, 0.0f, 0.0f, dimension, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f }, nullptr,
		[&](const geometric_substance::mesh& mesh, const geometric_substance::material& material, geometric_substance::shader_resources& shader_resources, geometric_substance::pipeline_state& pipeline_state) {
			pipeline_state.vertex_shader = skymap_vs.Get();
			pipeline_state.pixel_shader = skymap_ps.Get();
			return 0;
		});
	framebuffers[static_cast<size_t>(offscreen::scene_resolved)]->deactivate(immediate_context);

	if (enable_post_effects)
	{
		// ポストエフェクト処理
		framebuffers[static_cast<size_t>(offscreen::post_processed)]->clear(immediate_context, framebuffer::usage::color);
		framebuffers[static_cast<size_t>(offscreen::post_processed)]->activate(immediate_context, framebuffer::usage::color);

		//投影テクスチャ変換を計算
		if (enable_projection_texture && projection_texture_fovy > 0.0f)
		{
			projection_texture_rotation += delta_time * 180;
			XMStoreFloat4x4(&cb_post_effect->data.projection_texture_transforms,
				XMMatrixLookAtLH(
					XMVectorSet(nico->position().x, nico->position().y + projection_texture_altitude, nico->position().z, 1.0f),
					XMVectorSet(nico->position().x, nico->position().y, nico->position().z, 1.0f),
					XMVector3Transform(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), XMMatrixRotationRollPitchYaw(0, XMConvertToRadians(projection_texture_rotation), 0))) *
				XMMatrixPerspectiveFovLH(XMConvertToRadians(projection_texture_fovy), 1.0f, 1.0f, 500.0f)
			);
			cb_post_effect->data.projection_texture_intensity.w = has_amassed_husk_particles ? projection_texture_intensity : 0.0f;
		}

		else
		{
			cb_post_effect->data.projection_texture_intensity.w = 0.0f;
		}

		rendering_state->bind_blend_state(immediate_context, blend_state::none);
		rendering_state->bind_depth_stencil_state(immediate_context, depth_stencil_state::zt_off_zw_off);
		rendering_state->bind_rasterizer_state(immediate_context, rasterizer_state::cull_none);
		ID3D11ShaderResourceView* post_effects_textures[]{
			framebuffers[static_cast<size_t>(offscreen::scene_resolved)]->color_map().Get(),
			framebuffers[static_cast<size_t>(offscreen::scene_resolved)]->depth_map().Get() };
		bit_block_transfer->blit(immediate_context, post_effects_textures, 0, _countof(post_effects_textures), post_effect_ps.Get());
		framebuffers[static_cast<size_t>(offscreen::post_processed)]->deactivate(immediate_context);

		//高輝度成分を抽出し、ぼやけた画像を生成する。
		if (enable_bloom)
		{
			rendering_state->bind_blend_state(immediate_context, blend_state::none);
			rendering_state->bind_depth_stencil_state(immediate_context, depth_stencil_state::zt_off_zw_off);
			rendering_state->bind_rasterizer_state(immediate_context, rasterizer_state::cull_none);
			bloom_effect->make(immediate_context, framebuffers[static_cast<size_t>(offscreen::post_processed)]->color_map().Get());

			framebuffers[static_cast<size_t>(offscreen::post_processed)]->activate(immediate_context, framebuffer::usage::color);
			rendering_state->bind_blend_state(immediate_context, blend_state::add);
			rendering_state->bind_depth_stencil_state(immediate_context, depth_stencil_state::zt_off_zw_off);
			rendering_state->bind_rasterizer_state(immediate_context, rasterizer_state::cull_none);
			bloom_effect->blit(immediate_context);
			framebuffers[static_cast<size_t>(offscreen::post_processed)]->deactivate(immediate_context);
		}

		// Tone mapping
		rendering_state->bind_blend_state(immediate_context, blend_state::none);
		rendering_state->bind_depth_stencil_state(immediate_context, depth_stencil_state::zt_off_zw_off);
		rendering_state->bind_rasterizer_state(immediate_context, rasterizer_state::cull_none);
		bit_block_transfer->blit(immediate_context, framebuffers[static_cast<size_t>(offscreen::post_processed)]->color_map().GetAddressOf(), 0, 1, tone_map_ps.Get());
	}
	else
	{
		// Tone mapping
		rendering_state->bind_blend_state(immediate_context, blend_state::none);
		rendering_state->bind_depth_stencil_state(immediate_context, depth_stencil_state::zt_off_zw_off);
		rendering_state->bind_rasterizer_state(immediate_context, rasterizer_state::cull_none);
		bit_block_transfer->blit(immediate_context, framebuffers[static_cast<size_t>(offscreen::scene_resolved)]->color_map().GetAddressOf(), 0, 1, tone_map_ps.Get());
	}

	draw_ui(immediate_context, delta_time);

#if 0
	bit_block_transfer->blit(immediate_context, _cascade_shadow_map->_shader_resource_view.GetAddressOf(), 0, 1);
#endif

}

bool main_scene::uninitialize(ID3D11Device* device)
{
	return true;
}

bool main_scene::on_size_changed(ID3D11Device* device, UINT64 width, UINT height)
{
	framebuffer_dimensions.cx = static_cast<LONG>(width);
	framebuffer_dimensions.cy = height;

#ifdef ENABLE_MSAA
	framebuffers[static_cast<size_t>(offscreen::scene_msaa)] = std::make_unique<framebuffer>(device, framebuffer_dimensions.cx, framebuffer_dimensions.cy, framebuffer::usage::color_depth, true/*enable_msaa*/, 8/*subsamples*/, false/*generate_mips*/);
#endif
	framebuffers[static_cast<size_t>(offscreen::scene_resolved)] = std::make_unique<framebuffer>(device, framebuffer_dimensions.cx, framebuffer_dimensions.cy, framebuffer::usage::color_depth);
	framebuffers[static_cast<size_t>(offscreen::post_processed)] = std::make_unique<framebuffer>(device, framebuffer_dimensions.cx, framebuffer_dimensions.cy, framebuffer::usage::color);
	bloom_effect = std::make_unique<bloom>(device, framebuffer_dimensions.cx, framebuffer_dimensions.cy);

	return true;
}

void main_scene::draw_terrain(ID3D11DeviceContext* immediate_context, float delta_time)
{
	view_frustum view_frustum(cb_scene->data.view_projection);

	rendering_state->bind_blend_state(immediate_context, blend_state::alpha);
	rendering_state->bind_depth_stencil_state(immediate_context, depth_stencil_state::zt_on_zw_on);
	rendering_state->bind_rasterizer_state(immediate_context, rasterizer_state::solid);
	geometric_substances[static_cast<size_t>(model::terrain)]->render(immediate_context, terrain_world_transform, nullptr,
		[&](const geometric_substance::mesh& mesh, const geometric_substance::material& material, geometric_substance::shader_resources& shader_resources, geometric_substance::pipeline_state& pipeline_state) {

#if 1
			XMFLOAT3 bounding_box[2];
			mesh.transform_bounding_box(terrain_world_transform, bounding_box);
			if (enable_frustum_culling && intersect_frustum_aabb(view_frustum, bounding_box))
			{
				return -1;
			}
#endif

			if (mesh.name == "cave_plant_mdl")
			{
				shader_resources.material_data.emissive.x = 1.0f;
				shader_resources.material_data.emissive.y = 1.0f;
				shader_resources.material_data.emissive.z = 1.0f;
				shader_resources.material_data.emissive.w = 5.0f;
			}
			else if (mesh.name == "cave_leaf_mdl")
			{
				shader_resources.material_data.diffuse.x = 5.0f;
				shader_resources.material_data.diffuse.y = 5.0f;
				shader_resources.material_data.diffuse.z = 5.0f;
			}
			else if (mesh.name == "cave_in_mdl")
			{
				shader_resources.material_data.diffuse.x = 5.0f;
				shader_resources.material_data.diffuse.y = 5.0f;
				shader_resources.material_data.diffuse.z = 5.0f;
			}
			else if (mesh.name == "area_plan_stage_mdl")
			{
				shader_resources.material_data.specular.x = 0.02f;
				shader_resources.material_data.specular.y = 0.02f;
				shader_resources.material_data.specular.z = 0.02f;
				shader_resources.material_data.specular.w = 8.0f;
			}

			 if (material.name == "_trees_leaf_bottom_mtl")
			{
				pipeline_state.rasterizer_state = rendering_state->rasterizer_state(rasterizer_state::cull_none);
				pipeline_state.blend_state = rendering_state->blend_state(blend_state::alpha_to_coverage);
			}
			else if (material.name == "_trees_leaf_top_mtl")
			{
				pipeline_state.rasterizer_state = rendering_state->rasterizer_state(rasterizer_state::cull_none);
				pipeline_state.blend_state = rendering_state->blend_state(blend_state::alpha_to_coverage);
			}
			else if (material.name == "_trees_leaf_middle_mtl")
			{
				pipeline_state.rasterizer_state = rendering_state->rasterizer_state(rasterizer_state::cull_none);
				pipeline_state.blend_state = rendering_state->blend_state(blend_state::alpha_to_coverage);
			}
			else if (material.name == "leaf_mtl")
			{
				pipeline_state.rasterizer_state = rendering_state->rasterizer_state(rasterizer_state::cull_none);
				pipeline_state.blend_state = rendering_state->blend_state(blend_state::alpha_to_coverage);
			}
			else if (material.name == "shida_leaf_mtl")
			{
				pipeline_state.rasterizer_state = rendering_state->rasterizer_state(rasterizer_state::cull_none);
				pipeline_state.blend_state = rendering_state->blend_state(blend_state::alpha_to_coverage);
			}
			else if (material.name == "bigtree_mdl__trees_leaf_bottom_mtl")
			{
				pipeline_state.rasterizer_state = rendering_state->rasterizer_state(rasterizer_state::cull_none);
				pipeline_state.blend_state = rendering_state->blend_state(blend_state::alpha_to_coverage);
			}
			else if (material.name == "bigtree_mdl__trees_leaf_top_mtl")
			{
				pipeline_state.rasterizer_state = rendering_state->rasterizer_state(rasterizer_state::cull_none);
				pipeline_state.blend_state = rendering_state->blend_state(blend_state::alpha_to_coverage);
			}
			else if (material.name == "bigtree_mdl__trees_leaf_middle_mtl")
			{
				pipeline_state.rasterizer_state = rendering_state->rasterizer_state(rasterizer_state::cull_none);
				pipeline_state.blend_state = rendering_state->blend_state(blend_state::alpha_to_coverage);
			}

			return 0;
		});
}

void main_scene::draw_ui(ID3D11DeviceContext* immediate_context, float delta_time)
{
	D3D11_VIEWPORT viewport;
	UINT num_viewports = 1;
	immediate_context->RSGetViewports(&num_viewports, &viewport);
	const float aspect_ratio{ viewport.Width / viewport.Height };

	rendering_state->bind_blend_state(immediate_context, blend_state::alpha);
	rendering_state->bind_depth_stencil_state(immediate_context, depth_stencil_state::zt_off_zw_off);
	rendering_state->bind_rasterizer_state(immediate_context, rasterizer_state::cull_none);

	{
		float w = 165, h = w * 0.2035657865041751f;
		logo->blit(immediate_context, 1680 - w - 20, 715 - h - 20, w, h, 1, 1, 1, 0.75f, 0);
	}

	{
		float x = 16;
		float y = 36;
		float w = 24;
		float h = 24;
		for (int i = 0; i < 1; i++)
		{
			heart->blit(immediate_context, x, y, 500, h, 1, 1, 1, 0.75f, 0);
			x += w;
		}
	}

	{
		float x = 16;
		float y = 60;
		float w = 24;
		float h = 24;
		for (int i = 0; i < 1; i++)
		{
			latah_icon->blit(immediate_context, x, y, 360, h, 1, 1, 1, 0.75f, 0);
			x += w;
		}
	}

	if (nico->current_location() == "collision_boss_area_mtl")
	{
		{
			float w = 256;
			float h = 50;
			float x = (viewport.Width - w) / 2;
			float y = 16;
			plantune_name->blit(immediate_context, x, y, w, h, 1, 1, 1, 0.75f, 0);
		}

		{
			float w = 640;
			float h = 4;
			float x = (viewport.Width - w) / 2;
			float y = 68;
			life_bar->blit(immediate_context, x, y, w, h, 1, 1, 1, 0.75f, 0, 0, 0, 4, 4);

			w *= plantune->health_percentage();
			life_bar->blit(immediate_context, x, y, w, h, 1, 1, 1, 0.75f, 0, 4, 0, 4, 4);
		}
	}

	if (nico->heart_point() <= 0)
	{
		float w = 12;
		skull->blit(immediate_context, 0, 0, viewport.Width, viewport.Height, 1, 0.2f, 0.2f, 0.5f, 0);
	}

}

void main_scene::draw_collision_shape(ID3D11DeviceContext* immediate_context, float delta_time)
{
	rendering_state->bind_blend_state(immediate_context, blend_state::alpha);
	rendering_state->bind_rasterizer_state(immediate_context, rasterizer_state::wireframe_cull_none);
	sphere->draw(immediate_context, plantune->core_joint(), plantune_core_sphere_radiuse, { 1, 0, 0, 0.5f });
#if 1
	sphere->draw(immediate_context, nico->root_joint(), nico_root_sphere_radius, { 1, 1, 1, 0.2f });
	sphere->draw(immediate_context, plantune->right_paw_joint(), plantune_right_paw_sphere_radius, { 1, 1, 1, 0.2f });

	cylinder->draw(immediate_context, nico->position(), { nico_breadth * 0.5f, nico_stature, nico_breadth * 0.5f, 1.0f }, { 0, 0, 0, 0 }, { 1, 1, 1, 0.2f });
	cylinder->draw(immediate_context, plantune->position(), { plantune_breadth * 0.5f, plantune_stature, plantune_breadth * 0.5f, 1.0f }, { 0, 0, 0, 0 }, { 1, 1, 1, 0.2f });
#endif
}