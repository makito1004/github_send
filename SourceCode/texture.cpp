
#include "texture.h"

#include <string>
#include <map>
using namespace std;
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <WICTextureLoader.h>
#include <DDSTextureLoader.h>

#include "misc.h"

std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> texture::_textures;
std::mutex texture::_mutex;

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture::_emplace(ID3D11Device* device, const wchar_t* name)
{
	if (_textures.find(name) != _textures.end())
	{
		return _textures.at(name);
	}

	HRESULT hr{ S_OK };

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view;
	Microsoft::WRL::ComPtr<ID3D11Resource> resource;
	
	std::filesystem::path dds_filename(name);
	dds_filename.replace_extension("dds");
	if (std::filesystem::exists(dds_filename.c_str()))
	{
		hr = DirectX::CreateDDSTextureFromFile(device, dds_filename.c_str(), resource.GetAddressOf(), shader_resource_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}
	else
	{
		hr = DirectX::CreateWICTextureFromFile(device, name, resource.GetAddressOf(), shader_resource_view.GetAddressOf());
		//_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

	std::lock_guard<std::mutex> lock(_mutex);
	_textures.emplace(std::make_pair(name, shader_resource_view));

	return shader_resource_view;
}

#include <array>
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture::_emplace(ID3D11Device* device, DWORD value/*0xAABBGGRR*/, UINT dimension)
{
	// テクスチャ名の生成
	std::wstringstream name;
	name << setw(8) << setfill(L'0') << hex << uppercase << value << L"." << dec << dimension;
	// 既に同じ名前のテクスチャが存在する場合はそれを返す
	if (_textures.find(name.str().c_str()) != _textures.end())
	{
		return _textures.at(name.str().c_str());
	}
	// テクスチャの設定
	D3D11_TEXTURE2D_DESC texture2d_desc = {};
	texture2d_desc.Width = dimension;
	texture2d_desc.Height = dimension;
	texture2d_desc.MipLevels = 1;
	texture2d_desc.ArraySize = 1;
	texture2d_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texture2d_desc.SampleDesc.Count = 1;
	texture2d_desc.SampleDesc.Quality = 0;
	texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
	texture2d_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	texture2d_desc.CPUAccessFlags = 0;
	texture2d_desc.MiscFlags = 0;

	size_t size = static_cast<size_t>(dimension) * dimension;
	std::vector<DWORD> sysmem(size);
	std::fill(sysmem.begin(), sysmem.end(), value);

	HRESULT hr;

	D3D11_SUBRESOURCE_DATA subresource_data = {};
	subresource_data.pSysMem = sysmem.data();
	subresource_data.SysMemPitch = sizeof(DWORD) * dimension;
	subresource_data.SysMemSlicePitch = 0;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2d;
	hr = device->CreateTexture2D(&texture2d_desc, &subresource_data, &texture2d);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	// シェーダーリソースビューの作成
	D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc = {};
	shader_resource_view_desc.Format = texture2d_desc.Format;
	shader_resource_view_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	shader_resource_view_desc.Texture2D.MipLevels = 1;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view;
	hr = device->CreateShaderResourceView(texture2d.Get(), &shader_resource_view_desc, shader_resource_view.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	//マルチスレッドセーフに登録
	std::lock_guard<std::mutex> lock(_mutex);
	_textures.emplace(std::make_pair(name.str().c_str(), shader_resource_view));

	return shader_resource_view;
}

D3D11_TEXTURE2D_DESC texture::_texture2d_desc(ID3D11ShaderResourceView* shader_resource_view)
{
	Microsoft::WRL::ComPtr<ID3D11Resource> resource;
	shader_resource_view->GetResource(resource.GetAddressOf());

	D3D11_TEXTURE2D_DESC texture2d_desc;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2d;
	HRESULT hr = resource.Get()->QueryInterface<ID3D11Texture2D>(texture2d.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	texture2d->GetDesc(&texture2d_desc);

	return texture2d_desc;
}


