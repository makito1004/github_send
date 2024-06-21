#pragma once

#include <d3d11.h>

#include <memory>
#include <directxmath.h>

#include "actor.h"
#include "geometric_substance.h"
#include "collision_mesh.h"
#include "audio.h"

class boss : public actor
{
public:
	boss(const char* name, ID3D11Device* device);
	~boss() = default;
	void update(float delta_time);
	void render(ID3D11DeviceContext* immediate_context, ID3D11PixelShader* replacement_pixel_shader = NULL);
	void cast_shadow(ID3D11DeviceContext* immediate_context)
	{
		model->cast_shadow(immediate_context, transform(), keyframe());
	}

	void animation_transition(float elapsed_time);
	void audio_transition(float delta_time);
	void collide_with(const collision_mesh* collision_mesh, DirectX::XMFLOAT4X4 transform);

	enum class state { idle, run, attack, damaged, death, teleportion };
	state tell_state() const { return _state; }
	enum class animation_clip { idle, run_b, run, run_e, attack, damaged, death };
	animation_sequencer<animation_clip> animation_sequencer = { animation_clip::idle };

	const animation::keyframe* keyframe() const
	{
		return animation_sequencer.keyframe(model->animation_clips);
	}
	const DirectX::XMFLOAT4& forward() const { return _forward; }
	const DirectX::XMFLOAT4& velocity() const { return _velocity; };

	DirectX::XMFLOAT4 right_paw_joint() const
	{
		return model->joint("Slime_1", "Spine01", transform(), keyframe());
	}
	DirectX::XMFLOAT4 core_joint() const
	{
		return model->joint("Slime_1", "Spine01", transform(), keyframe());
	}
	float health_point() const { return _health_point; }
	float health_percentage() const { return _health_point / _max_health_point; }

private:
	std::shared_ptr<geometric_substance> model;

	DirectX::XMFLOAT4 _forward = { 0, 0, 1, 0 };
	DirectX::XMFLOAT4 _velocity = { 0, 0, 0, 0 };

	state _state = state::idle;
	state _prev_state = state::idle;

	float _health_point = _max_health_point;
	float _teleportation_time = 0;


	float _max_linear_speed = 4;
	float _max_turning_speed = 180; // degree per second
	float _max_health_point = 18;
	float _territory_radius[2] = { 8.0f, 21.40f };
	float _attack_range = 3.0f;

	const float innate_size = 2.5f;
	const DirectX::XMFLOAT4 spawn_position = { -15.0f, 0.88f + 0.5f, 0.0f, 1.0f };

	std::shared_ptr<audio> _audios[8];
};

class buddy : public actor
{
	std::shared_ptr<geometric_substance> model;

public:
	buddy(const char* name, ID3D11Device* device, DirectX::XMFLOAT4 initial_position);
	~buddy() = default;
	void update(float delta_time);
	void render(ID3D11DeviceContext* immediate_context, ID3D11PixelShader* replacement_pixel_shader = NULL);
	void cast_shadow(ID3D11DeviceContext* immediate_context)
	{
		model->cast_shadow(immediate_context, transform(), keyframe());
	}
	void animation_transition(float elapsed_time);

	enum class state { idle, run, run_stop, attack };
	enum class state state() const { return _state; }
	enum animation_clip { idle, run_b, run, run_e, attack };
	animation_sequencer<animation_clip> animation_sequencer = { animation_clip::idle };
	const animation::keyframe* keyframe() const
	{
		return animation_sequencer.keyframe(model->animation_clips);
	}

	bool detected() const { return _detected; }

private:
	bool _detected = false;
	enum class state _state = state::idle;

};