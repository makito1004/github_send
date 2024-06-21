#pragma once

// UNIT.17
#include <d3d11.h>
#include <wrl.h>

#include <directxmath.h>

#include <vector>
#include <string>

#include <fbxsdk.h>

// UNIT.19
#include <set>
#include <unordered_map>

// UNIT.30
#include <cereal/archives/binary.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/set.hpp>
#include <cereal/types/unordered_map.hpp>

// UNIT.99
#include <functional>
#include <memory>
#include <unordered_map>
#include <mutex>

namespace DirectX
{
	template<class T>
	void serialize(T& archive, DirectX::XMFLOAT2& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y)
		);
	}

	template<class T>
	void serialize(T& archive, DirectX::XMFLOAT3& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y),
			cereal::make_nvp("z", v.z)
		);
	}

	template<class T>
	void serialize(T& archive, DirectX::XMFLOAT4& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y),
			cereal::make_nvp("z", v.z),
			cereal::make_nvp("w", v.w)
		);
	}

	template<class T>
	void serialize(T& archive, DirectX::XMFLOAT4X4& m)
	{
		archive(
			cereal::make_nvp("_11", m._11), cereal::make_nvp("_12", m._12),
			cereal::make_nvp("_13", m._13), cereal::make_nvp("_14", m._14),
			cereal::make_nvp("_21", m._21), cereal::make_nvp("_22", m._22),
			cereal::make_nvp("_23", m._23), cereal::make_nvp("_24", m._24),
			cereal::make_nvp("_31", m._31), cereal::make_nvp("_32", m._32),
			cereal::make_nvp("_33", m._33), cereal::make_nvp("_34", m._34),
			cereal::make_nvp("_41", m._41), cereal::make_nvp("_42", m._42),
			cereal::make_nvp("_43", m._43), cereal::make_nvp("_44", m._44)
		);
	}
}

// UNIT.24
struct skeleton
{
	struct bone
	{
		uint64_t unique_id{ 0 };
		std::string name;
		
		int64_t parent_index{ -1 }; ////自身を含む配列内の親骨の位置を指すインデックスです。
		// シーンのノード配列を参照するインデックス.
		int64_t node_index{ 0 };

		// offset_transformはモデル空間からボーン（ノード）シーンへの変換に使用
		DirectX::XMFLOAT4X4 offset_transform{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };

		bool is_orphan() const { return parent_index < 0; };

		// UNIT.30
		template<class T>
		void serialize(T& archive)
		{
			archive(unique_id, name, parent_index, node_index, offset_transform);
		}
	};
	std::vector<bone> bones;
	int64_t indexof(uint64_t unique_id) const
	{
		int64_t index{ 0 };
		for (const bone& bone : bones)
		{
			if (bone.unique_id == unique_id)
			{
				return index;
			}
			++index;
		}
		return -1;
	}
#if 0
	int64_t indexof(string name) const
	{
		int64_t index{ 0 };
		for (const bone& bone : bones)
		{
			if (bone.name == name)
			{
				return index;
			}
			++index;
		}
		return -1;
	}
#endif
	// UNIT.30
	template<class T>
	void serialize(T& archive)
	{
		archive(bones);
	}
};
// UNIT.25
struct animation
{
	std::string name;
	float sampling_rate{ 0 };

	struct keyframe
	{
		struct node
		{
			// global_transform'はローカル空間からシーンのグローバル空間への変換に使う.
			DirectX::XMFLOAT4X4 global_transform{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
			
			DirectX::XMFLOAT3 scaling{ 1, 1, 1 };
			DirectX::XMFLOAT4 rotation{ 0, 0, 0, 1 }; // Rotation quaternion
			DirectX::XMFLOAT3 translation{ 0, 0, 0 };

			// UNIT.30
			template<class T>
			void serialize(T& archive)
			{
				archive(global_transform, scaling, rotation, translation);
			}
		};
		std::vector<node> nodes;

		// UNIT.30
		template<class T>
		void serialize(T& archive)
		{
			archive(nodes);
		}
	};
	std::vector<keyframe> sequence;

	// UNIT.30
	template<class T>
	void serialize(T& archive)
	{
		archive(name, sampling_rate, sequence);
	}
};

// UNIT.99
template <class T>
struct animation_sequencer
{
private:
	T _clip;
	T _prev_clip;

	float _tick = 0.0f;
	size_t _frame = 0;
	bool _loop_time = false;

public:
	animation_sequencer(T initial_animation_clip) : _clip(initial_animation_clip), _prev_clip(initial_animation_clip) {}

