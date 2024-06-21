#pragma once
#include <d3d11.h>
#include <directxmath.h>
#include <deque>

#include <sstream>

#include "actor.h"
#include "audio.h"

#include "geometric_substance.h"
#include "collision_mesh.h"

class avatar : public actor
{
public:
	avatar(const char* name, ID3D11Device* device, const DirectX::XMFLOAT4& initial_position);
	~avatar() = default;
	void update(float delta_time);
	void render(ID3D11DeviceContext* immediate_context, ID3D11PixelShader* replacement_pixel_shader = NULL);
	void cast_shadow(ID3D11DeviceContext* immediate_context)
	{
		model->cast_shadow(immediate_context, transform(), keyframe());
	}
	void animation_transition(float delta_time);
	void audio_transition(float delta_time);
	void collide_with(const collision_mesh* collision_mesh, DirectX::XMFLOAT4X4 transform);

	const DirectX::XMFLOAT4& forward() const { return _forward; }
	void respawn(DirectX::XMFLOAT4 location)
	{
		_state = state::jump_fall;
		_scale = { 1.0f, 1.0f, 1.0f, 1.0f };
		_position = location;
		_velocity = { 0.0f, 0.0f, 0.0f, 0.0f };
		_rotation = { 0, DirectX::XMConvertToRadians(180.0f), 0, 1 };
		_forward = { 0, 0, -1, 0 };
		_heart_point = _max_heart_point;
		_compose_transform();
	}
	
	DirectX::XMFLOAT4 root_joint() const
	{
		return model->joint("NIC:full_body", "NIC:Root_M_BK", transform(), keyframe());
	}

	DirectX::XMFLOAT4 magic_wand_sphere_joint() const
	{
		return model->joint("NIC:magic_wand", "NIC:wand2_BK", transform(), keyframe());
	}
	enum class state { idle, run, run_stop, attack, jump_rise, jump_fall, landing, damaged, magic, death, respawn };
	state tell_state() const { return _state; }

	std::shared_ptr<geometric_substance> model;

	const animation::keyframe* keyframe() const
	{
		return animation_sequencer.keyframe(model->animation_clips);
	}


	enum class animation_clip { idle, run, run_b, run_e, jump_fall, landing, jump_rise, attack, death };
	animation_sequencer<animation_clip> animation_sequencer = { animation_clip::idle };

	int heart_point() const { return _heart_point; }
	bool invincible() const { return _invincible_time > 0; }
	std::string current_location() const { return _current_location; }

	const DirectX::XMFLOAT4& velocity() const { return _velocity; };

	bool enable_infinite_jump = false;

private:
	state _state = state::jump_fall;
	state _prev_state = state::idle;

	DirectX::XMFLOAT4 _forward = { 0, 0, -1, 0 };
	DirectX::XMFLOAT4 _velocity = { 0, 0, 0, 0 };

	float _max_linear_speed = 10;
	float _max_turning_speed = 360; 
	float _initial_jump_speed = 7.5f;
	int _max_heart_point = 10;

	float _linear_speed = 0;
	float _turning_speed = 0; 
	
	int _heart_point = _max_heart_point;
	
	float _invincible_time = 0;
	std::string _current_location;
	std::shared_ptr<audio> _audios[8];

public:
	const std::string& current_location() { return _current_location; }
};
