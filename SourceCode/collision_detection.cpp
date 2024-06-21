#include <utility>
#include "collision_detection.h"
using namespace DirectX;

bool intersect_ray_aabb(const float p[3], const float d[3], const float min[3], const float max[3], float q[3], float& tmin)
{
	tmin = 0.0f; //-FLT_MAX�ɐݒ肷��
	float tmax{ FLT_MAX }; // ���C���ړ��ł���ő勗����ݒ�

	//3�̃X���u���ׂĂɂ���
	for (int i = 0; i < 3; i++)
	{
		if (fabsf(d[i]) < FLT_EPSILON)
		{
			// ���C�̓X���u�ɕ��s�B���_���X���u���ɂȂ��ꍇ�̓q�b�g���Ȃ�
			if (p[i] < min[i] || p[i] > max[i])
			{
				return false;
			}
		}
		else
		{
			// �����ƃX���u�̋ߕ��ʂ���щ����ʂƂ̌�_��t�l���v�Z����B
			const float ood{ 1.0f / d[i] };
			float t1{ (min[i] - p[i]) * ood };
			float t2{ (max[i] - p[i]) * ood };
			//t1���ߕ��ʁAt2�������ʂƂ̌�_�Ƃ���
			if (t1 > t2)
			{
				std::swap<float>(t1, t2);
			}
			// �X���u�̌�����Ԃ̌v�Z
			tmin = std::max<float>(tmin, t1);
			tmax = std::min<float>(tmax, t2);
			// �X���u�����_����ɂȂ莟��A�Փ˂�����return
			if (tmin > tmax)
			{
				return false;
			}
		}
	}
	// ���C��3�̃X���u�S�Ăƌ�������B�߂�_(q)�ƌ����_��t�l(tmin) 
	for (int i = 0; i < 3; i++)
	{
		q[i] = p[i] + d[i] * tmin;
	}
	return true;
}

