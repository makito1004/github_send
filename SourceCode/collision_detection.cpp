#include <utility>
#include "collision_detection.h"
using namespace DirectX;

bool intersect_ray_aabb(const float p[3], const float d[3], const float min[3], const float max[3], float q[3], float& tmin)
{
	tmin = 0.0f; //-FLT_MAXに設定する
	float tmax{ FLT_MAX }; // レイが移動できる最大距離を設定

	//3つのスラブすべてについて
	for (int i = 0; i < 3; i++)
	{
		if (fabsf(d[i]) < FLT_EPSILON)
		{
			// レイはスラブに平行。原点がスラブ内にない場合はヒットしない
			if (p[i] < min[i] || p[i] > max[i])
			{
				return false;
			}
		}
		else
		{
			// 光線とスラブの近平面および遠平面との交点のt値を計算する。
			const float ood{ 1.0f / d[i] };
			float t1{ (min[i] - p[i]) * ood };
			float t2{ (max[i] - p[i]) * ood };
			//t1を近平面、t2を遠平面との交点とする
			if (t1 > t2)
			{
				std::swap<float>(t1, t2);
			}
			// スラブの交差区間の計算
			tmin = std::max<float>(tmin, t1);
			tmax = std::min<float>(tmax, t2);
			// スラブ交差点が空になり次第、衝突せずにreturn
			if (tmin > tmax)
			{
				return false;
			}
		}
	}
	// レイは3つのスラブ全てと交差する。戻り点(q)と交差点のt値(tmin) 
	for (int i = 0; i < 3; i++)
	{
		q[i] = p[i] + d[i] * tmin;
	}
	return true;
}

//#define 正確な計算
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
	const bool CCW{ RHS }; // 3次元三角形 ABC
	const int C0{ 0 };
	const int C1{ CCW ? 1 : 2 };
	const int C2{ CCW ? 2 : 1 };

	//  光線と三角形の交差点
	// 光線と三角形の交点を計算するためにまず、このような交点の形状を考えます：
	
	//原点Pと方向Dを持つ光線は、その頂点A、B、Cで定義される三角形と交点//点Qで交差する。
	// 三角形ABCを図式的に囲む正方形の領域は、三角形の支持平面を表す
	//三角形ABCを図式的に囲む四角い領域は、三角形の支持平面、すなわち三角形が横たわる平面を表す.
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

		XMVECTOR Q;//光線と三角形の交点

		// 三角形ABCの平面の方程式を求める.
		const XMVECTOR N{ XMVector3Normalize(XMVector3Cross(XMVectorSubtract(B, A), XMVectorSubtract(C, A))) };
		const float d{ XMVectorGetByIndex(XMVector3Dot(N, A), 0) };

		// 光線と平面を交差させる.
		const float denominator{ XMVectorGetByIndex(XMVector3Dot(N, D), 0) };
		if (denominator < 0) //N.D=0の場合、Dは平面に平行であり、光線は平面と交差しない
		{
			const float numerator{ d - XMVectorGetByIndex(XMVector3Dot(N, P), 0) };
			const float t{ numerator / denominator };

			if (t > 0 && t < ray_length_limit) //  レイの前進と長さの限界
			{
				Q = XMVectorAdd(P, XMVectorScale(D, t));

				// 三角形の内側と外側の計算

				//リアルタイム衝突検出
				// 点Qが反時計回りの三角形ABCの内側にあるかどうかをテストする

				// 点が原点に来るように点と三角形を平行移動させる。

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

				// Qは三角形の中または三角形にいなければならない。
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

		//各軸（x,y,z）をチェックし、平面の向いている方向（平面法線）から最も遠いAABB頂点を取得する。
		DirectX::XMFLOAT3 axis{
			view_frustum.faces[plane_index].x < 0.0f ? bounding_box[0].x : bounding_box[1].x, // Which AABB vertex is furthest down (plane normals direction) the x axis
			view_frustum.faces[plane_index].y < 0.0f ? bounding_box[0].y : bounding_box[1].y, // Which AABB vertex is furthest down (plane normals direction) the y axis
			view_frustum.faces[plane_index].z < 0.0f ? bounding_box[0].z : bounding_box[1].z  // Which AABB vertex is furthest down (plane normals direction) the z axis
		};

		// ここで、フラストラムプレーンの法線から最も下にあるAABB頂点からの符号付き距離を求める、
		//符号距離が負であれば、ボックス全体がフラストラムプレーンの後ろにある
		if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(n, DirectX::XMLoadFloat3(&axis))) + d < 0.0f)
		{
			cull = true;
			// 残りのプレーンを飛ばしてチェックし、次のツリーに進む
			break;
		}
	}
	return cull;
}