#pragma once

// UNIT.99
#include <directxmath.h>
#include <functional>

//関数の引数変数の座標系はすべて同じでなければならない。
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
// レイ R(t) = p + t*d を AABB(min, max) と交差させる。
// 光源から交点(q)までの、より近い距離(tmin)を返す。
bool intersect_ray_aabb(const float p[3], const float d[3], const float min[3], const float max[3], float q[3], float& tmin);

// 三角形が交差していればそのインデックスを返し、交差していなければ-1を返す。
int intersect_ray_triangles
(
	const float* positions,
	const uint32_t stride, 
	const uint32_t* indices,
	const size_t index_count,
	const DirectX::XMFLOAT4& ray_position,
	const DirectX::XMFLOAT4& ray_direction, 
	DirectX::XMFLOAT4& intersection,
	float& distance, //[in] 光線の最大距離、[out] 光線源点から交点までの最小距離
	bool RHS = true //右手座標系
);


// フラスタムカリング
struct view_frustum
{
	DirectX::XMFLOAT4 faces[6];
	view_frustum(const DirectX::XMFLOAT4X4& view_projection_matrix)
	{
		// 左平面
		DirectX::XMStoreFloat4(&faces[0], DirectX::XMVector3Normalize(DirectX::XMVectorSet(
			view_projection_matrix._14 + view_projection_matrix._11,
			view_projection_matrix._24 + view_projection_matrix._21,
			view_projection_matrix._34 + view_projection_matrix._31,
			view_projection_matrix._44 + view_projection_matrix._41)));

		// 左平面
		DirectX::XMStoreFloat4(&faces[1], DirectX::XMVector3Normalize(DirectX::XMVectorSet(
			view_projection_matrix._14 - view_projection_matrix._11,
			view_projection_matrix._24 - view_projection_matrix._21,
			view_projection_matrix._34 - view_projection_matrix._31,
			view_projection_matrix._44 - view_projection_matrix._41)));

		//上平面
		DirectX::XMStoreFloat4(&faces[2], DirectX::XMVector3Normalize(DirectX::XMVectorSet(
			view_projection_matrix._14 - view_projection_matrix._12,
			view_projection_matrix._24 - view_projection_matrix._22,
			view_projection_matrix._34 - view_projection_matrix._32,
			view_projection_matrix._44 - view_projection_matrix._42)));

		// 下平面
		DirectX::XMStoreFloat4(&faces[3], DirectX::XMVector3Normalize(DirectX::XMVectorSet(
			view_projection_matrix._14 + view_projection_matrix._12,
			view_projection_matrix._24 + view_projection_matrix._22,
			view_projection_matrix._34 + view_projection_matrix._32,
			view_projection_matrix._44 + view_projection_matrix._42)));

		// 近い平面
		DirectX::XMStoreFloat4(&faces[4], DirectX::XMVector3Normalize(DirectX::XMVectorSet(
			view_projection_matrix._13,
			view_projection_matrix._23,
			view_projection_matrix._33,
			view_projection_matrix._43)));

		//遠い平面
		DirectX::XMStoreFloat4(&faces[5], DirectX::XMVector3Normalize(DirectX::XMVectorSet(
			view_projection_matrix._14 - view_projection_matrix._12,
			view_projection_matrix._24 - view_projection_matrix._22,
			view_projection_matrix._34 - view_projection_matrix._32,
			view_projection_matrix._44 - view_projection_matrix._42)));
	}
};
//オブジェクトAABB(Axis-Aligned Bounding Box)がカメラビュー内にあるかどうかを確認し、
//そうでない場合はGPUに送信しない方法を学びます。
int intersect_frustum_aabb(const view_frustum& view_frustum, const DirectX::XMFLOAT3 bounding_box[2]);
