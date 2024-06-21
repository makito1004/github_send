#include "camera.h"
using namespace DirectX;

void camera::update(float delta_time)
{
	XMFLOAT4 avatar_position = actor::_at(subject_name.c_str())->position();
	_focus = { avatar_position.x, avatar_position.y + _focus_offset_y, avatar_position.z, 1.0f };

	XMVECTOR CP = XMLoadFloat4(&_position);
	XMVECTOR CF = XMLoadFloat4(&_focus);
	XMVECTOR CX, CY, CZ;
	CY = XMVectorSet(0, 1, 0, 0);
	CZ = XMVector3Normalize(CF - CP);
	CX = XMVector3Normalize(XMVector3Cross(CY, CZ));
	CY = XMVector3Normalize(XMVector3Cross(CZ, CX));

	float sensitivity_panorama = 2.0f * delta_time;
	XMMATRIX RY = XMMatrixRotationAxis(XMVectorSet(0, 1, 0, 0), _panorama * sensitivity_panorama);

	const float cosine = cosf(XMConvertToRadians(10.0f));
	float sensitivity_elevation = 1.0f * delta_time;
	XMMATRIX RX = XMMatrixRotationAxis(CX, _elevation * sensitivity_elevation);

	CP = CF + XMVector3Normalize(XMVector3TransformNormal(CP - CF, RX * RY)) *_focal_length;
	
#if 1
	// ŒÀŠE‹ÂŠp
	float elevation_cosine = XMVectorGetX(XMVector3Dot(XMVectorSet(0, 1, 0, 0), XMVector3Normalize(CP - CF)));
	float elevation_agnle_upper_limit = 30; // Angle of rotation of y-axis around x-axis
	float elevation_agnle_lower_limit = 95; // Angle of rotation of y-axis around x-axis
	if (elevation_cosine > XMScalarCos(XMConvertToRadians(elevation_agnle_upper_limit)))
	{
		CP = CF + XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), XMMatrixRotationAxis(CX, -XMConvertToRadians(elevation_agnle_upper_limit))) * _focal_length;
	}
	else if (elevation_cosine < XMScalarCos(XMConvertToRadians(elevation_agnle_lower_limit)))
	{
		CP = CF + XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), XMMatrixRotationAxis(CX, -XMConvertToRadians(elevation_agnle_lower_limit))) * _focal_length;
	}
#endif

	XMStoreFloat4(&_position, CP);

	const float sensitivity_zoom = 5.0f * delta_time;
	_focal_length = std::max<float>(1.5f, std::max<float>(0.0f, _focal_length + _zoom * sensitivity_zoom));
}

void camera::collide_with(const collision_mesh* collision_mesh, DirectX::XMFLOAT4X4 transform)
{
	XMFLOAT4 intersection;
	std::string mesh;
	std::string material;

	if (collision_mesh->raycast(
		{ _position.x, _position.y + 10, _position.z, 1.0f },
		{ 0, -1, 0, 0 },
		transform, intersection, mesh, material))
	{
		_position = intersection;
		// TODO
	}
}