	void transition(T next, bool loop_time = false)
	{
		if (_clip == next)
		{
			return;
		}

		_clip = next;
		_loop_time = loop_time;
		if (_clip != _prev_clip)
		{
			_frame = 0;
			_tick = 0;
		}
	}
	T clip() const
	{
		return _clip;
	}
	bool tictac(const std::vector<animation>& animation_clips, float delta_time)
	{
		_frame = static_cast<size_t>(_tick * animation_clips.at(static_cast<size_t>(_clip)).sampling_rate);
		_prev_clip = _clip;

		bool has_ended = false;
		size_t end_of_frame = animation_clips.at(static_cast<size_t>(_clip)).sequence.size();
		if (_frame < end_of_frame)
		{
			// playbacking
			_tick += delta_time;
		}
		else
		{
			if (_loop_time)
			{
				// loop playback
				_frame = 0;
				_tick = 0;
			}
			else
			{
				// stops on last frame
				_frame = end_of_frame - 1;
				has_ended = true;
			}
		}
		return has_ended;
	}
	const animation::keyframe* keyframe(const std::vector<animation>& animation_clips) const
	{
		return animation_clips.size() > 0 ? &animation_clips.at(static_cast<size_t>(_clip)).sequence.at(_frame) : nullptr;
	}
	size_t frame() const
	{
		return _frame;
	}

};

// UNIT.99
enum class geometric_attribute
{
	static_mesh, skinnned_mesh
};
// UNIT.17
class geometric_substance // UNIT.99
{
	// UNIT.17*
	struct scene
	{
		struct node
		{
			uint64_t unique_id{ 0 };
			std::string name;
			FbxNodeAttribute::EType attribute{ FbxNodeAttribute::EType::eUnknown };
			int64_t parent_index{ -1 };
			// UNIT.30
			template<class T>
			void serialize(T& archive)
			{
				archive(unique_id, name, attribute, parent_index);
			}
		};
		std::vector<node> nodes;
		int64_t indexof(uint64_t unique_id) const
		{
			int64_t index{ 0 };
			for (const node& node : nodes)
			{
				if (node.unique_id == unique_id)
				{
					return index;
				}
				++index;
			}
			return -1;
		}
#if 0
		int64_t indexof(string name) const
		{
			int64_t index{ 0 };
			for (const node& node : nodes)
			{
				if (node.name == name)
				{
					return index;
				}
				++index;
			}
			return -1;
		}
#endif
		// UNIT.30
		template<class T>
		void serialize(T& archive)
		{
			archive(nodes);
		}
	};

public:
	struct vertex_position // UNIT.99
	{
		DirectX::XMFLOAT3 position;
		//DirectX::XMFLOAT3 normal;
		//DirectX::XMFLOAT4 tangent; // UNIT.29
		//DirectX::XMFLOAT2 texcoord;

		// UNIT.30
		template<class T>
		void serialize(T& archive)
		{
			archive(position/*, normal, tangent, texcoord*/);
		}
	};
	// UNIT.99
	struct vertex_extra_attribute
	{
		DirectX::XMFLOAT3 normal;
		DirectX::XMFLOAT4 tangent; // UNIT.29
		DirectX::XMFLOAT2 texcoord;

		// UNIT.30
		template<class T>
		void serialize(T& archive)
		{
			archive(normal, tangent, texcoord);
		}
	};
	static const int MAX_BONE_INFLUENCES{ 4 }; 
	
	struct vertex_bone_influence
	{
		// 22
		float bone_weights[MAX_BONE_INFLUENCES]{ 1, 0, 0, 0 };
		uint32_t bone_indices[MAX_BONE_INFLUENCES]{};

		// UNIT.30
		template<class T>
		void serialize(T& archive)
		{
			archive(bone_weights, bone_indices);
		}
	};
	static const int MAX_BONES{ 256 }; // UNIT.23
	struct constants
	{
		DirectX::XMFLOAT4X4 world;
	};
	struct bone_constants
	{
		DirectX::XMFLOAT4X4 bone_transforms[MAX_BONES]{ 
			{ 1, 0, 0, 0,
			   0, 1, 0, 0, 
			   0 ,0, 1, 0, 
			   0, 0, 0, 1 } };
	};
	struct material_constants
	{
		DirectX::XMFLOAT4 ambient{ 0.2f, 0.2f, 0.2f, 1.0f };
		DirectX::XMFLOAT4 diffuse{ 0.8f, 0.8f, 0.8f, 1.0f }; // The w component is 'opacity'.
		DirectX::XMFLOAT4 specular{ 0.0f, 0.0f, 0.0f, 1.0f }; // The w component is 'shininess'.
		DirectX::XMFLOAT4 reflection{ 0.0f, 0.0f, 0.0f, 1.0f };
		DirectX::XMFLOAT4 emissive{ 0.0f, 0.0f, 0.0f, 1.0f };
		DirectX::XMFLOAT2 texcoord_offset{ 0.0f, 0.0f };
		DirectX::XMFLOAT2 texcoord_scale{ 1.0f, 1.0f };
	};

