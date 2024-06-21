
#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <cstdint>

//'�s�N�Z���V�F�[�_�ƃT���v���̏�Ԃ������Ă��Ȃ��̂�
//�����Ńs�N�Z���V�F�[�_�ƃT���v���̏�Ԃ�����Đݒ肷��K�v������
class fullscreen_quad
{
public:
	fullscreen_quad(ID3D11Device *device);
	virtual ~fullscreen_quad() = default;

private:
	Microsoft::WRL::ComPtr<ID3D11VertexShader> embedded_vertex_shader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> embedded_pixel_shader;

public:
	void blit(ID3D11DeviceContext *immediate_contextbool, ID3D11ShaderResourceView* const* shader_resource_views, uint32_t start_slot, uint32_t num_views, ID3D11PixelShader* replaced_pixel_shader = nullptr);
};
