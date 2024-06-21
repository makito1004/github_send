#pragma once

#include <directxmath.h>
#include <string>

#include "actor.h"
#include "event.h"
#include "collision_mesh.h"

class camera : public actor
{
public:
	camera(const char* name, const char* subject_name, const DirectX::XMFLOAT4& focus, const DirectX::XMFLOAT4& direction, float focal_length, float focus_offset_y) : actor(name), subject_name(subject_name)
	{
		respawn(focus, direction, focal_length, focus_offset_y);
		event::_bind("@trigger_state", [&](const arguments& args) {
			_zoom = std::get<float>(args.at(0)) - std::get<float>(args.at(1));
			});

		event::_bind("@thumb_state_r", [&](const arguments& args) {
			_panorama = std::get<float>(args.at(0));
			_elevation = std::get<float>(args.at(1));
			});
	}

	const DirectX::XMFLOAT4& focus() const { return _focus; };

	void collide_with(const collision_mesh* collision_mesh, DirectX::XMFLOAT4X4 transform);
	void respawn(const DirectX::XMFLOAT4& focus, const DirectX::XMFLOAT4& direction, float focal_length, float focus_offset_y)
	{
		_focus = focus;
		_focus.y += _focus_offset_y = focus_offset_y;
		_focal_length = focal_length;

#if 1
		_position = { focus.x - direction.x * focal_length, focus.y - direction.y * focal_length + focus_offset_y, focus.z - direction.z * focal_length, 1.0f };
#else
		using namespace DirectX;
		XMVECTOR CF = XMLoadFloat4(&_focus);
		XMVECTOR CX, CY, CZ;
		CY = XMVectorSet(0, 1, 0, 0);
		CZ = XMVector3Normalize(XMLoadFloat4(&direction));
		CX = XMVector3Normalize(XMVector3Cross(CY, CZ));
		CY = XMVector3Normalize(XMVector3Cross(CZ, CX));

		XMStoreFloat4(&_position, CF + XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0) * _focal_length, XMMatrixRotationAxis(CX, -XMConvertToRadians(95))));
#endif
	}
	void update(float delta_time);

	DirectX::XMFLOAT4X4 perspective_projection_matrix(float aspect_ratio) const
	{
		DirectX::XMFLOAT4X4 perspective_projection_matrix;
		DirectX::XMStoreFloat4x4(&perspective_projection_matrix, DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(_field_of_view), aspect_ratio, _near_z, _far_z));
		return perspective_projection_matrix;
	}
	DirectX::XMFLOAT4X4 view_matrix() const
	{
		DirectX::XMFLOAT4X4 view_matrix;
		DirectX::XMStoreFloat4x4(&view_matrix, DirectX::XMMatrixLookAtLH(
			DirectX::XMVectorSet(_position.x, _position.y, _position.z, 1.0f),
			DirectX::XMVectorSet(_focus.x, _focus.y, _focus.z, 1.0f),
			DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)));
		return view_matrix;
	}

	DirectX::XMFLOAT4 _focus;
	float _focal_length = 5;
	float _focus_offset_y = 1.2f;
	float _field_of_view = 60.0f;
	float _near_z = 1.0f;
	float _far_z = 1000.0f;

private:
	std::string subject_name;


	float _zoom = 0;
	float _panorama = 0;
	float _elevation = 0;
};
