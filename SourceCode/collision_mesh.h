#pragma once
#include <algorithm>

// UNIT.99
#include "geometric_substance.h"

class collision_mesh
{
public:
	struct mesh
	{
		std::string name;
		std::vector<DirectX::XMFLOAT3> vertex_positions;
		std::vector<uint32_t> indices;
		struct subset
		{
			std::string material_name;

			uint32_t start_index_location{ 0 }; // GPUがインデックスバッファから最初に読み込んだインデックスの位置
			uint32_t index_count{ 0 }; // 描画するインデックスの数。

			void operator=(const geometric_substance::mesh::subset& rhs)
			{
				material_name = rhs.material_name;
				start_index_location = rhs.start_index_location;
				index_count = rhs.index_count;
			}
		};
		std::vector<subset> subsets;
		const subset* find_subset(uint32_t index) const
		{
			for (const subset& subset : subsets)
			{
				if (subset.start_index_location <= index && subset.start_index_location + subset.index_count > index)
				{
					return &subset;
				}
			}
			return nullptr;
		}
		//データを簡単に複製することができ,レンダリングの準備において非常に便利なためコピーする
		DirectX::XMFLOAT4X4 default_global_transform{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
		DirectX::XMFLOAT4X4 geometric_transform{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
		DirectX::XMFLOAT3 bounding_box[2]{};

		void operator=(const geometric_substance::mesh& rhs)
		{
			name = rhs.name;
			default_global_transform = rhs.default_global_transform;
			geometric_transform = rhs.geometric_transform;
			bounding_box[0] = rhs.bounding_box[0];
			bounding_box[1] = rhs.bounding_box[1];

			size_t vertex_count{ rhs.vertex_positions.size() };
			vertex_positions.resize(vertex_count);
			for (size_t vertex_index = 0; vertex_index < vertex_count; ++vertex_index)
			{
				vertex_positions.at(vertex_index) = rhs.vertex_positions.at(vertex_index).position;
			}

			size_t index_count{ rhs.indices.size() };
			indices.resize(index_count);
			for (size_t index_index = 0; index_index < index_count; ++index_index)
			{
				indices.at(index_index) = rhs.indices.at(index_index);
			}

			size_t subset_count{ rhs.subsets.size() };
			subsets.resize(subset_count);
			for (size_t subset_index = 0; subset_index < subset_count; ++subset_index)
			{
				subsets.at(subset_index) = rhs.subsets.at(subset_index);
			}
		}
	};
	std::vector<mesh> meshes;

	collision_mesh(ID3D11Device* device, const char* fbx_filename, bool triangulate = false)
	{
		geometric_substance interim_geometric_substance(device, fbx_filename, {}, triangulate, 0, true/*avoid_create_com_objects*/);
		size_t mesh_count = interim_geometric_substance.meshes.size();
		meshes.resize(mesh_count);
		for (size_t mesh_index = 0; mesh_index < mesh_count; ++mesh_index)
		{
			meshes.at(mesh_index) = interim_geometric_substance.meshes.at(mesh_index);
		}
	}

	// すべての関数の引数の座標系はワールド空間.
	bool raycast(const DirectX::XMFLOAT4& position, const DirectX::XMFLOAT4& direction, const DirectX::XMFLOAT4X4& world_transform, _Out_ DirectX::XMFLOAT4& closest_point,
		_Out_ std::string& intersected_mesh, _Out_ std::string& intersected_material, bool skip_if = true/*Once the first intersection is found, the process is interrupted.*/) const;
};
