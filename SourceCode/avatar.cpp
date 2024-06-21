#include <string>

#include "avatar.h"
#include "monster.h"
#include "shader.h"

#include "event.h"
#include "camera.h"

#include <string.h>

using namespace DirectX;
class main_scene;

avatar::avatar(const char* name, ID3D11Device* device, const DirectX::XMFLOAT4& initial_position) : actor(name)
{

	respawn(initial_position);
	model = geometric_substance::_emplace(device, ".\\resources\\nico.fbx");
	event::_bind("@thumb_state_l", [&](const arguments& args) {
		using namespace DirectX;

		if (_state == state::damaged || _state == state::death)
		{
			return;
		}

		const float thumb_state_lx = std::get<float>(args.at(0));
		const float thumb_state_ly = std::get<float>(args.at(1));
		if (thumb_state_lx == 0 && thumb_state_ly == 0)
		{
			if (_state == state::run)
			{
				_state = state::run_stop;
				_linear_speed = 0;
			}
			return;
		}

		XMVECTOR PX, PY, PZ;
		PY = XMVectorSet(0, 1, 0, 0);
		PZ = XMVector3Normalize(XMLoadFloat4(&_forward));
		PX = XMVector3Normalize(XMVector3Cross(PY, PZ));

		XMFLOAT4 camera_position = actor::_at("eye_view_camera")->position();
		XMFLOAT4 camera_focus = actor::_at<camera>("eye_view_camera")->focus();
		XMVECTOR CP = XMLoadFloat4(&camera_position);
		XMVECTOR CF = XMLoadFloat4(&camera_focus);
		XMVECTOR CX, CY, CZ;
		CY = XMVectorSet(0, 1, 0, 0);
		CZ = XMVector3Normalize(CF - CP);
		CX = XMVector3Normalize(XMVector3Cross(CY, CZ));
		CY = XMVector3Normalize(XMVector3Cross(CZ, CX));

		const float steering_play = 0.0001f;
		XMVECTOR D = XMVector3Normalize(thumb_state_lx * CX + thumb_state_ly * CZ);

		float linear_speed_factor = XMVectorGetX(XMVector3Dot(D, PZ));
		float turning_speed_factor = XMVectorGetX(XMVector3Dot(D, PX));

		if (fabsf(linear_speed_factor) > steering_play)
		{
			switch (_state)
			{
			case state::idle:
			case state::run:
			case state::landing:
				_state = state::run;
				
				break;
			case state::damaged:
				if (_invincible_time == 0)
				{
					_state = state::run;
				}
				else
				{
					linear_speed_factor = 0;
					turning_speed_factor = 0;
				}
				break;
			case state::jump_rise:
			case state::jump_fall:
				linear_speed_factor = linear_speed_factor < 0 ? 0 : linear_speed_factor;
				break;
			case state::run_stop:
			case state::attack:
			case state::respawn:
			case state::death:
			default:
				linear_speed_factor = 0;
				turning_speed_factor = 0;
				break;
			}
		}
		
		_linear_speed = _max_linear_speed * linear_speed_factor;
		if (fabsf(turning_speed_factor) > steering_play)
		{
			_turning_speed += _max_turning_speed * turning_speed_factor;
		}

		});

	event::_bind("@button", [&](const arguments& args) {

		if (_state == state::damaged || _state == state::death)
		{
			return;
		}

		const char keybutton = std::get<std::string>(args.at(0)).at(0);
		switch (keybutton)
		{
		case 'a':
			if (enable_infinite_jump || _state != state::jump_rise && _state != state::jump_fall)
			{
				_state = state::jump_rise;
				_velocity.y = _initial_jump_speed;
			}
			break;
		case 'b':
			_state = state::attack;
			break;
		case 'x':
			break;
		case 'y':
			break;
		}
		});

	
	event::_bind("nico@damaged", [&](const arguments& args) {
		if (_state == state::damaged || _state == state::death)
		{
			return;
		}
		const int damage = static_cast<int>(std::get<float>(args.at(0)));
		if (_invincible_time == 0)
		{
			_heart_point = std::max(0, _heart_point - damage);
			_state = state::damaged;
			_invincible_time = 3.0f;
		}
		});
	event::_bind("nico@collided_with_plantune", [&](const arguments& args) {
		const float penetration = std::get<float>(args.at(0));
		XMFLOAT3 v;
		XMStoreFloat3(&v, XMVector3Normalize(XMLoadFloat4(&_velocity)));
		const float extra_space = 0.01f;
		_position.x += v.x * (penetration - extra_space);
		_position.z += v.z * (penetration - extra_space);
		});

	_audios[0] = audio::_emplace(L".\\resources\\footsteps-of-a-runner-on-gravel.wav");
	_audios[1] = audio::_emplace(L".\\resources\\footsteps-dry-leaves-g.wav");
	_audios[2] = audio::_emplace(L".\\resources\\swinging-staff-whoosh-low-04.wav");


}

