
#include "misc.h"
#include "geometric_substance.h"

#include <sstream>

using namespace DirectX;

#include "shader.h"

#include <filesystem>
#include "texture.h"

#include <fstream>

//FbxAMatrix 型の行列を、DirectXMathライブラリの XMFLOAT4X4 型の行列に変換
inline XMFLOAT4X4 to_xmfloat4x4(const FbxAMatrix& fbxamatrix)
{
	XMFLOAT4X4 xmfloat4x4;
	for (int row = 0; row < 4; row++)
	{
		for (int column = 0; column < 4; column++)
		{
			xmfloat4x4.m[row][column] = static_cast<float>(fbxamatrix[row][column]);
		}
	}
	return xmfloat4x4;
}
inline XMFLOAT3 to_xmfloat3(const FbxDouble3& fbxdouble3)
{
	XMFLOAT3 xmfloat3;
	xmfloat3.x = static_cast<float>(fbxdouble3[0]);
	xmfloat3.y = static_cast<float>(fbxdouble3[1]);
	xmfloat3.z = static_cast<float>(fbxdouble3[2]);
	return xmfloat3;
}
inline XMFLOAT4 to_xmfloat4(const FbxDouble4& fbxdouble4)
{
	XMFLOAT4 xmfloat4;
	xmfloat4.x = static_cast<float>(fbxdouble4[0]);
	xmfloat4.y = static_cast<float>(fbxdouble4[1]);
	xmfloat4.z = static_cast<float>(fbxdouble4[2]);
	xmfloat4.w = static_cast<float>(fbxdouble4[3]);
	return xmfloat4;
}

// 
struct bone_influence
{
	uint32_t bone_index;
	float bone_weight;
};
using bone_influences_per_control_point = std::vector<bone_influence>;
void fetch_bone_influences(const FbxMesh* fbx_mesh, std::vector<bone_influences_per_control_point>& bone_influences)
{
	const int control_points_count{ fbx_mesh->GetControlPointsCount() };
	bone_influences.resize(control_points_count);

	const int skin_count{ fbx_mesh->GetDeformerCount(FbxDeformer::eSkin) };
	for (int skin_index = 0; skin_index < skin_count; ++skin_index)
	{
		const FbxSkin* fbx_skin{ static_cast<FbxSkin*>(fbx_mesh->GetDeformer(skin_index, FbxDeformer::eSkin)) };

		const int cluster_count{ fbx_skin->GetClusterCount() };
		for (int cluster_index = 0; cluster_index < cluster_count; ++cluster_index)
		{
			const FbxCluster* fbx_cluster{ fbx_skin->GetCluster(cluster_index) };

			const int control_point_indices_count{ fbx_cluster->GetControlPointIndicesCount() };
			for (int control_point_indices_index = 0; control_point_indices_index < control_point_indices_count; ++control_point_indices_index)
			{
#if 1
				int control_point_index{ fbx_cluster->GetControlPointIndices()[control_point_indices_index] };
				double control_point_weight{ fbx_cluster->GetControlPointWeights()[control_point_indices_index] };
				bone_influence& bone_influence{ bone_influences.at(control_point_index).emplace_back() };
				bone_influence.bone_index = static_cast<uint32_t>(cluster_index);
				bone_influence.bone_weight = static_cast<float>(control_point_weight);
#else
				bone_influences.at((fbx_cluster->GetControlPointIndices())[control_point_indices_index]).emplace_back()
					= { static_cast<uint32_t>(cluster_index),  static_cast<float>((fbx_cluster->GetControlPointWeights())[control_point_indices_index]) };
#endif
			}
		}
	}
}

void geometric_substance::fetch_scene(const char* fbx_filename, bool triangulate, float sampling_rate)
{
	FbxManager* fbx_manager{ FbxManager::Create() };
	FbxScene* fbx_scene{ FbxScene::Create(fbx_manager, "") };
	FbxImporter* fbx_importer{ FbxImporter::Create(fbx_manager, "") };
	bool import_status{ false };
	import_status = fbx_importer->Initialize(fbx_filename);
	_ASSERT_EXPR_A(import_status, fbx_importer->GetStatus().GetErrorString());
	import_status = fbx_importer->Import(fbx_scene);
	_ASSERT_EXPR_A(import_status, fbx_importer->GetStatus().GetErrorString());

	FbxGeometryConverter fbx_converter(fbx_manager);
	if (triangulate)
	{
		fbx_converter.Triangulate(fbx_scene, true, false);
		fbx_converter.RemoveBadPolygonsFromMeshes(fbx_scene);
	}

	// シーングラフ全体をシリアライズする
	std::function<void(FbxNode*)> traverse{ [&](FbxNode* fbx_node) {
#if 0
		if (fbx_node->GetNodeAttribute())
		{
			switch (fbx_node->GetNodeAttribute()->GetAttributeType())
			{
			case FbxNodeAttribute::EType::eNull:
			case FbxNodeAttribute::EType::eMesh:
			case FbxNodeAttribute::EType::eSkeleton:
			case FbxNodeAttribute::EType::eUnknown:
			case FbxNodeAttribute::EType::eMarker:
			case FbxNodeAttribute::EType::eCamera:
			case FbxNodeAttribute::EType::eLight:
				scene::node& node{ scene_view.nodes.emplace_back() };
				node.attribute = fbx_node->GetNodeAttribute()->GetAttributeType();
				node.name = fbx_node->GetName();
				node.unique_id = fbx_node->GetUniqueID();
				node.parent_index = scene_view.indexof(fbx_node->GetParent() ? fbx_node->GetParent()->GetUniqueID() : 0);
				break;
			}
		}
#else
		scene::node& node{ scene_view.nodes.emplace_back() };
		node.attribute = fbx_node->GetNodeAttribute() ? fbx_node->GetNodeAttribute()->GetAttributeType() : FbxNodeAttribute::EType::eUnknown;
		node.name = fbx_node->GetName();
		node.unique_id = fbx_node->GetUniqueID();
		node.parent_index = scene_view.indexof(fbx_node->GetParent() ? fbx_node->GetParent()->GetUniqueID() : 0);
#endif
		for (int child_index = 0; child_index < fbx_node->GetChildCount(); ++child_index)
		{
			traverse(fbx_node->GetChild(child_index));
		}
	} };
	traverse(fbx_scene->GetRootNode());

	fetch_meshes(fbx_scene, meshes);

	fetch_materials(fbx_scene, materials);


#if 0
	float sampling_rate{ 0 };
#endif
	fetch_animations(fbx_scene, animation_clips, sampling_rate);


	fbx_manager->Destroy();
}


