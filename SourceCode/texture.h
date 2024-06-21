#pragma once

#include <d3d11.h>
#include <wrl.h>

#include <string>
#include<unordered_map>
#include <mutex>

class texture
{
public:
	static std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> _textures;
	static std::mutex _mutex;

#if 0
	static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> _at(const wchar_t* name) { return _textures.at(name); }
#endif
	static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> _emplace(ID3D11Device* device, const wchar_t* name);
	static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> _emplace(ID3D11Device* device, DWORD value/*0xAABBGGRR*/, UINT dimension);

	static void _exterminate()
	{
		std::lock_guard<std::mutex> lock(_mutex);
		_textures.clear();
	}
	static D3D11_TEXTURE2D_DESC _texture2d_desc(ID3D11ShaderResourceView* shader_resource_view);
};
