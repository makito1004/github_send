#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <assert.h>

#include "misc.h"


#define USAGE_DYNAMIC

enum class cb_usage : byte
{
	v = 0x01,
	h = 0x02,
	d = 0x04,
	g = 0x08,
	p = 0x10,
	c = 0x20,
	vp = v | p,
	vgp = v | g | p,
	vhdp = v | h | d | p,
	vhdgp = v | h | d | g | p,
	vhdgpc = v | h | d | g | p | c,
};

template <class T>
struct constant_buffer
{
	T data;
	constant_buffer(ID3D11Device* device)
	{

#if 0
		assert(sizeof(T) % 16 == 0 && L"constant buffer's need to be 16 byte aligned");
#endif

		HRESULT hr = S_OK;
		D3D11_BUFFER_DESC buffer_desc = {};
		buffer_desc.ByteWidth = (sizeof(T) + 0x0f) & ~0x0f;
		buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		buffer_desc.MiscFlags = 0;
		buffer_desc.StructureByteStride = 0;
#ifdef USAGE_DYNAMIC
		//https://docs.microsoft.com/en-us/windows/desktop/direct3d11/how-to--use-dynamic-resources
		buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
		buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		hr = device->CreateBuffer(&buffer_desc, 0, buffer_object.GetAddressOf());
		assert(SUCCEEDED(hr) && hr_trace(hr));
#else
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.CPUAccessFlags = 0;
		D3D11_SUBRESOURCE_DATA subresource_data;
		subresource_data.pSysMem = &data;
		subresource_data.SysMemPitch = 0;
		subresource_data.SysMemSlicePitch = 0;
		hr = device->CreateBuffer(&buffer_desc, &subresource_data, buffer_object.GetAddressOf());
		assert(SUCCEEDED(hr) && hr_trace(hr));
#endif
	}
	virtual ~constant_buffer() = default;
	constant_buffer(constant_buffer&) = delete;
	constant_buffer& operator =(constant_buffer&) = delete;

	void activate(ID3D11DeviceContext* immediate_context, int slot, cb_usage usage)
	{
		activate(immediate_context, slot, usage, &data);
	}
	void activate(ID3D11DeviceContext* immediate_context, int slot, cb_usage usage, const T* data)
	{
		HRESULT hr = S_OK;
#ifdef USAGE_DYNAMIC
		D3D11_MAP map = D3D11_MAP_WRITE_DISCARD;
		D3D11_MAPPED_SUBRESOURCE mapped_buffer;

		hr = immediate_context->Map(buffer_object.Get(), 0, map, 0, &mapped_buffer);
		assert(SUCCEEDED(hr) && hr_trace(hr));
		memcpy_s(mapped_buffer.pData, sizeof(T), data, sizeof(T));
		immediate_context->Unmap(buffer_object.Get(), 0);
#else
		immediate_context->UpdateSubresource(buffer_object.Get(), 0, 0, &data, 0, 0);
#endif
		if (static_cast<byte>(usage) & static_cast<byte>(cb_usage::v))
		{
			immediate_context->VSSetConstantBuffers(slot, 1, buffer_object.GetAddressOf());
		}
		if (static_cast<byte>(usage) & static_cast<byte>(cb_usage::h))
		{
			immediate_context->HSSetConstantBuffers(slot, 1, buffer_object.GetAddressOf());
		}
		if (static_cast<byte>(usage) & static_cast<byte>(cb_usage::d))
		{
			immediate_context->DSSetConstantBuffers(slot, 1, buffer_object.GetAddressOf());
		}
		if (static_cast<byte>(usage) & static_cast<byte>(cb_usage::g))
		{
			immediate_context->GSSetConstantBuffers(slot, 1, buffer_object.GetAddressOf());
		}
		if (static_cast<byte>(usage) & static_cast<byte>(cb_usage::p))
		{
			immediate_context->PSSetConstantBuffers(slot, 1, buffer_object.GetAddressOf());
		}
		if (static_cast<byte>(usage) & static_cast<byte>(cb_usage::c))
		{
			immediate_context->CSSetConstantBuffers(slot, 1, buffer_object.GetAddressOf());
		}
	}
	void deactivate(ID3D11DeviceContext* immediate_context)
	{
		// NOP
	}
private:
	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer_object;

};