geometric_substance::geometric_substance(ID3D11Device* device, const char* fbx_filename, const std::vector<std::string>& animation_filenames, bool triangulate, float sampling_rate, bool avoid_create_com_objects/*UNIT.99*/)
{
	std::filesystem::path cereal_filename(fbx_filename);
	cereal_filename.replace_extension("cereal");
	if (std::filesystem::exists(cereal_filename.c_str()))
	{
		std::ifstream ifs(cereal_filename.c_str(), std::ios::binary);
		cereal::BinaryInputArchive deserialization(ifs);
		deserialization(scene_view, meshes, materials, animation_clips);
	}
	else
	{
		//シーンの読み込み
		fetch_scene(fbx_filename, triangulate, sampling_rate);

		//アニメーションファイルの追加
		for (const std::string animation_filename : animation_filenames)
		{
			append_animations(animation_filename.c_str(), sampling_rate);
		}

		
		std::ofstream ofs(cereal_filename.c_str(), std::ios::binary);
		cereal::BinaryOutputArchive serialization(ofs);
		serialization(scene_view, meshes, materials, animation_clips);
	}
	//オブジェクトの作成を制御
	if (!avoid_create_com_objects) 
	{
		create_com_objects(device, fbx_filename);
	}

}


void geometric_substance::fetch_meshes(FbxScene* fbx_scene, std::vector<mesh>& meshes)
{
	for (const scene::node& node : scene_view.nodes)
	{
		if (node.attribute != FbxNodeAttribute::EType::eMesh)
		{
			continue;
		}

		FbxNode* fbx_node{ fbx_scene->FindNodeByName(node.name.c_str()) };
		FbxMesh* fbx_mesh{ fbx_node->GetMesh() };

		mesh& mesh{ meshes.emplace_back() };
#if 0
		mesh.unique_id = fbx_mesh->GetNode()->GetUniqueID();
		mesh.name = fbx_mesh->GetNode()->GetName();
		mesh.node_index = scene_view.indexof(mesh.unique_id);
		// UNIT.21
		mesh.default_global_transform = to_xmfloat4x4(fbx_mesh->GetNode()->EvaluateGlobalTransform());
#else
		mesh.unique_id = node.unique_id;
		mesh.name = node.name;
		mesh.node_index = scene_view.indexof(node.unique_id);
		mesh.default_global_transform = to_xmfloat4x4(fbx_node->EvaluateGlobalTransform());
#endif

	
		mesh.geometric_transform = to_xmfloat4x4(FbxAMatrix(
			fbx_mesh->GetNode()->GetGeometricTranslation(FbxNode::eSourcePivot),
			fbx_mesh->GetNode()->GetGeometricRotation(FbxNode::eSourcePivot),
			fbx_mesh->GetNode()->GetGeometricScaling(FbxNode::eSourcePivot)));

		
		std::vector<bone_influences_per_control_point> bone_influences;
		fetch_bone_influences(fbx_mesh, bone_influences);

		fetch_skeleton(fbx_mesh, mesh.bind_pose);

	//各マテリアルに対してサブセットを設定
		std::vector<mesh::subset>& subsets{ mesh.subsets };
		const int material_count{ fbx_mesh->GetNode()->GetMaterialCount() };
		subsets.resize(material_count > 0 ? material_count : 1);
		for (int material_index = 0; material_index < material_count; ++material_index)
		{
			//各サブセットの開始位置とインデックスカウントを設定
			const FbxSurfaceMaterial* fbx_material{ fbx_mesh->GetNode()->GetMaterial(material_index) };
			subsets.at(material_index).material_name = fbx_material->GetName();
			subsets.at(material_index).material_unique_id = fbx_material->GetUniqueID();
		}
		if (material_count > 0)
		{
			//素材の面を数える
			const int polygon_count{ fbx_mesh->GetPolygonCount() };
			for (int polygon_index = 0; polygon_index < polygon_count; ++polygon_index)
			{
				const int material_index{ fbx_mesh->GetElementMaterial()->GetIndexArray().GetAt(polygon_index) };
				subsets.at(material_index).index_count += 3;
			}
			uint32_t offset{ 0 };
			for (mesh::subset& subset : subsets)
			{
				subset.start_index_location = offset;
				offset += subset.index_count;
				// カウンタとして使用され0にリセット
				subset.index_count = 0;
			}
		}

		//FBXメッシュのポリゴン数に基づいて、メッシュの頂点情報を格納するための適切なサイズにする処理
		const int polygon_count{ fbx_mesh->GetPolygonCount() };
		mesh.vertex_positions.resize(polygon_count * 3LL);
		mesh.vertex_extra_attributes.resize(polygon_count * 3LL);
		mesh.vertex_bone_influences.resize(polygon_count * 3LL);
		mesh.indices.resize(polygon_count * 3LL);

		FbxStringList uv_names;
		fbx_mesh->GetUVSetNames(uv_names);
		const FbxVector4* control_points{ fbx_mesh->GetControlPoints() };
		for (int polygon_index = 0; polygon_index < polygon_count; ++polygon_index)
		{
			
			const int material_index{ material_count > 0 ? fbx_mesh->GetElementMaterial()->GetIndexArray().GetAt(polygon_index) : 0 };
			mesh::subset& subset{ subsets.at(material_index) };
			const uint32_t offset{ subset.start_index_location + subset.index_count };

			for (int position_in_polygon = 0; position_in_polygon < 3; ++position_in_polygon)
			{
				const int vertex_index{ polygon_index * 3 + position_in_polygon };

				vertex_position vertex_position;
				vertex_extra_attribute vertex_extra_attribute;
				vertex_bone_influence vertex_bone_influence;
				const int polygon_vertex{ fbx_mesh->GetPolygonVertex(polygon_index, position_in_polygon) };
				vertex_position.position.x = static_cast<float>(control_points[polygon_vertex][0]);
				vertex_position.position.y = static_cast<float>(control_points[polygon_vertex][1]);
				vertex_position.position.z = static_cast<float>(control_points[polygon_vertex][2]);

				// 頂点に対するボーンの影響（ボーンウェイトとボーンインデックス）を設定
				const bone_influences_per_control_point& influences_per_control_point{ bone_influences.at(polygon_vertex) };
				for (size_t influence_index = 0; influence_index < influences_per_control_point.size(); ++influence_index)
				{
					if (influence_index < MAX_BONE_INFLUENCES)
					{
						vertex_bone_influence.bone_weights[influence_index] = influences_per_control_point.at(influence_index).bone_weight;
						vertex_bone_influence.bone_indices[influence_index] = influences_per_control_point.at(influence_index).bone_index;
					}
#if 1
					else
					{
						size_t minimum_value_index = 0;
						float minimum_value = FLT_MAX;
						for (size_t i = 0; i < MAX_BONE_INFLUENCES; ++i)
						{
							if (minimum_value > vertex_bone_influence.bone_weights[i])
							{
								minimum_value = vertex_bone_influence.bone_weights[i];
								minimum_value_index = i;
							}
						}
						vertex_bone_influence.bone_weights[minimum_value_index] += influences_per_control_point.at(influence_index).bone_weight;
						vertex_bone_influence.bone_indices[minimum_value_index] = influences_per_control_point.at(influence_index).bone_index;
					}
#endif
				}

#if 0
				// Normalize bone weights
				float total_weight = 0;
				for (size_t i = 0; i < MAX_BONE_INFLUENCES; ++i)
				{
					total_weight += vertex_bone_influence.bone_weights[i];
				}
				for (size_t i = 0; i < MAX_BONE_INFLUENCES; ++i)
				{
					vertex_bone_influence.bone_weights[i] /= total_weight;
				}
#endif

			
				if (fbx_mesh->GenerateNormals(false))
				{
					FbxVector4 normal;
					fbx_mesh->GetPolygonVertexNormal(polygon_index, position_in_polygon, normal);
					vertex_extra_attribute.normal.x = static_cast<float>(normal[0]);
					vertex_extra_attribute.normal.y = static_cast<float>(normal[1]);
					vertex_extra_attribute.normal.z = static_cast<float>(normal[2]);
				}

				if (fbx_mesh->GetElementUVCount() > 0)
				{
					FbxVector2 uv;
					bool unmapped_uv;
					fbx_mesh->GetPolygonVertexUV(polygon_index, position_in_polygon, uv_names[0], uv, unmapped_uv);
					vertex_extra_attribute.texcoord.x = static_cast<float>(uv[0]);
					vertex_extra_attribute.texcoord.y = 1.0f - static_cast<float>(uv[1]);
				}
				// ファイルに法線ベクトル情報を持たない場合はGenerateTangentsData関数で生成する 
				if (fbx_mesh->GenerateTangentsData(0, false))
				{
					const FbxGeometryElementTangent* tangent = fbx_mesh->GetElementTangent(0);
					_ASSERT_EXPR(tangent->GetMappingMode() == FbxGeometryElement::EMappingMode::eByPolygonVertex &&
						tangent->GetReferenceMode() == FbxGeometryElement::EReferenceMode::eDirect,
						L"Only supports a combination of these modes.");

					vertex_extra_attribute.tangent.x = static_cast<float>(tangent->GetDirectArray().GetAt(vertex_index)[0]);
					vertex_extra_attribute.tangent.y = static_cast<float>(tangent->GetDirectArray().GetAt(vertex_index)[1]);
					vertex_extra_attribute.tangent.z = static_cast<float>(tangent->GetDirectArray().GetAt(vertex_index)[2]);
					vertex_extra_attribute.tangent.w = static_cast<float>(tangent->GetDirectArray().GetAt(vertex_index)[3]);
				}
				mesh.vertex_extra_attributes.at(vertex_index) = std::move(vertex_extra_attribute);
				mesh.vertex_bone_influences.at(vertex_index) = std::move(vertex_bone_influence);
				mesh.vertex_positions.at(vertex_index) = std::move(vertex_position);


#if 0
				mesh.indices.at(vertex_index) = vertex_index;
#else
				mesh.indices.at(static_cast<size_t>(offset) + position_in_polygon) = vertex_index;
				subset.index_count++;
#endif
			}
		}
		
		mesh.bounding_box[0] = { +D3D11_FLOAT32_MAX, +D3D11_FLOAT32_MAX, +D3D11_FLOAT32_MAX };
		mesh.bounding_box[1] = { -D3D11_FLOAT32_MAX, -D3D11_FLOAT32_MAX, -D3D11_FLOAT32_MAX };
		for (const vertex_position& v : mesh.vertex_positions)
		{
			mesh.bounding_box[0].x = std::min<float>(mesh.bounding_box[0].x, v.position.x);
			mesh.bounding_box[0].y = std::min<float>(mesh.bounding_box[0].y, v.position.y);
			mesh.bounding_box[0].z = std::min<float>(mesh.bounding_box[0].z, v.position.z);
			mesh.bounding_box[1].x = std::max<float>(mesh.bounding_box[1].x, v.position.x);
			mesh.bounding_box[1].y = std::max<float>(mesh.bounding_box[1].y, v.position.y);
			mesh.bounding_box[1].z = std::max<float>(mesh.bounding_box[1].z, v.position.z);
		}
	}
}

