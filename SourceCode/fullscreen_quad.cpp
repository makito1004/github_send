#include "fullscreen_quad.h"
#include "shader.h"
#include "misc.h"

fullscreen_quad::fullscreen_quad(ID3D11Device *device)
{
	embedded_vertex_shader = shader<ID3D11VertexShader>::_emplace(device, "fullscreen_quad_vs.cso", NULL, NULL, 0);
	embedded_pixel_shader = shader<ID3D11PixelShader>::_emplace(device, "fullscreen_quad_ps.cso");
}
//ある描画操作やエフェクトを一時的に適用する必要がある場合、現在の描画状態を維持しつつ、元の状態に戻すことができる
//管理が簡単になり、柔軟なエフェクトの適用が可能になる
void fullscreen_quad::blit(ID3D11DeviceContext *immediate_context, ID3D11ShaderResourceView* const* shader_resource_views, uint32_t start_slot, uint32_t num_views, ID3D11PixelShader* replaced_pixel_shader)
{
	//元のシェーダーリソースビューの状態を保存
	ID3D11ShaderResourceView* cached_shader_resource_views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
	immediate_context->PSGetShaderResources(start_slot, num_views, cached_shader_resource_views);
	//新しいシェーダーリソースビューをピクセルシェーダーに設定
	immediate_context->PSSetShaderResources(start_slot, num_views, shader_resource_views);
	//入力アセンブリの設定
	immediate_context->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	immediate_context->IASetInputLayout(NULL);

	immediate_context->VSSetShader(embedded_vertex_shader.Get(), 0, 0);
	replaced_pixel_shader ? immediate_context->PSSetShader(replaced_pixel_shader, 0, 0) : immediate_context->PSSetShader(embedded_pixel_shader.Get(), 0, 0);
	immediate_context->PSSetShaderResources(start_slot, num_views, shader_resource_views);

	immediate_context->Draw(4, 0);

	// 元のシェーダーリソースビューの復元
	immediate_context->PSSetShaderResources(start_slot, num_views, cached_shader_resource_views);
	for (ID3D11ShaderResourceView* cached_shader_resource_view : cached_shader_resource_views)
	{
		if (cached_shader_resource_view) cached_shader_resource_view->Release();
	}
}
