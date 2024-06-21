#include "bloom.h"
#include "shader.h"
#include "texture.h"
#include "misc.h"

bloom::bloom(ID3D11Device* device, uint32_t width, uint32_t height) : fullscreen_quad(device)
{
	glow_extraction = std::make_unique<framebuffer>(device, width, height, framebuffer::usage::color);
	for (size_t downsampled_index = 0; downsampled_index < downsampled_count; ++downsampled_index)
	{
		gaussian_blur[downsampled_index][0] = std::make_unique<framebuffer>(device, width >> downsampled_index, height >> downsampled_index, framebuffer::usage::color);
		gaussian_blur[downsampled_index][1] = std::make_unique<framebuffer>(device, width >> downsampled_index, height >> downsampled_index, framebuffer::usage::color);
	}
	glow_extraction_ps = shader<ID3D11PixelShader>::_emplace(device, "glow_extraction_ps.cso");
	gaussian_blur_horizontal_ps = shader<ID3D11PixelShader>::_emplace(device, "gaussian_blur_horizontal_ps.cso");
	gaussian_blur_vertical_ps = shader<ID3D11PixelShader>::_emplace(device, "gaussian_blur_vertical_ps.cso");
	gaussian_blur_upsampling_ps = shader<ID3D11PixelShader>::_emplace(device, "gaussian_blur_upsampling_ps.cso");
	gaussian_blur_downsampling_ps = shader<ID3D11PixelShader>::_emplace(device, "gaussian_blur_downsampling_ps.cso");
}

void bloom::make(ID3D11DeviceContext* immediate_context, ID3D11ShaderResourceView* color_map)
{
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> null_shader_resource_view;

	//明るい色を抽出する
	glow_extraction->clear(immediate_context, framebuffer::usage::color, { 0, 0, 0, 1 });
	glow_extraction->activate(immediate_context);
	fullscreen_quad::blit(immediate_context, &color_map, 0, 1, glow_extraction_ps.Get());
	glow_extraction->deactivate(immediate_context);

	//Gaussian blur　サンプル
	//http://rastergrid.com/blog/2010/09/efficient-gaussian-blur-with-linear-sampling/
	
	// downsampling
	gaussian_blur[0][0]->clear(immediate_context, framebuffer::usage::color, { 0, 0, 0, 1 });
	gaussian_blur[0][0]->activate(immediate_context);
	fullscreen_quad::blit(immediate_context, glow_extraction->color_map().GetAddressOf(), 0, 1, gaussian_blur_downsampling_ps.Get());
	gaussian_blur[0][0]->deactivate(immediate_context);

	// ping-pong gaussian blur
	gaussian_blur[0][1]->clear(immediate_context, framebuffer::usage::color, { 0, 0, 0, 1 });
	gaussian_blur[0][1]->activate(immediate_context);
	fullscreen_quad::blit(immediate_context, gaussian_blur[0][0]->color_map().GetAddressOf(), 0, 1, gaussian_blur_horizontal_ps.Get());
	gaussian_blur[0][1]->deactivate(immediate_context);

	gaussian_blur[0][0]->clear(immediate_context, framebuffer::usage::color, { 0, 0, 0, 1 });
	gaussian_blur[0][0]->activate(immediate_context);
	fullscreen_quad::blit(immediate_context, gaussian_blur[0][1]->color_map().GetAddressOf(), 0, 1, gaussian_blur_vertical_ps.Get());
	gaussian_blur[0][0]->deactivate(immediate_context);

	for (size_t downsampled_index = 1; downsampled_index < downsampled_count; ++downsampled_index)
	{
		// downsampling
		gaussian_blur[downsampled_index][0]->clear(immediate_context, framebuffer::usage::color, { 0, 0, 0, 1 });
		gaussian_blur[downsampled_index][0]->activate(immediate_context);
		fullscreen_quad::blit(immediate_context, gaussian_blur[downsampled_index - 1][0]->color_map().GetAddressOf(), 0, 1, gaussian_blur_downsampling_ps.Get());
		gaussian_blur[downsampled_index][0]->deactivate(immediate_context);

		// ping-pong gaussian blur
		gaussian_blur[downsampled_index][1]->clear(immediate_context, framebuffer::usage::color, { 0, 0, 0, 1 });
		gaussian_blur[downsampled_index][1]->activate(immediate_context);
		fullscreen_quad::blit(immediate_context, gaussian_blur[downsampled_index][0]->color_map().GetAddressOf(), 0, 1, gaussian_blur_horizontal_ps.Get());
		gaussian_blur[downsampled_index][1]->deactivate(immediate_context);

		gaussian_blur[downsampled_index][0]->clear(immediate_context, framebuffer::usage::color, { 0, 0, 0, 1 });
		gaussian_blur[downsampled_index][0]->activate(immediate_context);
		fullscreen_quad::blit(immediate_context, gaussian_blur[downsampled_index][1]->color_map().GetAddressOf(), 0, 1, gaussian_blur_vertical_ps.Get());
		gaussian_blur[downsampled_index][0]->deactivate(immediate_context);
	}
}

void bloom::blit(ID3D11DeviceContext* immediate_context)
{
	std::vector<ID3D11ShaderResourceView*> shader_resource_views;
	for (size_t downsampled_index = 0; downsampled_index < downsampled_count; ++downsampled_index)
	{
		shader_resource_views.push_back(gaussian_blur[downsampled_index][0]->color_map().Get());
	}
	fullscreen_quad::blit(immediate_context, shader_resource_views.data(), 0, downsampled_count, gaussian_blur_upsampling_ps.Get());
}