void geometric_substance::create_com_objects(ID3D11Device* device, const char* fbx_filename)
{
	
	for (mesh& mesh : meshes)
	{
		//メッシュの属性を設定
		mesh.attribute = mesh.bind_pose.bones.size() > 0 ? geometric_attribute::skinnned_mesh : geometric_attribute::static_mesh;

		HRESULT hr{ S_OK };
		D3D11_BUFFER_DESC buffer_desc{};
		D3D11_SUBRESOURCE_DATA subresource_data{};
		buffer_desc.ByteWidth = static_cast<UINT>(sizeof(vertex_position) * mesh.vertex_positions.size());
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		buffer_desc.CPUAccessFlags = 0;
		buffer_desc.MiscFlags = 0;
		buffer_desc.StructureByteStride = 0;
		subresource_data.pSysMem = mesh.vertex_positions.data();
		subresource_data.SysMemPitch = 0;
		subresource_data.SysMemSlicePitch = 0;
		hr = device->CreateBuffer(&buffer_desc, &subresource_data, mesh.vertex_buffers[0].ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));    

		
		buffer_desc.ByteWidth = static_cast<UINT>(sizeof(vertex_extra_attribute) * mesh.vertex_extra_attributes.size());
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		subresource_data.pSysMem = mesh.vertex_extra_attributes.data();
		hr = device->CreateBuffer(&buffer_desc, &subresource_data, mesh.vertex_buffers[1].ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		buffer_desc.ByteWidth = static_cast<UINT>(sizeof(vertex_bone_influence) * mesh.vertex_bone_influences.size());
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		subresource_data.pSysMem = mesh.vertex_bone_influences.data();
		hr = device->CreateBuffer(&buffer_desc, &subresource_data, mesh.vertex_buffers[2].ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		buffer_desc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * mesh.indices.size());
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		subresource_data.pSysMem = mesh.indices.data();
		hr = device->CreateBuffer(&buffer_desc, &subresource_data, mesh.index_buffer.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		mesh.vertex_positions.clear();
		mesh.vertex_extra_attributes.clear();
		mesh.vertex_bone_influences.clear();
		mesh.indices.clear();
	}

	for (std::unordered_map<uint64_t, material>::iterator iterator = materials.begin(); iterator != materials.end(); ++iterator)
	{
		// テクスチャをロードし、シェーダーリソースビューに設定する処理
		for (size_t texture_index = 0; texture_index < 2; ++texture_index)
		{
			if (iterator->second.texture_filenames[texture_index].size() > 0)
			{
				std::filesystem::path path(fbx_filename);
				path.replace_filename(iterator->second.texture_filenames[texture_index]);
				iterator->second.shader_resource_views[texture_index] = texture::_emplace(device, path.c_str());
			}
			else
			{	
				iterator->second.shader_resource_views[texture_index] = texture::_emplace(device, texture_index == 1 ? 0xFFFF7F7F : 0xFFFFFFFF, 16);
			}
		}
	}

	HRESULT hr = S_OK;
	D3D11_INPUT_ELEMENT_DESC input_element_desc[]
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "WEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 2, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BONES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 2, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	// シェーダーをデバイスにロードおよび配置するための処理
	vertex_shaders[0] = shader<ID3D11VertexShader>::_emplace(device, "static_mesh_vs.cso", input_layouts[0].ReleaseAndGetAddressOf(), input_element_desc, 4);
	vertex_shaders[1] = shader<ID3D11VertexShader>::_emplace(device, "skinned_mesh_vs.cso", input_layouts[1].ReleaseAndGetAddressOf(), input_element_desc, 6);
	vertex_shaders[2] = shader<ID3D11VertexShader>::_emplace(device, "static_mesh_csm_vs.cso", NULL, NULL, 0);
	vertex_shaders[3] = shader<ID3D11VertexShader>::_emplace(device, "skinned_mesh_csm_vs.cso", NULL, NULL, 0);
	pixel_shaders[0] = shader<ID3D11PixelShader>::_emplace(device, "static_mesh_ps.cso");
	pixel_shaders[1] = shader<ID3D11PixelShader>::_emplace(device, "skinned_mesh_ps.cso");
	geometry_shader = shader<ID3D11GeometryShader>::_emplace(device, "geometric_substance_csm_gs.cso");

	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = sizeof(constants);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	hr = device->CreateBuffer(&buffer_desc, nullptr, constant_buffers[0].ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	buffer_desc.ByteWidth = sizeof(bone_constants);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	hr = device->CreateBuffer(&buffer_desc, nullptr, constant_buffers[1].ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	buffer_desc.ByteWidth = sizeof(material_constants);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	hr = device->CreateBuffer(&buffer_desc, nullptr, constant_buffers[2].ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

void geometric_substance::render(ID3D11DeviceContext* immediate_context, const XMFLOAT4X4& world, const animation::keyframe* keyframe/*UNIT.25*/,
	std::function<int(const mesh&, const material&, shader_resources&, pipeline_state&)> callback/*UNIT.99*/)
{
	D3D11_PRIMITIVE_TOPOLOGY cached_topology;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> cached_vertex_shader;
	Microsoft::WRL::ComPtr<ID3D11HullShader> cached_hull_shader;                           
	Microsoft::WRL::ComPtr<ID3D11DomainShader> cached_domain_shader;
	Microsoft::WRL::ComPtr<ID3D11GeometryShader> cached_geometry_shader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> cached_pixel_shader;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> cached_rasterizer_state;
	Microsoft::WRL::ComPtr<ID3D11BlendState> cached_blend_state;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> cached_depth_stencil_state;
	UINT cached_stencil_ref;

	immediate_context->IAGetPrimitiveTopology(&cached_topology);
	immediate_context->VSGetShader(cached_vertex_shader.ReleaseAndGetAddressOf(), nullptr, nullptr);
	immediate_context->DSGetShader(cached_domain_shader.ReleaseAndGetAddressOf(), nullptr, nullptr);
	immediate_context->HSGetShader(cached_hull_shader.ReleaseAndGetAddressOf(), nullptr, nullptr);
	immediate_context->GSGetShader(cached_geometry_shader.ReleaseAndGetAddressOf(), nullptr, nullptr);
	immediate_context->PSGetShader(cached_pixel_shader.ReleaseAndGetAddressOf(), nullptr, nullptr);
	immediate_context->RSGetState(cached_rasterizer_state.ReleaseAndGetAddressOf());
	immediate_context->OMGetBlendState(cached_blend_state.ReleaseAndGetAddressOf(), nullptr, nullptr);
	immediate_context->OMGetDepthStencilState(cached_depth_stencil_state.ReleaseAndGetAddressOf(), &cached_stencil_ref);

	for (mesh& mesh : meshes)
	{
		uint32_t strides[3] = { sizeof(vertex_position), sizeof(vertex_extra_attribute), sizeof(vertex_bone_influence) };
		uint32_t offsets[3] = { 0, 0, 0 };
		ID3D11Buffer* vertex_buffers[3] =
		{
			mesh.vertex_buffers[0].Get(),
			mesh.vertex_buffers[1].Get(),
			mesh.attribute == geometric_attribute::skinnned_mesh ? mesh.vertex_buffers[2].Get() : nullptr
		};
		immediate_context->IASetVertexBuffers(0, static_cast<size_t>(mesh.attribute) + 2, vertex_buffers, strides, offsets);
		immediate_context->IASetIndexBuffer(mesh.index_buffer.Get(), DXGI_FORMAT_R32_UINT, 0);
		immediate_context->IASetInputLayout(input_layouts[static_cast<size_t>(mesh.attribute)].Get());

		constants data;
		if (keyframe && keyframe->nodes.size() > 0)
		{
			const animation::keyframe::node& mesh_node = keyframe->nodes.at(mesh.node_index);
#if 0
			XMStoreFloat4x4(&data.world, XMLoadFloat4x4(&mesh_node.global_transform) * XMLoadFloat4x4(&world));
#else
			XMStoreFloat4x4(&data.world, XMLoadFloat4x4(&mesh.geometric_transform)/*UNIT.99*/ * XMLoadFloat4x4(&mesh_node.global_transform) * XMLoadFloat4x4(&world));
#endif
			const size_t bone_count = mesh.bind_pose.bones.size();
			_ASSERT_EXPR(bone_count < MAX_BONES, L"The value of the 'bone_count' has exceeded MAX_BONES.");

			bone_constants bone_data;
			for (size_t bone_index = 0; bone_index < bone_count; ++bone_index)
			{
				const skeleton::bone& bone{ mesh.bind_pose.bones.at(bone_index) };
				const animation::keyframe::node& bone_node{ keyframe->nodes.at(bone.node_index) };
				XMStoreFloat4x4(&bone_data.bone_transforms[bone_index],
					XMLoadFloat4x4(&bone.offset_transform) *
					XMLoadFloat4x4(&bone_node.global_transform) *
					XMMatrixInverse(nullptr, XMLoadFloat4x4(&mesh_node.global_transform))
				);
			}
			if (mesh.attribute == geometric_attribute::skinnned_mesh)
			{
				immediate_context->UpdateSubresource(constant_buffers[1].Get(), 0, 0, &bone_data, 0, 0);
				immediate_context->VSSetConstantBuffers(1, 1, constant_buffers[1].GetAddressOf());
			}
		}
		else
		{
#if 0
			XMStoreFloat4x4(&data.world, XMLoadFloat4x4(&mesh.default_global_transform) * XMLoadFloat4x4(&world));
#else
			XMStoreFloat4x4(&data.world, XMLoadFloat4x4(&mesh.geometric_transform)/*UNIT.99*/ * XMLoadFloat4x4(&mesh.default_global_transform) * XMLoadFloat4x4(&world));
#endif

#if 1
			if (mesh.attribute == geometric_attribute::skinnned_mesh) // UNIT.99
			{
				//必要なボーンデータがない場合でも適切に動作するようにするためバインドする
				bone_constants bone_data;
				for (size_t bone_index = 0; bone_index < MAX_BONES; ++bone_index)
				{
					bone_data.bone_transforms[bone_index] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
				}
				immediate_context->UpdateSubresource(constant_buffers[1].Get(), 0, 0, &bone_data, 0, 0);
				immediate_context->VSSetConstantBuffers(1, 1, constant_buffers[1].GetAddressOf());
			}
#endif
		}
		immediate_context->UpdateSubresource(constant_buffers[0].Get(), 0, 0, &data, 0, 0);
		immediate_context->VSSetConstantBuffers(0, 1, constant_buffers[0].GetAddressOf());

		for (const mesh::subset& subset : mesh.subsets)
		{
			const material& material = materials.at(subset.material_unique_id);

			
			pipeline_state default_pipeline_state;
			default_pipeline_state.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			default_pipeline_state.vertex_shader = vertex_shaders[static_cast<size_t>(mesh.attribute)].Get();
			default_pipeline_state.hull_shader = nullptr;
			default_pipeline_state.domain_shader = nullptr;
			default_pipeline_state.geometry_shader = nullptr;
			default_pipeline_state.pixel_shader = pixel_shaders[static_cast<size_t>(mesh.attribute)].Get();
			default_pipeline_state.rasterizer_state = cached_rasterizer_state.Get();
			default_pipeline_state.blend_state = cached_blend_state.Get();
			default_pipeline_state.depth_stencil_state = cached_depth_stencil_state.Get();
			default_pipeline_state.stencil_ref = cached_stencil_ref;

			shader_resources default_shader_resources;
			default_shader_resources.material_data.ambient = material.ambient;
			default_shader_resources.material_data.diffuse = material.diffuse;
			default_shader_resources.material_data.specular = material.specular;
			default_shader_resources.material_data.reflection = material.reflection;
			default_shader_resources.material_data.emissive = material.emissive;
			default_shader_resources.shader_resource_views[0] = material.shader_resource_views[0].Get();
			default_shader_resources.shader_resource_views[1] = material.shader_resource_views[1].Get();
			default_shader_resources.shader_resource_views[2] = material.shader_resource_views[2].Get();
			default_shader_resources.shader_resource_views[3] = material.shader_resource_views[3].Get();

			if (callback(mesh, material, default_shader_resources, default_pipeline_state) >= 0)
			{
				immediate_context->UpdateSubresource(constant_buffers[2].Get(), 0, 0, &default_shader_resources.material_data, 0, 0);
				immediate_context->VSSetConstantBuffers(2, 1, constant_buffers[2].GetAddressOf());
				immediate_context->PSSetConstantBuffers(2, 1, constant_buffers[2].GetAddressOf());

				immediate_context->PSSetShaderResources(0, 1, &default_shader_resources.shader_resource_views[0]);
				immediate_context->PSSetShaderResources(1, 1, &default_shader_resources.shader_resource_views[1]);

				immediate_context->IASetPrimitiveTopology(default_pipeline_state.topology);
				immediate_context->VSSetShader(default_pipeline_state.vertex_shader, nullptr, 0);
				immediate_context->DSSetShader(default_pipeline_state.domain_shader, nullptr, 0);
				immediate_context->HSSetShader(default_pipeline_state.hull_shader, nullptr, 0);
				immediate_context->GSSetShader(default_pipeline_state.geometry_shader, nullptr, 0);
				immediate_context->PSSetShader(default_pipeline_state.pixel_shader, nullptr, 0);
				immediate_context->RSSetState(default_pipeline_state.rasterizer_state);
				immediate_context->OMSetBlendState(default_pipeline_state.blend_state, nullptr, 0xFFFFFFFF);
				immediate_context->OMSetDepthStencilState(default_pipeline_state.depth_stencil_state, default_pipeline_state.stencil_ref);

#if 1
				immediate_context->DrawIndexed(subset.index_count, subset.start_index_location, 0);
#else
				immediate_context->DrawIndexedInstanced(subset.index_count, 1, subset.start_index_location, 0, 0);
#endif
			}
		}
	}

	immediate_context->IASetPrimitiveTopology(cached_topology);
	immediate_context->VSSetShader(cached_vertex_shader.Get(), nullptr, 0);
	immediate_context->DSSetShader(cached_domain_shader.Get(), nullptr, 0);
	immediate_context->HSSetShader(cached_hull_shader.Get(), nullptr, 0);
	immediate_context->GSSetShader(cached_geometry_shader.Get(), nullptr, 0);
	immediate_context->PSSetShader(cached_pixel_shader.Get(), nullptr, 0);
	immediate_context->RSSetState(cached_rasterizer_state.Get());
	immediate_context->OMSetBlendState(cached_blend_state.Get(), nullptr, 0xFFFFFFFF);
	immediate_context->OMSetDepthStencilState(cached_depth_stencil_state.Get(), cached_stencil_ref);
}

void geometric_substance::cast_shadow(ID3D11DeviceContext* immediate_context, const XMFLOAT4X4& world, const animation::keyframe* keyframe)
{
	for (mesh& mesh : meshes)
	{
		uint32_t strides[3] = { sizeof(vertex_position), sizeof(vertex_extra_attribute), sizeof(vertex_bone_influence) };
		uint32_t offsets[3] = { 0, 0, 0 };
		ID3D11Buffer* vertex_buffers[3] =
		{
			mesh.vertex_buffers[0].Get(),
			nullptr,
			mesh.attribute == geometric_attribute::skinnned_mesh ? mesh.vertex_buffers[2].Get() : nullptr
		};
		immediate_context->IASetVertexBuffers(0, static_cast<size_t>(mesh.attribute) + 2, vertex_buffers, strides, offsets);
		immediate_context->IASetIndexBuffer(mesh.index_buffer.Get(), DXGI_FORMAT_R32_UINT, 0);
		immediate_context->IASetInputLayout(input_layouts[static_cast<size_t>(mesh.attribute)].Get());

		constants data;
		if (keyframe && keyframe->nodes.size() > 0)
		{
			const animation::keyframe::node& mesh_node = keyframe->nodes.at(mesh.node_index);
#if 0
			XMStoreFloat4x4(&data.world, XMLoadFloat4x4(&mesh_node.global_transform) * XMLoadFloat4x4(&world));
#else
			XMStoreFloat4x4(&data.world, XMLoadFloat4x4(&mesh.geometric_transform) * XMLoadFloat4x4(&mesh_node.global_transform) * XMLoadFloat4x4(&world));
#endif
			const size_t bone_count{ mesh.bind_pose.bones.size() };
			_ASSERT_EXPR(bone_count < MAX_BONES, L"The value of the 'bone_count' has exceeded MAX_BONES.");

			bone_constants bone_data;
			for (size_t bone_index = 0; bone_index < bone_count; ++bone_index)
			{
				const skeleton::bone& bone{ mesh.bind_pose.bones.at(bone_index) };
				const animation::keyframe::node& bone_node{ keyframe->nodes.at(bone.node_index) };
				XMStoreFloat4x4(&bone_data.bone_transforms[bone_index],
					XMLoadFloat4x4(&bone.offset_transform) *
					XMLoadFloat4x4(&bone_node.global_transform) *
					XMMatrixInverse(nullptr, XMLoadFloat4x4(&mesh_node.global_transform))
				);
			}
			if (mesh.attribute == geometric_attribute::skinnned_mesh)
			{
				immediate_context->UpdateSubresource(constant_buffers[1].Get(), 0, 0, &bone_data, 0, 0);
				immediate_context->VSSetConstantBuffers(1, 1, constant_buffers[1].GetAddressOf());
			}
		}
		else
		{
#if 0
			XMStoreFloat4x4(&data.world, XMLoadFloat4x4(&mesh.default_global_transform) * XMLoadFloat4x4(&world));
#else
			XMStoreFloat4x4(&data.world, XMLoadFloat4x4(&mesh.geometric_transform)/*UNIT.99*/ * XMLoadFloat4x4(&mesh.default_global_transform) * XMLoadFloat4x4(&world));
#endif

#if 1
			if (mesh.attribute == geometric_attribute::skinnned_mesh) 
			{
				// 必要なボーンデータがない場合でも適切に動作するようにするためバインドする
				bone_constants bone_data;
				for (size_t bone_index = 0; bone_index < MAX_BONES; ++bone_index)
				{
					bone_data.bone_transforms[bone_index] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
				}
				immediate_context->UpdateSubresource(constant_buffers[1].Get(), 0, 0, &bone_data, 0, 0);
				immediate_context->VSSetConstantBuffers(1, 1, constant_buffers[1].GetAddressOf());
			}
#endif
		}
		immediate_context->UpdateSubresource(constant_buffers[0].Get(), 0, 0, &data, 0, 0);
		immediate_context->VSSetConstantBuffers(0, 1, constant_buffers[0].GetAddressOf());

		for (const mesh::subset& subset : mesh.subsets)
		{
			immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			immediate_context->VSSetShader(vertex_shaders[static_cast<size_t>(mesh.attribute) + 2].Get(), NULL, 0);
			immediate_context->DSSetShader(NULL, NULL, 0);
			immediate_context->HSSetShader(NULL, NULL, 0);
			immediate_context->GSSetShader(geometry_shader.Get(), NULL, 0);
			immediate_context->PSSetShader(NULL, NULL, 0);
#if 0
			immediate_context->DrawIndexed(subset.index_count, subset.start_index_location, 0);
#else
			immediate_context->DrawIndexedInstanced(subset.index_count, 4, subset.start_index_location, 0, 0);
#endif
		}
	}
	immediate_context->VSSetShader(NULL, NULL, 0);
	immediate_context->GSSetShader(NULL, NULL, 0);

}
//マテリアル取得
void geometric_substance::fetch_materials(FbxScene* fbx_scene, std::unordered_map<uint64_t, material>& materials)
{
	const size_t node_count{ scene_view.nodes.size() };
	for (size_t node_index = 0; node_index < node_count; ++node_index)
	{
		const scene::node& node{ scene_view.nodes.at(node_index) };
		const FbxNode* fbx_node{ fbx_scene->FindNodeByName(node.name.c_str()) };

		const int material_count{ fbx_node->GetMaterialCount() };
		for (int material_index = 0; material_index < material_count; ++material_index)
		{
			const FbxSurfaceMaterial* fbx_material{ fbx_node->GetMaterial(material_index) };

			material material;
			material.name = fbx_material->GetName();
			material.unique_id = fbx_material->GetUniqueID();
			FbxProperty fbx_property;
			fbx_property = fbx_material->FindProperty(FbxSurfaceMaterial::sDiffuse);

			//各プロパティを読み取り、値が有効である場合そのプロパティの値を読み取り、カスタムマテリアル構造に格納する
			if (fbx_property.IsValid())
			{
				const float factor{ fbx_material->FindProperty(FbxSurfaceMaterial::sDiffuseFactor).Get<FbxFloat>() };
				const FbxDouble3 color{ fbx_property.Get<FbxDouble3>() };
				material.diffuse.x = static_cast<float>(color[0]) * factor;
				material.diffuse.y = static_cast<float>(color[1]) * factor;
				material.diffuse.z = static_cast<float>(color[2]) * factor;
				material.diffuse.w = 1.0f;

				const FbxFileTexture* fbx_texture{ fbx_property.GetSrcObject<FbxFileTexture>() };
				material.texture_filenames[0] = fbx_texture ? fbx_texture->GetRelativeFileName() : "";
			}
			fbx_property = fbx_material->FindProperty(FbxSurfaceMaterial::sAmbient);
			if (fbx_property.IsValid())
			{
				const float factor{ fbx_material->FindProperty(FbxSurfaceMaterial::sAmbientFactor).Get<FbxFloat>() };
				const FbxDouble3 color{ fbx_property.Get<FbxDouble3>() };
				material.ambient.x = static_cast<float>(color[0]) * factor;
				material.ambient.y = static_cast<float>(color[1]) * factor;
				material.ambient.z = static_cast<float>(color[2]) * factor;
				material.ambient.w = 1.0f;
			}
			fbx_property = fbx_material->FindProperty(FbxSurfaceMaterial::sSpecular);
			if (fbx_property.IsValid())
			{
				const float factor{ fbx_material->FindProperty(FbxSurfaceMaterial::sSpecularFactor).Get<FbxFloat>() };
				const FbxDouble3 color{ fbx_property.Get<FbxDouble3>() };
				material.specular.x = static_cast<float>(color[0]) * factor;
				material.specular.y = static_cast<float>(color[1]) * factor;
				material.specular.z = static_cast<float>(color[2]) * factor;
				material.specular.w = fbx_material->FindProperty(FbxSurfaceMaterial::sShininess).Get<FbxFloat>();
			}
			fbx_property = fbx_material->FindProperty(FbxSurfaceMaterial::sNormalMap);
			if (fbx_property.IsValid())
			{
				const FbxFileTexture* fbx_texture{ fbx_property.GetSrcObject<FbxFileTexture>() };
				material.texture_filenames[1] = fbx_texture ? fbx_texture->GetRelativeFileName() : "";
			}
			fbx_property = fbx_material->FindProperty(FbxSurfaceMaterial::sReflection);
			if (fbx_property.IsValid())
			{
				float factor = fbx_material->FindProperty(FbxSurfaceMaterial::sReflectionFactor).Get<FbxFloat>();
				const FbxDouble3 color{ fbx_property.Get<FbxDouble3>() };
				material.reflection.x = static_cast<float>(color[0]) * factor;
				material.reflection.y = static_cast<float>(color[1]) * factor;
				material.reflection.z = static_cast<float>(color[2]) * factor;
				material.reflection.w = 1.0f;
			}
			fbx_property = fbx_material->FindProperty(FbxSurfaceMaterial::sEmissive);
			if (fbx_property.IsValid())
			{
				float factor = fbx_material->FindProperty(FbxSurfaceMaterial::sEmissiveFactor).Get<FbxFloat>();
				const FbxDouble3 color{ fbx_property.Get<FbxDouble3>() };
				material.emissive.x = static_cast<float>(color[0]) * factor;
				material.emissive.y = static_cast<float>(color[1]) * factor;
				material.emissive.z = static_cast<float>(color[2]) * factor;
				material.emissive.w = 1.0f;
			}

			materials.emplace(material.unique_id, std::move(material));
		}
	}
#if 1

	materials.emplace();
#endif
}
//ボーン取得
void geometric_substance::fetch_skeleton(FbxMesh* fbx_mesh, skeleton& bind_pose)
{
	const int deformer_count = fbx_mesh->GetDeformerCount(FbxDeformer::eSkin);
	for (int deformer_index = 0; deformer_index < deformer_count; ++deformer_index)
	{
		FbxSkin* skin = static_cast<FbxSkin*>(fbx_mesh->GetDeformer(deformer_index, FbxDeformer::eSkin));
		const int cluster_count = skin->GetClusterCount();
		bind_pose.bones.resize(cluster_count);
		for (int cluster_index = 0; cluster_index < cluster_count; ++cluster_index)
		{
			FbxCluster* cluster = skin->GetCluster(cluster_index);

			skeleton::bone& bone{ bind_pose.bones.at(cluster_index) };
			bone.name = cluster->GetLink()->GetName();
			bone.unique_id = cluster->GetLink()->GetUniqueID();
			bone.parent_index = bind_pose.indexof(cluster->GetLink()->GetParent()->GetUniqueID());
			bone.node_index = scene_view.indexof(bone.unique_id);

			//モデルのローカル空間からグローバル空間への変換.
			FbxAMatrix reference_global_init_position;
			cluster->GetTransformMatrix(reference_global_init_position);

			//ボーンのローカル空間からグローバル空間への変換
			FbxAMatrix cluster_global_init_position;
			cluster->GetTransformLinkMatrix(cluster_global_init_position);

			// 行列はカラムメジャー方式で定義しメッシュ空間からボーン空間へ位置を変換する
			bone.offset_transform = to_xmfloat4x4(cluster_global_init_position.Inverse() * reference_global_init_position);
		}
	}
}
//アニメーション取得
void geometric_substance::fetch_animations(FbxScene* fbx_scene, std::vector<animation>& animation_clips, float sampling_rate /*If this value is 0, the animation data will be sampled at the default frame rate.*/)
{
	//samplingが負数の場合、アニメーションデータをロードしない。
	if (sampling_rate < 0.0f)
	{
		return;
	}

	FbxArray<FbxString*> animation_stack_names;
	fbx_scene->FillAnimStackNameArray(animation_stack_names);
	const int animation_stack_count{ animation_stack_names.GetCount() };
	for (int animation_stack_index = 0; animation_stack_index < animation_stack_count; ++animation_stack_index)
	{
		animation& animation_clip{ animation_clips.emplace_back() };
		animation_clip.name = animation_stack_names[animation_stack_index]->Buffer();

		FbxAnimStack* animation_stack{ fbx_scene->FindMember<FbxAnimStack>(animation_clip.name.c_str()) };
		fbx_scene->SetCurrentAnimationStack(animation_stack);

#if 1
		const FbxTime::EMode time_mode{ fbx_scene->GetGlobalSettings().GetTimeMode() };
		FbxTime one_second;
		one_second.SetTime(0, 0, 1, 0, 0, time_mode);
		animation_clip.sampling_rate = sampling_rate > 0 ? sampling_rate : static_cast<float>(one_second.GetFrameRate(time_mode));
		const FbxTime smapling_step{ static_cast<FbxLongLong>(one_second.Get() / animation_clip.sampling_rate) };

		const FbxTakeInfo* take_info{ fbx_scene->GetTakeInfo(animation_clip.name.c_str()) };
		const FbxTime start_time{ take_info->mLocalTimeSpan.GetStart() };
		const FbxTime stop_time{ take_info->mLocalTimeSpan.GetStop() };
#else
		FbxTime start_time;
		FbxTime stop_time;
		FbxTakeInfo* take_info{ fbx_scene->GetTakeInfo(animation_clip.name.c_str()) };
		if (take_info)
		{
			start_time = take_info->mLocalTimeSpan.GetStart();
			stop_time = take_info->mLocalTimeSpan.GetStop();
		}
		else
		{
			FbxTimeSpan time_line_time_span;
			fbx_scene->GetGlobalSettings().GetTimelineDefaultTimeSpan(time_line_time_span);

			start_time = time_line_time_span.GetStart();
			stop_time = time_line_time_span.GetStop();
		}

		FbxTime::EMode time_mode = fbx_scene->GetGlobalSettings().GetTimeMode();
		FbxTime smapling_step;
		smapling_step.SetTime(0, 0, 1, 0, 0, time_mode);
		animation_clip.sampling_rate = sampling_rate > 0 ? sampling_rate : static_cast<float>(smapling_step.GetFrameRate(time_mode));
		smapling_step = static_cast<FbxLongLong>(smapling_step.Get() * (1.0f / animation_clip.sampling_rate));
#endif // 0
		for (FbxTime time = start_time; time <= stop_time; time += smapling_step)
		{
			animation::keyframe& keyframe{ animation_clip.sequence.emplace_back() };

			const size_t node_count{ scene_view.nodes.size() };
			keyframe.nodes.resize(node_count);
			for (size_t node_index = 0; node_index < node_count; ++node_index)
			{
				FbxNode* fbx_node{ fbx_scene->FindNodeByName(scene_view.nodes.at(node_index).name.c_str()) };
				if (fbx_node)
				{
					animation::keyframe::node& node{ keyframe.nodes.at(node_index) };
					//global_transformは、シーンのグローバル座標系に対するノードの変換行列。
					node.global_transform = to_xmfloat4x4(fbx_node->EvaluateGlobalTransform(time));
					// UNIT.27
					// local_transform' は、親のローカル座標系に対するノードの変換行列。
					const FbxAMatrix& local_transform{ fbx_node->EvaluateLocalTransform(time) };
					node.scaling = to_xmfloat3(local_transform.GetS());
					node.rotation = to_xmfloat4(local_transform.GetQ());
					node.translation = to_xmfloat3(local_transform.GetT());
				}
#if 0
				else
				{
					animation::keyframe::node& keyframe_node{ keyframe.nodes.at(node_index) };
					keyframe_node.global_transform = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
					keyframe_node.translation = { 0, 0, 0 };
					keyframe_node.rotation = { 0, 0, 0, 1 };
					keyframe_node.scaling = { 1, 1, 1 };
				}
#endif
			}
		}
	}
	for (int animation_stack_index = 0; animation_stack_index < animation_stack_count; ++animation_stack_index)
	{
		delete animation_stack_names[animation_stack_index];
	}
}
// アニメーション制御 
void geometric_substance::update_animation(animation::keyframe& keyframe)
{
	const size_t node_count{ keyframe.nodes.size() };
	for (size_t node_index = 0; node_index < node_count; ++node_index)
	{
		animation::keyframe::node& node{ keyframe.nodes.at(node_index) };
		XMMATRIX S{ XMMatrixScaling(node.scaling.x, node.scaling.y, node.scaling.z) };
		XMMATRIX R{ XMMatrixRotationQuaternion(XMLoadFloat4(&node.rotation)) };
		XMMATRIX T{ XMMatrixTranslation(node.translation.x, node.translation.y, node.translation.z) };

		const int64_t parent_index{ scene_view.nodes.at(node_index).parent_index };
		XMMATRIX P{ parent_index < 0 ? XMMatrixIdentity() : XMLoadFloat4x4(&keyframe.nodes.at(parent_index).global_transform) };

		XMStoreFloat4x4(&node.global_transform, S * R * T * P);
	}
}
//アニメーションデータの追加 
bool geometric_substance::append_animations(const char* animation_filename, float sampling_rate)
{
	FbxManager* fbx_manager{ FbxManager::Create() };
	FbxScene* fbx_scene{ FbxScene::Create(fbx_manager, "") };

	FbxImporter* fbx_importer{ FbxImporter::Create(fbx_manager, "") };
	bool import_status{ false };
	import_status = fbx_importer->Initialize(animation_filename);
	_ASSERT_EXPR_A(import_status, fbx_importer->GetStatus().GetErrorString());
	import_status = fbx_importer->Import(fbx_scene);
	_ASSERT_EXPR_A(import_status, fbx_importer->GetStatus().GetErrorString());

	fetch_animations(fbx_scene, animation_clips, sampling_rate/*0:デフォルト値を使用、0未満:取得しない*/);

	fbx_manager->Destroy();

	return true;
}

//アニメーションブレンド
void geometric_substance::blend_animations(const animation::keyframe* keyframes[2], float factor, animation::keyframe& keyframe)
{
	_ASSERT_EXPR(keyframes[0]->nodes.size() == keyframes[1]->nodes.size(), "The size of the two node arrays must be the same.");

	size_t node_count{ keyframes[0]->nodes.size() };
	keyframe.nodes.resize(node_count);
	for (size_t node_index = 0; node_index < node_count; ++node_index)
	{
		XMVECTOR S[2]{ XMLoadFloat3(&keyframes[0]->nodes.at(node_index).scaling), XMLoadFloat3(&keyframes[1]->nodes.at(node_index).scaling) };
		XMStoreFloat3(&keyframe.nodes.at(node_index).scaling, XMVectorLerp(S[0], S[1], factor));

		XMVECTOR R[2]{ XMLoadFloat4(&keyframes[0]->nodes.at(node_index).rotation), XMLoadFloat4(&keyframes[1]->nodes.at(node_index).rotation) };
		XMStoreFloat4(&keyframe.nodes.at(node_index).rotation, XMQuaternionSlerp(R[0], R[1], factor));

		XMVECTOR T[2]{ XMLoadFloat3(&keyframes[0]->nodes.at(node_index).translation), XMLoadFloat3(&keyframes[1]->nodes.at(node_index).translation) };
		XMStoreFloat3(&keyframe.nodes.at(node_index).translation, XMVectorLerp(T[0], T[1], factor));
	}
}


std::unordered_map<std::string, std::shared_ptr<geometric_substance>> geometric_substance::_geometric_substances;
std::mutex geometric_substance::_mutex;
