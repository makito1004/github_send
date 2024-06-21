#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <string>
#include <d3dcompiler.h>
#include <unordered_map> 
#include <cassert>
#include <memory>
#include <mutex>

#include "misc.h"

struct blob
{
	std::unique_ptr<unsigned char[]> data;
	long size;
	blob(const char* name)
	{
		FILE* fp = NULL;
		fopen_s(&fp, name, "rb");
		_ASSERT_EXPR(fp, L"cso file not found");

		fseek(fp, 0, SEEK_END);
		size = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		data = std::make_unique<unsigned char[]>(size);
		fread(data.get(), size, 1, fp);
		fclose(fp);
	}
};

template <class T>
HRESULT _create(ID3D11Device* device, const blob& cso, T** pixel_shader);
template <>
HRESULT _create<ID3D11PixelShader>(ID3D11Device* device, const blob& cso, ID3D11PixelShader** shader);
template <>
HRESULT _create<ID3D11HullShader>(ID3D11Device* device, const blob& cso, ID3D11HullShader** shader);
template <>
HRESULT _create<ID3D11DomainShader>(ID3D11Device* device, const blob& cso, ID3D11DomainShader** shader);
template <>
HRESULT _create<ID3D11GeometryShader>(ID3D11Device* device, const blob& cso, ID3D11GeometryShader** shader);
template <>
HRESULT _create<ID3D11ComputeShader>(ID3D11Device* device, const blob& cso, ID3D11ComputeShader** shader);
template <class T>
class shader
{
	static std::unordered_map<std::string, Microsoft::WRL::ComPtr<T>> _shaders;
	static std::mutex _mutex;

public:
	static Microsoft::WRL::ComPtr<T> _emplace(ID3D11Device* device, const char* name)
	{
		Microsoft::WRL::ComPtr<T> shader;
		if (_shaders.find(name) != _shaders.end())
		{
			shader = _shaders.at(name);
		}
		else
		{
			blob cso(name);
			HRESULT hr = _create<T>(device, cso, shader.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

			std::lock_guard<std::mutex> lock(_mutex);
			_shaders.emplace(std::make_pair(name, shader));
		}
		return shader;
	}
	static void _exterminate()
	{
		std::lock_guard<std::mutex> lock(_mutex);
		_shaders.clear();
	}
};

template <class T>
std::unordered_map<std::string, Microsoft::WRL::ComPtr<T>> shader<T>::_shaders;
template <class T>
std::mutex shader<T>::_mutex;

#include <sstream>

template<>
class shader<ID3D11VertexShader> {
	static std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11VertexShader>> _vertex_shaders;
	static std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11InputLayout>> _input_layouts;
	static std::mutex _mutex;

public:	
	static Microsoft::WRL::ComPtr<ID3D11VertexShader> _emplace(ID3D11Device* device, const char* name, ID3D11InputLayout** input_layout, D3D11_INPUT_ELEMENT_DESC* input_element_desc, UINT num_elements)
	{
		Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shader;
		if (_vertex_shaders.find(name) != _vertex_shaders.end())
		{
			vertex_shader = _vertex_shaders.at(name);
			if (input_layout)
			{
				if (_input_layouts.find(name) != _input_layouts.end())
				{
					*input_layout = _input_layouts.at(name).Get();
					(*input_layout)->AddRef();
				}
				else
				{
#if 1
					_auto_generate_input_layout(device, name, input_layout, input_element_desc, num_elements);
#else
					_ASSERT_EXPR(FALSE, L"");
#endif
				}

			}
		}
		else
		{
			HRESULT hr;

			blob cso(name);
			hr = device->CreateVertexShader(cso.data.get(), cso.size, nullptr, vertex_shader.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		
			std::lock_guard<std::mutex> lock(_mutex);
			_vertex_shaders.emplace(std::make_pair(name, vertex_shader));

			if (input_layout)
			{
				hr = device->CreateInputLayout(input_element_desc, num_elements, cso.data.get(), cso.size, input_layout);
				_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
				_input_layouts.emplace(std::make_pair(name, *input_layout));
			}
		}
		return vertex_shader;
	}
	static HRESULT _auto_generate_input_layout(ID3D11Device* device, const char* name, ID3D11InputLayout** input_layout, D3D11_INPUT_ELEMENT_DESC* input_element_desc, size_t num_elements)
	{
		if (_input_layouts.find(name) != _input_layouts.end())
		{
			*input_layout = _input_layouts.at(name).Get();
			(*input_layout)->AddRef();
			return S_OK;
		}

		std::string source;
		source.append("struct VS_IN\n{\n");
		for (size_t line_number = 0; line_number < num_elements; line_number++)
		{
			std::stringstream line;
			switch (input_element_desc[line_number].Format)
			{
			case DXGI_FORMAT_R32_FLOAT:
			case DXGI_FORMAT_R32G32_FLOAT:
			case DXGI_FORMAT_R32G32B32_FLOAT:
			case DXGI_FORMAT_R32G32B32A32_FLOAT:
				line << "float4 ";
				break;
			case DXGI_FORMAT_R32_UINT:
			case DXGI_FORMAT_R32G32_UINT:
			case DXGI_FORMAT_R32G32B32_UINT:
			case DXGI_FORMAT_R32G32B32A32_UINT:
				line << "uint4 ";
				break;
			case DXGI_FORMAT_R32_SINT:
			case DXGI_FORMAT_R32G32_SINT:
			case DXGI_FORMAT_R32G32B32_SINT:
			case DXGI_FORMAT_R32G32B32A32_SINT:
				line << "int4 ";
				break;
			default:
				_ASSERT_EXPR_A(0, "This DXGI_FORMAT is not supported.");
				break;
			}
			line << " _" << input_element_desc[line_number].SemanticName << " : " << input_element_desc[line_number].SemanticName << " ;\n";
			source.append(line.str());
		}
		source.append("};\nvoid main(VS_IN vin){}\n");

		HRESULT hr = S_OK;
		UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
		flags |= D3DCOMPILE_DEBUG;
		flags |= D3D10_SHADER_SKIP_OPTIMIZATION;
#endif
		Microsoft::WRL::ComPtr<ID3D11VertexShader> dummy_vertex_shader;
		Microsoft::WRL::ComPtr<ID3DBlob> compiled_shader_blob;
		Microsoft::WRL::ComPtr<ID3DBlob> error_message_blob;
		const char* target = "vs_5_0";
		hr = D3DCompile(source.c_str(), source.length(), 0, 0, 0, "main", target, flags, 0, compiled_shader_blob.GetAddressOf(), error_message_blob.GetAddressOf());
		_ASSERT_EXPR_A(SUCCEEDED(hr), reinterpret_cast<LPCSTR>(error_message_blob->GetBufferPointer()));
		hr = device->CreateVertexShader(compiled_shader_blob->GetBufferPointer(), compiled_shader_blob->GetBufferSize(), 0, dummy_vertex_shader.GetAddressOf());
		_ASSERT_EXPR_A(SUCCEEDED(hr), reinterpret_cast<LPCSTR>(error_message_blob->GetBufferPointer()));
		hr = device->CreateInputLayout(input_element_desc, static_cast<UINT>(num_elements), compiled_shader_blob->GetBufferPointer(), static_cast<UINT>(compiled_shader_blob->GetBufferSize()), input_layout);
		_ASSERT_EXPR_A(SUCCEEDED(hr), reinterpret_cast<LPCSTR>(error_message_blob->GetBufferPointer()));

		std::lock_guard<std::mutex> lock(_mutex);
		_input_layouts.emplace(std::make_pair(name, *input_layout));

		return hr;
	}
	static void _exterminate()
	{
		std::lock_guard<std::mutex> lock(_mutex);
		_input_layouts.clear();
		_vertex_shaders.clear();
	}
};