	// UNIT.18
	struct mesh
	{
		uint64_t unique_id{ 0 };
		std::string name;
		// 'node_index' is an index that refers to the node array of the scene.
		int64_t node_index{ 0 };

		std::vector<vertex_position> vertex_positions;
		std::vector<vertex_extra_attribute> vertex_extra_attributes;
		std::vector<vertex_bone_influence> vertex_bone_influences; // UNIT.99
		std::vector<uint32_t> indices;

		//attribute を初期化		
		geometric_attribute attribute{ geometric_attribute::skinnned_mesh }; 

		struct subset
		{
			uint64_t material_unique_id{ 0 };
			std::string material_name;

			uint32_t start_index_location{ 0 }; // GPUがインデックスバッファから最初に読み込んだインデックスの位置
			uint32_t index_count{ 0 }; //描画するインデックスの数

			
			template<class T>
			void serialize(T& archive)
			{
				archive(material_unique_id, material_name, start_index_location, index_count);
			}
		};
		std::vector<subset> subsets;

	
		DirectX::XMFLOAT4X4 default_global_transform{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
		
		DirectX::XMFLOAT4X4 geometric_transform{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };

	
		skeleton bind_pose;

		DirectX::XMFLOAT3 bounding_box[2];
		//ワールド空間でのバウンディングボックスに変換
		void transform_bounding_box(const DirectX::XMFLOAT4X4& world_transform, DirectX::XMFLOAT3 world_bounding_box[2]) const
		{
			DirectX::XMMATRIX concatenated_matrix{
				DirectX::XMLoadFloat4x4(&geometric_transform) *
				DirectX::XMLoadFloat4x4(&default_global_transform) *
				DirectX::XMLoadFloat4x4(&world_transform) };
			// 変換行列を使ってバウンディングボックスの最小点を変換
			DirectX::XMStoreFloat3(&world_bounding_box[0], DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&bounding_box[0]), concatenated_matrix));
			// 変換行列を使ってバウンディングボックスの最大点を変換
			DirectX::XMStoreFloat3(&world_bounding_box[1], DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&bounding_box[1]), concatenated_matrix));

			if (world_bounding_box[0].x > world_bounding_box[1].x) std::swap<float>(world_bounding_box[0].x, world_bounding_box[1].x);
			if (world_bounding_box[0].y > world_bounding_box[1].y) std::swap<float>(world_bounding_box[0].y, world_bounding_box[1].y);
			if (world_bounding_box[0].z > world_bounding_box[1].z) std::swap<float>(world_bounding_box[0].z, world_bounding_box[1].z);
		}

		template<class T>
		void serialize(T& archive)
		{
			archive(unique_id, name, node_index, subsets, default_global_transform, geometric_transform/*UNIT.99*/, bind_pose, bounding_box, vertex_positions/*UNIT.99*/, vertex_extra_attributes/*UNIT.99*/, vertex_bone_influences/*UNIT.99*/, indices);
		}
	private:
		Microsoft::WRL::ComPtr<ID3D11Buffer> vertex_buffers[3];
		Microsoft::WRL::ComPtr<ID3D11Buffer> index_buffer;
		friend class geometric_substance; // UNIT.99
	};
	std::vector<mesh> meshes;

	struct material
	{
		uint64_t unique_id{ 0 };
		std::string name;

		DirectX::XMFLOAT4 ambient{ 0.2f, 0.2f, 0.2f, 1.0f };
		DirectX::XMFLOAT4 diffuse{ 0.8f, 0.8f, 0.8f, 1.0f }; // The w component is 'opacity'.
		DirectX::XMFLOAT4 specular{ 0.0f, 0.0f, 0.0f, 1.0f }; // The w component is 'shininess'.
		DirectX::XMFLOAT4 reflection{ 0.0f, 0.0f, 0.0f, 1.0f };
		DirectX::XMFLOAT4 emissive{ 0.0f, 0.0f, 0.0f, 1.0f };

		std::string texture_filenames[4];
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_views[4];

		template<class T>
		void serialize(T& archive)
		{
			archive(unique_id, name, ambient, diffuse, specular, reflection, emissive, texture_filenames);
		}
	};
	std::unordered_map<uint64_t, material> materials;


	std::vector<animation> animation_clips;

	//パイプラインステートを一括管理することで、コードの複雑さを減少させ、メンテナンス性を向上させます
	struct pipeline_state
	{
		D3D11_PRIMITIVE_TOPOLOGY topology;
		ID3D11VertexShader* vertex_shader;
		ID3D11HullShader* hull_shader;
		ID3D11DomainShader* domain_shader;
		ID3D11GeometryShader* geometry_shader;
		ID3D11PixelShader* pixel_shader;
		ID3D11RasterizerState* rasterizer_state;
		ID3D11BlendState* blend_state;
		ID3D11DepthStencilState* depth_stencil_state;
		UINT stencil_ref;
	};
	struct shader_resources
	{
		material_constants material_data;
		ID3D11ShaderResourceView* shader_resource_views[4];
	};