void avatar::update(float delta_time)
{

	if (_invincible_time > 0)
	{
		_invincible_time = std::max<float>(0, _invincible_time - delta_time);
	}

	if (_state == state::death || _state == state::damaged)
	{
		if (_invincible_time == 0)
		{
			if (_heart_point == 0)
			{
				_state = state::respawn;
			}
			else
			{
				_state = state::idle;
			}
		}
	}
	if (_state == state::jump_rise && _prev_state == state::jump_rise && _velocity.y < 0)
	{
		_state = state::jump_fall;
	}

	_rotation.x = _rotation.z = 0.0f;
	_rotation.y += XMConvertToRadians(_turning_speed) * delta_time;
	XMStoreFloat4(&_forward, XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0, 0, 1, 0), XMMatrixRotationY(_rotation.y))));
	_velocity.x = _linear_speed * _forward.x;
	_velocity.z = _linear_speed * _forward.z;
	_velocity.y -= 9.8f * delta_time;

	_position.x += _velocity.x * delta_time;
	_position.y += _velocity.y * delta_time;
	_position.z += _velocity.z * delta_time;

	_compose_transform();

	_turning_speed = 0;
	_linear_speed = 0;
	_prev_state = _state;
}

void avatar::audio_transition(float delta_time)
{
	if (_state == state::run)
	{
		if (_current_location == "collision_road_mtl")
		{
			_audios[0]->play();
			_audios[0]->volume(0.2f);

			_audios[1]->stop();
		}
		else
		{
			_audios[1]->play();
			_audios[1]->volume(0.5f);

			_audios[0]->stop();
		}
	}
	else
	{
		_audios[0]->stop();
		_audios[1]->stop();
	}

	if (_state == state::attack/* && _prev_state != state::attack*/)
	{
		_audios[2]->play();
		_audios[1]->volume(0.8f);
	}
}

