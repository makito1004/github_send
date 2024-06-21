#include "monster.h"
using namespace DirectX;

#include <random>
#include <algorithm>

#include "avatar.h"
#include "event.h"

#include <algorithm>

boss::boss(const char* name, ID3D11Device* device) : actor(name)
{
	_state = state::idle;

	_scale = { innate_size, innate_size, innate_size, 1.0f };
	_position = { -15.0f, 0.88f + 1.2f, -10.0f, 1.0f };
	_rotation = { 0, 0, 0, 1 };
	_velocity = { 0, 0, 0, 0 };
	_forward = { 0, 0, 1, 0 };
	_compose_transform();

	_health_point = _max_health_point;

	model = geometric_substance::_emplace(device, ".\\resources\\Slime\\Slime.fbx");
	_audios[0] = audio::_emplace(L".\\resources\\monster.wav");
	_audios[1] = audio::_emplace(L".\\resources\\monster-growl.wav");

	event::_bind("plantune@damaged", [&](const arguments& args) {
		if (_prev_state != state::damaged && _state != state::teleportion)
		{
			const float damage = std::get<float>(args.at(0));
			_state = state::damaged;
			_health_point = std::max(0.0f, _health_point - damage);
		}
		});

	
}
void boss::update(float delta_time)
{
	XMVECTOR X, Y, Z;
	Y = XMVectorSet(0, 1, 0, 0);
	Z = XMVector3Normalize(XMLoadFloat4(&_forward));
	X = XMVector3Cross(Y, Z);
	Y = XMVector3Normalize(XMVector3Cross(Z, X));

	XMVECTOR M = XMLoadFloat4(&_position);
	XMVECTOR B = XMLoadFloat4(&spawn_position);
	XMVECTOR P = XMLoadFloat4(&actor::_at<avatar>("nico")->position());
	XMVECTOR D = P - M;
	float distance_to_nico = XMVectorGetX(XMVector3Length(D));
	D = XMVector3Normalize(D);

	bool facing_outward = XMVectorGetX(XMVector3Dot(Z, B - M)) < 0;

	float remote_distance = XMVectorGetX(XMVector3Length(M - B));

	float linear_speed = 0;
	float turning_speed = 0;

	if (_health_point <= 0)
	{
		_state = state::death;
	}

	if (_state != state::death && _state != state::damaged && _state != state::teleportion)
	{
		//â¬ìÆîªíË
		if (remote_distance > _territory_radius[1] && facing_outward)
		{
			linear_speed = 0;
			_state = state::idle;
		}
		else if (distance_to_nico < _territory_radius[1])
		{
			linear_speed = _max_linear_speed;
			if (_state == state::attack && animation_sequencer.frame() > 30)
			{
				_state = state::attack;
			}
			else
			{
				_state = state::run;
			}
		}
		else
		{
			linear_speed = 0;
			_state = state::idle;
		}

		//çUåÇâ¬î\îªíf
		if (distance_to_nico < _territory_radius[0])
		{
			linear_speed = 0;
			_state = state::attack;
		}

		turning_speed = XMVectorGetX(XMVector3Dot(D, X)) * _max_turning_speed;
	}

	if (_state == state::teleportion)
	{

		_scale.x = std::max(0.0f, _scale.x - innate_size * delta_time);
		_scale.y = std::max(0.0f, _scale.y - innate_size * delta_time);
		_scale.z = std::max(0.0f, _scale.z - innate_size * delta_time);

		_teleportation_time += delta_time;
		if (_teleportation_time > 1.5f)
		{
			_teleportation_time = 0.0f;
			_state = state::idle;

			static std::default_random_engine random_engine;
			std::uniform_real_distribution<float> dx(-_territory_radius[1], +_territory_radius[1]);
			std::uniform_real_distribution<float> dz(-_territory_radius[1], +_territory_radius[1]);

			_position.x = spawn_position.x + dx(random_engine);
			_position.z = spawn_position.z + dz(random_engine);
		}
	}
	else
	{
		_scale = { innate_size, innate_size, innate_size, 1.0f };

		_rotation.x = _rotation.z = 0.0f;
		_rotation.y += XMConvertToRadians(turning_speed) * delta_time;
		XMStoreFloat4(&_forward, XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0, 0, 1, 0), XMMatrixRotationY(_rotation.y))));

		_velocity.x = linear_speed * _forward.x;
		_velocity.z = linear_speed * _forward.z;

		_position.x += _velocity.x * delta_time;
		_position.z += _velocity.z * delta_time;

	}
	_compose_transform();

	_prev_state = _state;
}
void boss::audio_transition(float delta_time)
{
	if (_state == state::attack && _prev_state != state::attack)
	{
		_audios[0]->play();
	}
	else if (_state == state::damaged)
	{
		_audios[1]->play();
	}
}
void boss::render(ID3D11DeviceContext* immediate_context, ID3D11PixelShader* replacement_pixel_shader)
{
	model->render(immediate_context, transform(), keyframe(),
		[&](const geometric_substance::mesh&, const geometric_substance::material&, geometric_substance::shader_resources&, geometric_substance::pipeline_state& pipeline_state) {
			if (replacement_pixel_shader)
			{
				pipeline_state.pixel_shader = replacement_pixel_shader;
			}
			return 0;
		});
}

