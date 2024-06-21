#include "husk_particles.h"

#include <random>
#include "shader.h"
#include "misc.h"

using namespace DirectX;

husk_particles::husk_particles(ID3D11Device* device, size_t max_particle_count) : max_particle_count(max_particle_count)
{
	HRESULT hr{ S_OK };

	D3D11_BUFFER_DESC buffer_desc{};

	buffer_desc.ByteWidth = static_cast<UINT>(sizeof(husk_particle) * max_particle_count);
	buffer_desc.StructureByteStride = sizeof(husk_particle);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	buffer_desc.CPUAccessFlags = 0;
	buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	hr = device->CreateBuffer(&buffer_desc, NULL, husk_particle_buffer.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	buffer_desc.ByteWidth = static_cast<UINT>(sizeof(particle) * max_particle_count);
	buffer_desc.StructureByteStride = sizeof(particle);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	buffer_desc.CPUAccessFlags = 0;
	buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	hr = device->CreateBuffer(&buffer_desc, NULL, updated_particle_buffer.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	buffer_desc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * 1);
	buffer_desc.StructureByteStride = sizeof(uint32_t);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	buffer_desc.CPUAccessFlags = 0;
	buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	hr = device->CreateBuffer(&buffer_desc, NULL, completed_particle_counter_buffer.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	buffer_desc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * 32);
	buffer_desc.StructureByteStride = sizeof(uint32_t);
	buffer_desc.Usage = D3D11_USAGE_STAGING;
	buffer_desc.BindFlags = 0;
	buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	buffer_desc.MiscFlags = 0;
	hr = device->CreateBuffer(&buffer_desc, NULL, copied_structure_count_buffer.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));



	D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc;
	shader_resource_view_desc.Format = DXGI_FORMAT_UNKNOWN;
	shader_resource_view_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	shader_resource_view_desc.Buffer.ElementOffset = 0;
	shader_resource_view_desc.Buffer.NumElements = static_cast<UINT>(max_particle_count);
	hr = device->CreateShaderResourceView(updated_particle_buffer.Get(), &shader_resource_view_desc, updated_particle_buffer_srv.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));


	// husk_particle_buffer 用の UAV 作成
	D3D11_UNORDERED_ACCESS_VIEW_DESC unordered_access_view_desc;
	unordered_access_view_desc.Format = DXGI_FORMAT_UNKNOWN;
	unordered_access_view_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	unordered_access_view_desc.Buffer.FirstElement = 0;
	unordered_access_view_desc.Buffer.NumElements = static_cast<UINT>(max_particle_count);
	unordered_access_view_desc.Buffer.Flags = 0;
	hr = device->CreateUnorderedAccessView(husk_particle_buffer.Get(), &unordered_access_view_desc, husk_particle_buffer_uav.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	unordered_access_view_desc.Format = DXGI_FORMAT_UNKNOWN;
	unordered_access_view_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	unordered_access_view_desc.Buffer.FirstElement = 0;
	unordered_access_view_desc.Buffer.NumElements = static_cast<UINT>(max_particle_count);
	unordered_access_view_desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;
	hr = device->CreateUnorderedAccessView(husk_particle_buffer.Get(), &unordered_access_view_desc, husk_particle_append_buffer_uav.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	unordered_access_view_desc.Format = DXGI_FORMAT_UNKNOWN;
	unordered_access_view_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	unordered_access_view_desc.Buffer.FirstElement = 0;
	unordered_access_view_desc.Buffer.NumElements = static_cast<UINT>(max_particle_count);
	unordered_access_view_desc.Buffer.Flags = 0;
	hr = device->CreateUnorderedAccessView(updated_particle_buffer.Get(), &unordered_access_view_desc, updated_particle_buffer_uav.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	unordered_access_view_desc.Format = DXGI_FORMAT_UNKNOWN;
	unordered_access_view_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	unordered_access_view_desc.Buffer.FirstElement = 0;
	unordered_access_view_desc.Buffer.NumElements = 1;
	unordered_access_view_desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER;
	hr = device->CreateUnorderedAccessView(completed_particle_counter_buffer.Get(), &unordered_access_view_desc, completed_particle_counter_buffer_uav.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	buffer_desc.ByteWidth = sizeof(particle_constants);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	buffer_desc.CPUAccessFlags = 0;
	buffer_desc.MiscFlags = 0;
	buffer_desc.StructureByteStride = 0;
	hr = device->CreateBuffer(&buffer_desc, NULL, constant_buffer.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	vertex_shader = shader<ID3D11VertexShader>::_emplace(device, "husk_particles_vs.cso", NULL, NULL, 0);
	pixel_shader = shader<ID3D11PixelShader>::_emplace(device, "husk_particles_ps.cso");
	geometry_shader = shader<ID3D11GeometryShader>::_emplace(device, "husk_particles_gs.cso");
	compute_shader = shader<ID3D11ComputeShader>::_emplace(device, "husk_particles_cs.cso");
	// その他のシェーダーのロード
	copy_buffer_cs = shader<ID3D11ComputeShader>::_emplace(device, "husk_particles_copy_buffer_cs.cso");
	accumulate_husk_particles_ps = shader<ID3D11PixelShader>::_emplace(device, "accumulate_husk_particles_ps.cso");
}

static UINT align(UINT num, UINT alignment)
{
	return (num + (alignment - 1)) & ~(alignment - 1);
}
void husk_particles::integrate(ID3D11DeviceContext* immediate_context, float delta_time)
{
	HRESULT hr{ S_OK };
	// パーティクル数が最大値を超えていないことを確認し、0であるか状態が5以上であれば処理をスキップ
	_ASSERT_EXPR(particle_data.particle_count <= max_particle_count, L"");
	if (particle_data.particle_count == 0 || state >= 5)
	{
		return;
	}
	// コンピュートシェーダーで使用するUAVを設定
	immediate_context->CSSetUnorderedAccessViews(0, 1, husk_particle_buffer_uav.GetAddressOf(), NULL);
	immediate_context->CSSetUnorderedAccessViews(1, 1, updated_particle_buffer_uav.GetAddressOf(), NULL);
	UINT initial_count{ transitioned_particle_count };
	immediate_context->CSSetUnorderedAccessViews(2, 1, completed_particle_counter_buffer_uav.GetAddressOf(), &initial_count);
	// 定数バッファを更新し、コンピュートシェーダーに設定
	immediate_context->UpdateSubresource(constant_buffer.Get(), 0, 0, &particle_data, 0, 0);
	immediate_context->CSSetConstantBuffers(9, 1, constant_buffer.GetAddressOf());

	immediate_context->CSSetShader(compute_shader.Get(), NULL, 0);
	// パーティクル数に基づいてディスパッチ
	UINT num_threads = align(particle_data.particle_count, 256);
	immediate_context->Dispatch(num_threads / 256, 1, 1);
	// UAVを解除
	ID3D11UnorderedAccessView* null_unordered_access_view{};
	immediate_context->CSSetUnorderedAccessViews(0, 1, &null_unordered_access_view, NULL);
	immediate_context->CSSetUnorderedAccessViews(1, 1, &null_unordered_access_view, NULL);
	immediate_context->CSSetUnorderedAccessViews(2, 1, &null_unordered_access_view, NULL);
	// 完了したパーティクルのカウントをバッファにコピーし、読み取り
	immediate_context->CopyStructureCount(copied_structure_count_buffer.Get(), sizeof(uint32_t) * 1, completed_particle_counter_buffer_uav.Get());
	D3D11_MAPPED_SUBRESOURCE mapped_subresource{};
	hr = immediate_context->Map(copied_structure_count_buffer.Get(), 0, D3D11_MAP_READ, 0, &mapped_subresource);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	transitioned_particle_count = reinterpret_cast<uint32_t*>(mapped_subresource.pData)[1];
	immediate_context->Unmap(copied_structure_count_buffer.Get(), 0);
	// 状態を更新
	state = transitioned_particle_count / particle_data.particle_count;
}

void husk_particles::render(ID3D11DeviceContext* immediate_context)
{
	// パーティクル数が最大値を超えていないことを確認し、0であればレンダリングをスキップ
	_ASSERT_EXPR(particle_data.particle_count <= max_particle_count, L"");
	if (particle_data.particle_count == 0)
	{
		return;
	}

	immediate_context->VSSetShader(vertex_shader.Get(), NULL, 0);
	immediate_context->PSSetShader(pixel_shader.Get(), NULL, 0);
	immediate_context->GSSetShader(geometry_shader.Get(), NULL, 0);
	immediate_context->GSSetShaderResources(9, 1, updated_particle_buffer_srv.GetAddressOf());
	// 定数バッファを更新し、各シェーダーステージに設定
	immediate_context->UpdateSubresource(constant_buffer.Get(), 0, 0, &particle_data, 0, 0);
	immediate_context->VSSetConstantBuffers(9, 1, constant_buffer.GetAddressOf());
	immediate_context->PSSetConstantBuffers(9, 1, constant_buffer.GetAddressOf());
	immediate_context->GSSetConstantBuffers(9, 1, constant_buffer.GetAddressOf());

	immediate_context->IASetInputLayout(NULL);
	immediate_context->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	immediate_context->IASetIndexBuffer(NULL, DXGI_FORMAT_R32_UINT, 0);
	immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

	immediate_context->Draw(static_cast<UINT>(particle_data.particle_count), 0);
	// 設定したシェーダーリソースビューを解除
	ID3D11ShaderResourceView* null_shader_resource_view{};
	immediate_context->GSSetShaderResources(9, 1, &null_shader_resource_view);
	immediate_context->VSSetShader(NULL, NULL, 0);
	immediate_context->PSSetShader(NULL, NULL, 0);
	immediate_context->GSSetShader(NULL, NULL, 0);
}
//
void husk_particles::amass_husk_particles(ID3D11DeviceContext* immediate_context, std::function<void(ID3D11PixelShader*)> drawcallback)
{
	HRESULT hr{ S_OK };
	//現在のレンダーターゲットビューと深度ステンシルビュー、及び順序なしアクセスビューの状態を保存
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> cached_render_target_view;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> cached_depth_stencil_view;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> cached_unordered_access_view;
	immediate_context->OMGetRenderTargetsAndUnorderedAccessViews(
		1, cached_render_target_view.GetAddressOf(), cached_depth_stencil_view.GetAddressOf(),
		1, 1, cached_unordered_access_view.GetAddressOf()
	);
	//新しいレンダーターゲットビューと順序なしアクセスビューを設定
	ID3D11RenderTargetView* nulll_render_target_view{};
	UINT initial_count{ 0 };
	immediate_context->OMSetRenderTargetsAndUnorderedAccessViews(
		1, &nulll_render_target_view, NULL,
		1, 1, husk_particle_append_buffer_uav.GetAddressOf(), &initial_count
	);

	drawcallback(accumulate_husk_particles_ps.Get());
	// 保存したレンダーターゲットビューと順序なしアクセスビューの状態を復元
	immediate_context->OMSetRenderTargetsAndUnorderedAccessViews(
		1, cached_render_target_view.GetAddressOf(), cached_depth_stencil_view.Get(),
		1, 1, cached_unordered_access_view.GetAddressOf(), NULL
	);
	// UAV の構造カウントを特定のバッファにコピー
	immediate_context->CopyStructureCount(copied_structure_count_buffer.Get(), 0, husk_particle_append_buffer_uav.Get());
	D3D11_MAPPED_SUBRESOURCE mapped_subresource{};
	hr = immediate_context->Map(copied_structure_count_buffer.Get(), 0, D3D11_MAP_READ, 0, &mapped_subresource);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	particle_data.particle_count = reinterpret_cast<uint32_t*>(mapped_subresource.pData)[0];
	immediate_context->Unmap(copied_structure_count_buffer.Get(), 0);

	reset(immediate_context);
}
void husk_particles::reset(ID3D11DeviceContext* immediate_context)
{
	state = 0;
	transitioned_particle_count = 0;

	immediate_context->CSSetUnorderedAccessViews(0, 1, husk_particle_buffer_uav.GetAddressOf(), NULL);
	immediate_context->CSSetUnorderedAccessViews(1, 1, updated_particle_buffer_uav.GetAddressOf(), NULL);
	UINT initial_count{ transitioned_particle_count };
	immediate_context->CSSetUnorderedAccessViews(2, 1, completed_particle_counter_buffer_uav.GetAddressOf(), &initial_count);

	immediate_context->UpdateSubresource(constant_buffer.Get(), 0, 0, &particle_data, 0, 0);
	immediate_context->CSSetConstantBuffers(9, 1, constant_buffer.GetAddressOf());

	immediate_context->CSSetShader(copy_buffer_cs.Get(), NULL, 0);

	UINT num_threads = align(particle_data.particle_count, 256);
	immediate_context->Dispatch(num_threads / 256, 1, 1);

	ID3D11UnorderedAccessView* null_unordered_access_view{};
	immediate_context->CSSetUnorderedAccessViews(0, 1, &null_unordered_access_view, NULL);
	immediate_context->CSSetUnorderedAccessViews(1, 1, &null_unordered_access_view, NULL);
	immediate_context->CSSetUnorderedAccessViews(2, 1, &null_unordered_access_view, NULL);
}