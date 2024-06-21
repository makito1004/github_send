#include "collision_detection.h"
#include "collision_mesh.h"

using namespace DirectX;

bool collision_mesh::raycast(const XMFLOAT4& position, const XMFLOAT4& direction, const XMFLOAT4X4& world_transform, _Out_ XMFLOAT4& closest_point,
	_Out_ std::string& intersected_mesh, _Out_ std::string& intersected_material, bool skip_if) const
{
	float closest_distance{ FLT_MAX };

	for (const mesh& mesh : meshes)
	{
		XMFLOAT4 ray_position = position;
		XMFLOAT4 ray_direction = direction;

		XMMATRIX concatenated_matrix{
			XMLoadFloat4x4(&mesh.geometric_transform) *
			XMLoadFloat4x4(&mesh.default_global_transform) *
			XMLoadFloat4x4(&world_transform) };
		XMMATRIX inverse_concatenated_matrix{ XMMatrixInverse(nullptr, concatenated_matrix) };
		XMStoreFloat4(&ray_position, XMVector3TransformCoord(XMLoadFloat4(&ray_position), inverse_concatenated_matrix));
		XMStoreFloat4(&ray_direction, XMVector3TransformNormal(XMLoadFloat4(&ray_direction), inverse_concatenated_matrix));

#if 1
		const float* min{ reinterpret_cast<const float*>(&mesh.bounding_box[0]) };
		const float* max{ reinterpret_cast<const float*>(&mesh.bounding_box[1]) };
		if (!intersect_ray_aabb(reinterpret_cast<const float*>(&ray_position), reinterpret_cast<const float*>(&ray_direction), min, max))
		{
			continue;
		}
#endif

		float distance{ 1.0e+7f };
		XMFLOAT4 intersection{};
		const float* vertex_positions{ reinterpret_cast<const float*>( mesh.vertex_positions.data()) };
		const uint32_t* indices{ mesh.indices.data() };
		const size_t index_count{ mesh.indices.size() };

		const int intersected_triangle_index{ intersect_ray_triangles(vertex_positions, sizeof(XMFLOAT3), indices, index_count, ray_position, ray_direction, intersection, distance) };
		if (intersected_triangle_index >= 0)
		{
			if (closest_distance > distance)
			{
				closest_distance = distance;
				XMStoreFloat4(&closest_point, XMVector3TransformCoord(XMLoadFloat4(&intersection), concatenated_matrix));
				intersected_mesh = mesh.name;
				intersected_material = mesh.find_subset(intersected_triangle_index * 3)->material_name;
#if 1
				if (skip_if)
				{
					break;
				}
#endif
			}
		}
	}
	return closest_distance < FLT_MAX;
}