private:
	Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shaders[4];
	Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel_shaders[2];
	Microsoft::WRL::ComPtr<ID3D11InputLayout> input_layouts[2];
	Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffers[3];
	
	Microsoft::WRL::ComPtr<ID3D11GeometryShader> geometry_shader;


	void create_com_objects(ID3D11Device* device, const char* fbx_filename);

	void spawn(ID3D11Device* device, const char* fbx_filename, bool triangulate, float sampling_rate, bool avoid_create_com_objects,
		std::function<void(mesh&, mesh::subset& subset)> callback);

public:
	geometric_substance(ID3D11Device* device, const char* fbx_filename, const std::vector<std::string>& animation_filenames = {}, bool triangulate = false, float sampling_rate = 0, bool avoid_create_com_objects = false/*UNIT.99*/);

	virtual ~geometric_substance() = default;
	//shadow_castingフラグ変数をオンにすると、頂点位置だけがバインドする

	void render(ID3D11DeviceContext* immediate_context, const DirectX::XMFLOAT4X4& world, const animation::keyframe* keyframe/*UNIT.25*/,
		std::function<int(const mesh&, const material&, shader_resources&, pipeline_state&)> callback = [](const mesh&, const material&, shader_resources&, pipeline_state&) { return 0; }/*UNIT.99*/);
	void cast_shadow(ID3D11DeviceContext* immediate_context, const DirectX::XMFLOAT4X4& world, const animation::keyframe* keyframe);
	// アニメーション
	void update_animation(animation::keyframe& keyframe);
	bool append_animations(const char* animation_filename, float sampling_rate /*0: default*/);
	void blend_animations(const animation::keyframe* keyframes[2], float factor, animation::keyframe& keyframe);
	
	DirectX::XMFLOAT4 joint(const char* mesh_name, const char* bone_name, const DirectX::XMFLOAT4X4& transform, const animation::keyframe* keyframe) const
	{
		for (const geometric_substance::mesh& mesh : meshes)
		{
			if (mesh.name != mesh_name) continue;
			for (const skeleton::bone& bone : mesh.bind_pose.bones)
			{
				if (bone.name != bone_name) continue;

				const animation::keyframe::node& node = keyframe->nodes.at(bone.node_index);
				DirectX::XMFLOAT4 joint_position = { node.global_transform._41, node.global_transform._42, node.global_transform._43, 1.0f };
				DirectX::XMStoreFloat4(&joint_position, DirectX::XMVector3Transform(DirectX::XMLoadFloat4(&joint_position), DirectX::XMLoadFloat4x4(&transform)));
				return joint_position;
			}
		}
		_ASSERT_EXPR(FALSE, L"Could not find a joint by 'bone-name'.");
		return { 0, 0, 0, 1 };
	}

protected:
	scene scene_view;

	void fetch_meshes(FbxScene* fbx_scene, std::vector<mesh>& meshes);
	void fetch_materials(FbxScene* fbx_scene, std::unordered_map<uint64_t, material>& materials);
	
	void fetch_skeleton(FbxMesh* fbx_mesh, skeleton& bind_pose);
	
	//  samplinr_rateが負の数の場合、アニメーションデータはロードしない
	void fetch_animations(FbxScene* fbx_scene, std::vector<animation>& animation_clips, float sampling_rate/*値が0の場合、アニメーションデータはデフォルトのフレームレートでサンプリングする.*/);
	
	void fetch_scene(const char* fbx_filename, bool triangulate, float sampling_rate/*値が0の場合、アニメーションデータはデフォルトのフレームレートでサンプリングする*/);


	
private:
	static std::unordered_map<std::string, std::shared_ptr<geometric_substance>> _geometric_substances;
public:
#if 0
	static std::shared_ptr<geometric_substance> _at(const char* name) { return _geometric_substances.at(name); }
#endif
	static std::shared_ptr<geometric_substance> _emplace(ID3D11Device* device, const char* name, const std::vector<std::string>& animation_filenames = {}, bool triangulate = false, float sampling_rate = 0, bool avoid_create_com_objects = false)
	{
		if (_geometric_substances.find(name) == _geometric_substances.end())
		{
			std::lock_guard<std::mutex> lock(_mutex);
			_geometric_substances.emplace(std::make_pair(name, std::make_shared<geometric_substance>(device, name, animation_filenames, triangulate, sampling_rate, avoid_create_com_objects)));
		}
		return _geometric_substances.at(name);
	}
	static void _exterminate()
	{
		std::lock_guard<std::mutex> lock(_mutex);
		_geometric_substances.clear();
	}
private:
	static std::mutex _mutex;

};

