#include "fullscreen_quad.h"
#include "shader.h"
#include "misc.h"

fullscreen_quad::fullscreen_quad(ID3D11Device *device)
{
	embedded_vertex_shader = shader<ID3D11VertexShader>::_emplace(device, "fullscreen_quad_vs.cso", NULL, NULL, 0);
	embedded_pixel_shader = shader<ID3D11PixelShader>::_emplace(device, "fullscreen_quad_ps.cso");
}
//����`�摀���G�t�F�N�g���ꎞ�I�ɓK�p����K�v������ꍇ�A���݂̕`���Ԃ��ێ����A���̏�Ԃɖ߂����Ƃ��ł���
//�Ǘ����ȒP�ɂȂ�A�_��ȃG�t�F�N�g�̓K�p���\�ɂȂ�
void fullscreen_quad::blit(ID3D11DeviceContext *immediate_context, ID3D11ShaderResourceView* const* shader_resource_views, uint32_t start_slot, uint32_t num_views, ID3D11PixelShader* replaced_pixel_shader)
{
	//���̃V�F�[�_�[���\�[�X�r���[�̏�Ԃ�ۑ�
	ID3D11ShaderResourceView* cached_shader_resource_views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
	immediate_context->PSGetShaderResources(start_slot, num_views, cached_shader_resource_views);
	//�V�����V�F�[�_�[���\�[�X�r���[���s�N�Z���V�F�[�_�[�ɐݒ�
	immediate_context->PSSetShaderResources(start_slot, num_views, shader_resource_views);
	//���̓A�Z���u���̐ݒ�
	immediate_context->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	immediate_context->IASetInputLayout(NULL);

	immediate_context->VSSetShader(embedded_vertex_shader.Get(), 0, 0);
	replaced_pixel_shader ? immediate_context->PSSetShader(replaced_pixel_shader, 0, 0) : immediate_context->PSSetShader(embedded_pixel_shader.Get(), 0, 0);
	immediate_context->PSSetShaderResources(start_slot, num_views, shader_resource_views);

	immediate_context->Draw(4, 0);

	// ���̃V�F�[�_�[���\�[�X�r���[�̕���
	immediate_context->PSSetShaderResources(start_slot, num_views, cached_shader_resource_views);
	for (ID3D11ShaderResourceView* cached_shader_resource_view : cached_shader_resource_views)
	{
		if (cached_shader_resource_view) cached_shader_resource_view->Release();
	}
}