void avatar::animation_transition(float delta_time)
{
	switch (_state)
	{
	case state::idle:
		animation_sequencer.transition(animation_clip::idle, true);
		break;
	case state::run:
		if (animation_sequencer.clip() != animation_clip::attack && animation_sequencer.clip() != animation_clip::run)
		{
			animation_sequencer.transition(animation_clip::run_b);
		}
		else if (animation_sequencer.clip() == animation_clip::jump_fall)
		{
			animation_sequencer.transition(animation_clip::run_b);
		}

		break;
	case state::run_stop:
		animation_sequencer.transition(animation_clip::run_e);
		break;
	case state::attack:
		animation_sequencer.transition(animation_clip::attack);
		break;
	case state::jump_rise:
		animation_sequencer.transition(animation_clip::jump_rise);
		break;
	case state::jump_fall:
		animation_sequencer.transition(animation_clip::jump_fall);
		break;
	case state::landing:
		animation_sequencer.transition(animation_clip::landing);
		break;
	case state::death:
		animation_sequencer.transition(animation_clip::death);
		break;
	case state::damaged:
		animation_sequencer.transition(animation_clip::death);
		break;
	}

	if (animation_sequencer.tictac(model->animation_clips, delta_time))
	{
		// アニメーションの再生終了時の処理
		switch (animation_sequencer.clip())
		{
		case animation_clip::idle:
#if 0
			animation_sequencer.transition(animation_clip::idle, true);
			_state = state::idle;
#endif
			break;
		case animation_clip::run_b:
			animation_sequencer.transition(animation_clip::run, true);
			_state = state::run;
			break;
		case animation_clip::run:
			break;
		case animation_clip::run_e:
			animation_sequencer.transition(animation_clip::idle, true);
			_state = state::idle;
			break;
		case animation_clip::attack:
			animation_sequencer.transition(animation_clip::idle, true);
			_state = state::idle;
			break;
		case animation_clip::jump_rise:
			break;
		case animation_clip::jump_fall:
			animation_sequencer.transition(animation_clip::landing);
			_state = state::landing;
			break;
		case animation_clip::landing:
			animation_sequencer.transition(animation_clip::idle, true);
			_state = state::idle;
			break;
		case animation_clip::death:
			animation_sequencer.transition(animation_clip::death);
			break;
		}
	}
}

void avatar::render(ID3D11DeviceContext* immediate_context, ID3D11PixelShader* replacement_pixel_shader)
{
	model->render(immediate_context, transform(), keyframe(),
		[&](const geometric_substance::mesh&, const geometric_substance::material& material, geometric_substance::shader_resources& shader_resources, geometric_substance::pipeline_state& pipeline_state) {
			if (replacement_pixel_shader)
			{
				pipeline_state.pixel_shader = replacement_pixel_shader;
			}
			if (material.name == "Solus_Knight_Base_Color.png.001")
			{
				shader_resources.material_data.emissive.w = 50.0f;
			}
			return 0;
		});
}

void avatar::collide_with(const collision_mesh* collision_mesh, DirectX::XMFLOAT4X4 transform)
{
	XMFLOAT4 intersection;
	std::string mesh;
	std::string material;
	
	const float raycast_step_back = 1.0f;
	const float raycast_step_up = 1.5f;
	const float collision_radius = 0.5f;

	XMFLOAT4 ray_direction = { _forward.x, 0.0f, _forward.z, 0.0f };
	XMFLOAT4 ray_position = {
		_position.x - raycast_step_back * ray_direction.x,
		_position.y + raycast_step_up,
		_position.z - raycast_step_back * ray_direction.z,
		1.0f
	};

	if ((_velocity.x * _velocity.x + _velocity.z * _velocity.z > 0.0f) && collision_mesh->raycast(ray_position, ray_direction, transform, intersection, mesh, material, false))
	{
		float distance = sqrtf((_position.x - intersection.x) * (_position.x - intersection.x) + (_position.z - intersection.z) * (_position.z - intersection.z));
		if (distance < collision_radius)
		{
			_position.x -= (collision_radius - distance) * ray_direction.x;
			_position.z -= (collision_radius - distance) * ray_direction.z;

			_velocity.x = 0;
			_velocity.z = 0;
			_state = state::idle;

		}
	}

	const float raycast_lift_up = 1.75f;
	if (collision_mesh->raycast({ _position.x, _position.y + raycast_lift_up, _position.z, 1 }, { 0, -1, 0, 0 }, transform, intersection, mesh, material))
	{
		_current_location = material;

		//深度が0より大きいか 
		if (intersection.y - _position.y > 0)
		{

			_position.x = intersection.x;
			_position.y = intersection.y;
			_position.z = intersection.z;
			_position.w = 1;

			if (_velocity.y < 0)
			{
				_velocity.y = 0;
			}
		}
	}

}