//#define ���m�Ȍv�Z
int intersect_ray_triangles
(
	const float* positions,
	const uint32_t stride,
	const uint32_t* indices,
	const size_t index_count,
	const XMFLOAT4& ray_position,
	const XMFLOAT4& ray_direction,
	XMFLOAT4& intersection,
	float& distance,
	const bool RHS
)
{
	const bool CCW{ RHS }; // 3�����O�p�` ABC
	const int C0{ 0 };
	const int C1{ CCW ? 1 : 2 };
	const int C2{ CCW ? 2 : 1 };

	//  �����ƎO�p�`�̌����_
	// �����ƎO�p�`�̌�_���v�Z���邽�߂ɂ܂��A���̂悤�Ȍ�_�̌`����l���܂��F
	
	//���_P�ƕ���D���������́A���̒��_A�AB�AC�Œ�`�����O�p�`�ƌ�_//�_Q�Ō�������B
	// �O�p�`ABC��}���I�Ɉ͂ސ����`�̗̈�́A�O�p�`�̎x�����ʂ�\��
	//�O�p�`ABC��}���I�Ɉ͂ގl�p���̈�́A�O�p�`�̎x�����ʁA���Ȃ킿�O�p�`��������镽�ʂ�\��.
	int intersection_count{ 0 };
	int intersected_triangle_index{ -1 };

	const float ray_length_limit{ distance };
	float closest_distance{ FLT_MAX };

	XMVECTOR X{}; // closest cross point
	const XMVECTOR P{ XMVectorSet(ray_position.x, ray_position.y, ray_position.z, 1) };
	const XMVECTOR D{ XMVector3Normalize(XMVectorSet(ray_direction.x, ray_direction.y, ray_direction.z, 0)) };

	using byte = uint8_t;
	const byte* p{ reinterpret_cast<const byte*>(positions) };
	const size_t triangle_count{ index_count / 3 };
	for (size_t triangle_index = 0; triangle_index < triangle_count; triangle_index++)
	{
		const XMVECTOR A{ XMVectorSet(
			(reinterpret_cast<const float*>(p + indices[triangle_index * 3 + C0] * stride))[0],
			(reinterpret_cast<const float*>(p + indices[triangle_index * 3 + C0] * stride))[1],
			(reinterpret_cast<const float*>(p + indices[triangle_index * 3 + C0] * stride))[2],
			1.0f
		) };
		const XMVECTOR B{ XMVectorSet(
			(reinterpret_cast<const float*>(p + indices[triangle_index * 3 + C1] * stride))[0],
			(reinterpret_cast<const float*>(p + indices[triangle_index * 3 + C1] * stride))[1],
			(reinterpret_cast<const float*>(p + indices[triangle_index * 3 + C1] * stride))[2],
			1.0f
		) };
		const XMVECTOR C{ XMVectorSet(
			(reinterpret_cast<const float*>(p + indices[triangle_index * 3 + C2] * stride))[0],
			(reinterpret_cast<const float*>(p + indices[triangle_index * 3 + C2] * stride))[1],
			(reinterpret_cast<const float*>(p + indices[triangle_index * 3 + C2] * stride))[2],
			1.0f
		) };

		XMVECTOR Q;//�����ƎO�p�`�̌�_

		// �O�p�`ABC�̕��ʂ̕����������߂�.
		const XMVECTOR N{ XMVector3Normalize(XMVector3Cross(XMVectorSubtract(B, A), XMVectorSubtract(C, A))) };
		const float d{ XMVectorGetByIndex(XMVector3Dot(N, A), 0) };

		// �����ƕ��ʂ�����������.
		const float denominator{ XMVectorGetByIndex(XMVector3Dot(N, D), 0) };
		if (denominator < 0) //N.D=0�̏ꍇ�AD�͕��ʂɕ��s�ł���A�����͕��ʂƌ������Ȃ�
		{
			const float numerator{ d - XMVectorGetByIndex(XMVector3Dot(N, P), 0) };
			const float t{ numerator / denominator };

			if (t > 0 && t < ray_length_limit) //  ���C�̑O�i�ƒ����̌��E
			{
				Q = XMVectorAdd(P, XMVectorScale(D, t));

				// �O�p�`�̓����ƊO���̌v�Z

				//���A���^�C���Փˌ��o
				// �_Q�������v���̎O�p�`ABC�̓����ɂ��邩�ǂ������e�X�g����

				// �_�����_�ɗ���悤�ɓ_�ƎO�p�`�𕽍s�ړ�������B

				const XMVECTOR QA{ XMVectorSubtract(A, Q) };
				const XMVECTOR QB{ XMVectorSubtract(B, Q) };
				const XMVECTOR QC{ XMVectorSubtract(C, Q) };

				XMVECTOR U{ XMVector3Cross(QB, QC) };
				XMVECTOR V{ XMVector3Cross(QC, QA) };
				if (XMVectorGetByIndex(XMVector3Dot(U, V), 0) < 0)
				{
					continue;
				}

				XMVECTOR W{ XMVector3Cross(QA, QB) };
				if (XMVectorGetByIndex(XMVector3Dot(U, W), 0) < 0)
				{
					continue;
				}
				if (XMVectorGetByIndex(XMVector3Dot(V, W), 0) < 0)
				{
					continue;
				}

				// Q�͎O�p�`�̒��܂��͎O�p�`�ɂ��Ȃ���΂Ȃ�Ȃ��B
				if (t < closest_distance)
				{
					closest_distance = t;
					intersected_triangle_index = static_cast<int>(triangle_index);
					X = Q;
				}
				intersection_count++;
			}
		}
	}
	if (intersection_count > 0)
	{
		XMStoreFloat4(&intersection, X);
		distance = closest_distance;
	}
	return intersected_triangle_index;
}

int intersect_frustum_aabb(const view_frustum& view_frustum, const DirectX::XMFLOAT3 bounding_box[2])
{
	int cull{ false };
	for (int plane_index = 0; plane_index < 6; ++plane_index)
	{
		DirectX::XMVECTOR n{ DirectX::XMVectorSet(view_frustum.faces[plane_index].x, view_frustum.faces[plane_index].y, view_frustum.faces[plane_index].z, 0.0f) };
		float d{ view_frustum.faces[plane_index].w };

		//�e���ix,y,z�j���`�F�b�N���A���ʂ̌����Ă�������i���ʖ@���j����ł�����AABB���_���擾����B
		DirectX::XMFLOAT3 axis{
			view_frustum.faces[plane_index].x < 0.0f ? bounding_box[0].x : bounding_box[1].x, // Which AABB vertex is furthest down (plane normals direction) the x axis
			view_frustum.faces[plane_index].y < 0.0f ? bounding_box[0].y : bounding_box[1].y, // Which AABB vertex is furthest down (plane normals direction) the y axis
			view_frustum.faces[plane_index].z < 0.0f ? bounding_box[0].z : bounding_box[1].z  // Which AABB vertex is furthest down (plane normals direction) the z axis
		};

		// �����ŁA�t���X�g�����v���[���̖@������ł����ɂ���AABB���_����̕����t�����������߂�A
		//�������������ł���΁A�{�b�N�X�S�̂��t���X�g�����v���[���̌��ɂ���
		if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(n, DirectX::XMLoadFloat3(&axis))) + d < 0.0f)
		{
			cull = true;
			// �c��̃v���[�����΂��ă`�F�b�N���A���̃c���[�ɐi��
			break;
		}
	}
	return cull;
}