void boss::animation_transition(float delta_time)
{
	switch (_state)
	{
	case state::idle:
		animation_sequencer.transition(animation_clip::idle, true);
		break;
	case state::run:
		if (animation_sequencer.clip() != animation_clip::run)
		{
			animation_sequencer.transition(animation_clip::run_b);
		}
		break;
	case state::attack:
		animation_sequencer.transition(animation_clip::attack);
		break;
	case state::damaged:
		animation_sequencer.transition(animation_clip::damaged);
		break;
	case state::death:
		animation_sequencer.transition(animation_clip::death);
		break;
	}

	if (animation_sequencer.tictac(model->animation_clips, delta_time))
	{
		switch (animation_sequencer.clip())
		{
		case animation_clip::idle:
			animation_sequencer.transition(animation_clip::idle, true);
			_state = state::idle;
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
			_state = state::teleportion;
			break;
		case animation_clip::damaged:
			animation_sequencer.transition(animation_clip::idle, true);
			_state = state::teleportion;
			break;
		case animation_clip::death:
			animation_sequencer.transition(animation_clip::death);
			break;
		}
	}
}

void boss::collide_with(const collision_mesh* collision_mesh, DirectX::XMFLOAT4X4 transform)
{
	XMFLOAT4 intersection;
	std::string mesh;
	std::string material;

	float raycast_lift_up = 10.0f;
	if (collision_mesh->raycast({ _position.x, _position.y + raycast_lift_up, _position.z, 1 }, { 0, -1, 0, 0 }, transform, intersection, mesh, material))
	{
		if (_position.y < intersection.y)
		{
			_position.x = intersection.x;
			_position.y = intersection.y;
			_position.z = intersection.z;
			_position.w = 1;
			_velocity.y = 0;
		}
	}
}

buddy::buddy(const char* name, ID3D11Device* device, DirectX::XMFLOAT4 initial_position) : actor(name)
{
	model = geometric_substance::_emplace(device, ".\\resources\\latha.fbx");
	_position = initial_position;
	_scale = { 1.5f, 1.5f, 1.5f, 1.0f };
	_state = state::idle;

	event::_bind("latha@detected", [&](const arguments& args) {
		const std::string name = std::get<std::string>(args.at(0));
		if (actor::name == name)
		{
			_state = state::attack;
			_detected = true;
		}
		});
}
void buddy::update(float delta_time)
{
	_compose_transform();
	animation_transition(delta_time);
}
void buddy::render(ID3D11DeviceContext* immediate_context, ID3D11PixelShader* replacement_pixel_shader)
{
	model->render(immediate_context, transform(), keyframe(),
		[&](const geometric_substance::mesh&, const geometric_substance::material&, geometric_substance::shader_resources&, geometric_substance::pipeline_state& pipeline_state) {
			if (replacement_pixel_shader)
			{
				pipeline_state.pixel_shader = replacement_pixel_shader;
			}
			return 0;
		});
}

void buddy::animation_transition(float delta_time)
{
	switch (_state)
	{
	case state::idle:
		animation_sequencer.transition(animation_clip::idle, true);
		break;
	case state::run:
		animation_sequencer.transition(animation_clip::run_b);
		break;
	case state::run_stop:
		animation_sequencer.transition(animation_clip::run_e);
		break;
	case state::attack:
		//animation_sequencer.transition(animation_clip::attack);
		animation_sequencer.transition(animation_clip::attack, true);
		break;
	}

	if (animation_sequencer.tictac(model->animation_clips, delta_time))
	{
		switch (animation_sequencer.clip())
		{
		case animation_clip::idle:
			animation_sequencer.transition(animation_clip::idle, true);
			_state = state::idle;
			break;
		case animation_clip::run_b:
			animation_sequencer.transition(animation_clip::run, true);
			_state = state::run;
			break;
		case animation_clip::run:
			break;
		case animation_clip::run_e:
			animation_sequencer.transition(animation_clip::idle, true);
			_state = state::run;
			break;
		case animation_clip::attack:
			animation_sequencer.transition(animation_clip::idle, true);
			_state = state::attack;
			break;
		}
	}
}
