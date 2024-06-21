#pragma once

// UNIT.99
#include <directxmath.h>
#include <functional>

//�֐��̈����ϐ��̍��W�n�͂��ׂē����łȂ���΂Ȃ�Ȃ��B
inline bool intersect_ray_aabb(const float p[3], const float d[3], const float min[3], const float max[3])
{
	float tmin{ 0 };
	float tmax{ +FLT_MAX };

	for (size_t a = 0; a < 3; ++a)
	{
		float inv_d{ 1.0f / d[a] };
		float t0{ (min[a] - p[a]) * inv_d };
		float t1{ (max[a] - p[a]) * inv_d };
		if (inv_d < 0.0f)
		{
			std::swap<float>(t0, t1);
		}
		tmin = t0 > tmin ? t0 : tmin;
		tmax = t1 < tmax ? t1 : tmax;
		if (tmax <= tmin)
		{
			return false;
		}
	}
	return true;
}
// ���C R(t) = p + t*d �� AABB(min, max) �ƌ���������B
// ���������_(q)�܂ł́A���߂�����(tmin)��Ԃ��B
bool intersect_ray_aabb(const float p[3], const float d[3], const float min[3], const float max[3], float q[3], float& tmin);

// �O�p�`���������Ă���΂��̃C���f�b�N�X��Ԃ��A�������Ă��Ȃ����-1��Ԃ��B
int intersect_ray_triangles
(
	const float* positions,
	const uint32_t stride, 
	const uint32_t* indices,
	const size_t index_count,
	const DirectX::XMFLOAT4& ray_position,
	const DirectX::XMFLOAT4& ray_direction, 
	DirectX::XMFLOAT4& intersection,
	float& distance, //[in] �����̍ő勗���A[out] �������_�����_�܂ł̍ŏ�����
	bool RHS = true //�E����W�n
);


// �t���X�^���J�����O
struct view_frustum
{
	DirectX::XMFLOAT4 faces[6];
	view_frustum(const DirectX::XMFLOAT4X4& view_projection_matrix)
	{
		// ������
		DirectX::XMStoreFloat4(&faces[0], DirectX::XMVector3Normalize(DirectX::XMVectorSet(
			view_projection_matrix._14 + view_projection_matrix._11,
			view_projection_matrix._24 + view_projection_matrix._21,
			view_projection_matrix._34 + view_projection_matrix._31,
			view_projection_matrix._44 + view_projection_matrix._41)));

		// ������
		DirectX::XMStoreFloat4(&faces[1], DirectX::XMVector3Normalize(DirectX::XMVectorSet(
			view_projection_matrix._14 - view_projection_matrix._11,
			view_projection_matrix._24 - view_projection_matrix._21,
			view_projection_matrix._34 - view_projection_matrix._31,
			view_projection_matrix._44 - view_projection_matrix._41)));

		//�㕽��
		DirectX::XMStoreFloat4(&faces[2], DirectX::XMVector3Normalize(DirectX::XMVectorSet(
			view_projection_matrix._14 - view_projection_matrix._12,
			view_projection_matrix._24 - view_projection_matrix._22,
			view_projection_matrix._34 - view_projection_matrix._32,
			view_projection_matrix._44 - view_projection_matrix._42)));

		// ������
		DirectX::XMStoreFloat4(&faces[3], DirectX::XMVector3Normalize(DirectX::XMVectorSet(
			view_projection_matrix._14 + view_projection_matrix._12,
			view_projection_matrix._24 + view_projection_matrix._22,
			view_projection_matrix._34 + view_projection_matrix._32,
			view_projection_matrix._44 + view_projection_matrix._42)));

		// �߂�����
		DirectX::XMStoreFloat4(&faces[4], DirectX::XMVector3Normalize(DirectX::XMVectorSet(
			view_projection_matrix._13,
			view_projection_matrix._23,
			view_projection_matrix._33,
			view_projection_matrix._43)));

		//��������
		DirectX::XMStoreFloat4(&faces[5], DirectX::XMVector3Normalize(DirectX::XMVectorSet(
			view_projection_matrix._14 - view_projection_matrix._12,
			view_projection_matrix._24 - view_projection_matrix._22,
			view_projection_matrix._34 - view_projection_matrix._32,
			view_projection_matrix._44 - view_projection_matrix._42)));
	}
};
//�I�u�W�F�N�gAABB(Axis-Aligned Bounding Box)���J�����r���[���ɂ��邩�ǂ������m�F���A
//�����łȂ��ꍇ��GPU�ɑ��M���Ȃ����@���w�т܂��B
int intersect_frustum_aabb(const view_frustum& view_frustum, const DirectX::XMFLOAT3 bounding_box[2]);
