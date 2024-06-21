#include "shader.h"

//シェーダーオブジェクトの管理を効率化するために、テンプレート関数を使用しシェーダーの作成を行っている
template <>
HRESULT _create<ID3D11PixelShader>(ID3D11Device* device, const blob& cso, ID3D11PixelShader** shader)
{
	return device->CreatePixelShader(cso.data.get(), cso.size, NULL, shader);
};
template <>
HRESULT _create<ID3D11HullShader>(ID3D11Device* device, const blob& cso, ID3D11HullShader** shader)
{
	return device->CreateHullShader(cso.data.get(), cso.size, NULL, shader);
};
template <>
HRESULT _create<ID3D11DomainShader>(ID3D11Device* device, const blob& cso, ID3D11DomainShader** shader)
{
	return device->CreateDomainShader(cso.data.get(), cso.size, NULL, shader);
};
template <>
HRESULT _create<ID3D11GeometryShader>(ID3D11Device* device, const blob& cso, ID3D11GeometryShader** shader)
{
	return device->CreateGeometryShader(cso.data.get(), cso.size, NULL, shader);
};
template <>
HRESULT _create<ID3D11ComputeShader>(ID3D11Device* device, const blob& cso, ID3D11ComputeShader** shader)
{
	return device->CreateComputeShader(cso.data.get(), cso.size, NULL, shader);
};

std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11VertexShader>> shader<ID3D11VertexShader>::_vertex_shaders;
std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11InputLayout>> shader<ID3D11VertexShader>::_input_layouts;
std::mutex shader<ID3D11VertexShader>::_mutex;

