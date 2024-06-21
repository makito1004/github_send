#include "framework.h"
#include "boot_scene.h"
#include "intermezzo_scene.h"
#include "main_scene.h"

using namespace DirectX;

void acquire_high_performance_adapter(IDXGIFactory6* dxgi_factory6, IDXGIAdapter3** dxgi_adapter3)
{
	HRESULT hr = S_OK;

	Microsoft::WRL::ComPtr<IDXGIAdapter3> enumerated_adapter;
	for (UINT adapter_index = 0; DXGI_ERROR_NOT_FOUND != dxgi_factory6->EnumAdapterByGpuPreference(adapter_index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(enumerated_adapter.ReleaseAndGetAddressOf())); ++adapter_index)
	{
		DXGI_ADAPTER_DESC1 adapter_desc;
		hr = enumerated_adapter->GetDesc1(&adapter_desc);
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		if (adapter_desc.VendorId == 0x1002/*AMD*/ || adapter_desc.VendorId == 0x10DE/*NVIDIA*/)
		{
			OutputDebugStringW((std::wstring(adapter_desc.Description) + L" has been selected.\n").c_str());
			OutputDebugStringA(std::string("\tVendorId:" + std::to_string(adapter_desc.VendorId) + '\n').c_str());
			OutputDebugStringA(std::string("\tDeviceId:" + std::to_string(adapter_desc.DeviceId) + '\n').c_str());
			OutputDebugStringA(std::string("\tSubSysId:" + std::to_string(adapter_desc.SubSysId) + '\n').c_str());
			OutputDebugStringA(std::string("\tRevision:" + std::to_string(adapter_desc.Revision) + '\n').c_str());
			OutputDebugStringA(std::string("\tDedicatedVideoMemory:" + std::to_string(adapter_desc.DedicatedVideoMemory) + '\n').c_str());
			OutputDebugStringA(std::string("\tDedicatedSystemMemory:" + std::to_string(adapter_desc.DedicatedSystemMemory) + '\n').c_str());
			OutputDebugStringA(std::string("\tSharedSystemMemory:" + std::to_string(adapter_desc.SharedSystemMemory) + '\n').c_str());
			OutputDebugStringA(std::string("\tAdapterLuid.HighPart:" + std::to_string(adapter_desc.AdapterLuid.HighPart) + '\n').c_str());
			OutputDebugStringA(std::string("\tAdapterLuid.LowPart:" + std::to_string(adapter_desc.AdapterLuid.LowPart) + '\n').c_str());
			OutputDebugStringA(std::string("\tFlags:" + std::to_string(adapter_desc.Flags) + '\n').c_str());
			break;
		}
	}
	*dxgi_adapter3 = enumerated_adapter.Detach();
}

framework::framework(HWND hwnd, BOOL fullscreen) : hwnd(hwnd), fullscreen_mode(fullscreen), windowed_style(static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE)))
{
	if (fullscreen)
	{
		tell_fullscreen_state(true);
	}

	RECT client_rect;
	GetClientRect(hwnd, &client_rect);
	framebuffer_dimensions.cx = client_rect.right - client_rect.left;
	framebuffer_dimensions.cy = client_rect.bottom - client_rect.top;

	HRESULT hr{ S_OK };

	UINT create_factory_flags = 0;
#ifdef _DEBUG
	create_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
	Microsoft::WRL::ComPtr<IDXGIFactory6> dxgi_factory6;
	CreateDXGIFactory2(create_factory_flags, IID_PPV_ARGS(dxgi_factory6.GetAddressOf()));
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	acquire_high_performance_adapter(dxgi_factory6.Get(), adapter.GetAddressOf());


	UINT create_device_flags{ 0 };
#ifdef ENABLE_DIRECT2D
	create_device_flags |= D3D11_CREATE_DEVICE_BGRA_SUPPORT; // UNIT.99:Required to use Direct2D on DirectX11.
#endif
#ifdef _DEBUG
	create_device_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	D3D_FEATURE_LEVEL feature_levels[]
	{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};
	D3D_FEATURE_LEVEL feature_level;
	hr = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, 0, create_device_flags, feature_levels, _countof(feature_levels), D3D11_SDK_VERSION, device.ReleaseAndGetAddressOf(), &feature_level, immediate_context.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	_ASSERT_EXPR(!(feature_level < D3D_FEATURE_LEVEL_11_0), L"Direct3D Feature Level 11 unsupported.");

	DXGI_SWAP_CHAIN_DESC1 swap_chain_desc1{};
	swap_chain_desc1.Width = framebuffer_dimensions.cx;
	swap_chain_desc1.Height = framebuffer_dimensions.cy;
	swap_chain_desc1.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	swap_chain_desc1.Stereo = FALSE;
	swap_chain_desc1.SampleDesc.Count = 1;
	swap_chain_desc1.SampleDesc.Quality = 0;
	swap_chain_desc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desc1.BufferCount = 2;
	swap_chain_desc1.Scaling = DXGI_SCALING_STRETCH;
	swap_chain_desc1.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	swap_chain_desc1.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	hr = dxgi_factory6->CreateSwapChainForHwnd(device.Get(), hwnd, &swap_chain_desc1, nullptr, nullptr, swap_chain.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

#if 1

	hr = dxgi_factory6->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
#endif

	Microsoft::WRL::ComPtr<ID3D11Texture2D> render_target_buffer;
	hr = swap_chain->GetBuffer(0, IID_PPV_ARGS(render_target_buffer.GetAddressOf()));
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	hr = device->CreateRenderTargetView(render_target_buffer.Get(), nullptr, render_target_view.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	Microsoft::WRL::ComPtr<ID3D11Texture2D> depth_stencil_buffer{};
	D3D11_TEXTURE2D_DESC texture2d_desc{};
	texture2d_desc.Width = framebuffer_dimensions.cx;
	texture2d_desc.Height = framebuffer_dimensions.cy;
	texture2d_desc.MipLevels = 1;
	texture2d_desc.ArraySize = 1;
	texture2d_desc.Format = DXGI_FORMAT_R32_TYPELESS; // DXGI_FORMAT_R24G8_TYPELESS DXGI_FORMAT_R32_TYPELESS
	texture2d_desc.SampleDesc.Count = 1;
	texture2d_desc.SampleDesc.Quality = 0;
	texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
	texture2d_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	texture2d_desc.CPUAccessFlags = 0;
	texture2d_desc.MiscFlags = 0;
	hr = device->CreateTexture2D(&texture2d_desc, NULL, depth_stencil_buffer.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
	depth_stencil_view_desc.Format = DXGI_FORMAT_D32_FLOAT; // DXGI_FORMAT_D24_UNORM_S8_UINT DXGI_FORMAT_D32_FLOAT
	depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depth_stencil_view_desc.Texture2D.MipSlice = 0;
	hr = device->CreateDepthStencilView(depth_stencil_buffer.Get(), &depth_stencil_view_desc, depth_stencil_view.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

#ifdef ENABLE_DIRECT2D
	// Direct2d用のCOMオブジェクトを作成
	Microsoft::WRL::ComPtr<IDXGIDevice2> dxgi_device2;
	hr = device.As(&dxgi_device2);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	Microsoft::WRL::ComPtr<ID2D1Factory1> d2d_factory1;
	D2D1_FACTORY_OPTIONS factory_options{};
#ifdef _DEBUG
	factory_options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
	hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory_options, d2d_factory1.GetAddressOf());

	Microsoft::WRL::ComPtr<ID2D1Device> d2d_device;
	hr = d2d_factory1->CreateDevice(dxgi_device2.Get(), d2d_device.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	hr = d2d_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, d2d1_device_context.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	hr = dxgi_device2->SetMaximumFrameLatency(1);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	Microsoft::WRL::ComPtr<IDXGISurface2> dxgi_surface2;
	hr = swap_chain->GetBuffer(0, IID_PPV_ARGS(dxgi_surface2.GetAddressOf()));
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	Microsoft::WRL::ComPtr<ID2D1Bitmap1> d2d_bitmap1;
	hr = d2d1_device_context->CreateBitmapFromDxgiSurface(dxgi_surface2.Get(),
		D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
			D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE)), d2d_bitmap1.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	d2d1_device_context->SetTarget(d2d_bitmap1.Get());


	// DirectWrite用のCOMオブジェクトを作成.
	Microsoft::WRL::ComPtr<IDWriteFactory> dwrite_factory;
	hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(dwrite_factory.GetAddressOf()));
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	hr = dwrite_factory->CreateTextFormat(L"Meiryo", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 11, L"", dwrite_text_format.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	hr = dwrite_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	hr = d2d1_device_context->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), d2d_solid_color_brush.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
#endif

	D3D11_VIEWPORT viewport{};
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = static_cast<float>(framebuffer_dimensions.cx);
	viewport.Height = static_cast<float>(framebuffer_dimensions.cy);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	immediate_context->RSSetViewports(1, &viewport);
}

bool framework::initialize()
{

	HRESULT hr{ S_OK };

	scene::_boot<boot_scene>(device.Get(), framebuffer_dimensions.cx, framebuffer_dimensions.cy, {});
	scene::_emplace<intermezzo_scene>();
	scene::_emplace<main_scene>();

	return true;
}



bool framework::update(float delta_time/*Elapsed seconds from last frame*/)
{
#if 1
	if (GetAsyncKeyState(VK_RETURN) & 1 && GetAsyncKeyState(VK_MENU) & 1)
	{
		tell_fullscreen_state(!fullscreen_mode);
	}
#endif

#if 0
	if (GetKeyState('W') & 0x8000)
	{
		se[0]->play();
	}
	else
	{
		se[0]->stop();
	}
	if (GetAsyncKeyState('Q') & 1)
	{
		if (bgm[0]->queuing())
		{
			bgm[0]->stop();
		}
		else
		{
			bgm[0]->play(255);
		}
	}
#endif // 0


	bool renderable = scene::_update(immediate_context.Get(), delta_time);
	return renderable;
}
void framework::render(float delta_time)
{
	HRESULT hr{ S_OK };

	ID3D11RenderTargetView* null_render_target_views[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
	immediate_context->OMSetRenderTargets(_countof(null_render_target_views), null_render_target_views, 0);
	ID3D11ShaderResourceView* null_shader_resource_views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
	immediate_context->VSSetShaderResources(0, _countof(null_shader_resource_views), null_shader_resource_views);
	immediate_context->PSSetShaderResources(0, _countof(null_shader_resource_views), null_shader_resource_views);

	FLOAT color[]{ 0.0f, 0.0f, 0.0f, 1.0f };
	immediate_context->ClearRenderTargetView(render_target_view.Get(), color);
	immediate_context->ClearDepthStencilView(depth_stencil_view.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	immediate_context->OMSetRenderTargets(1, render_target_view.GetAddressOf(), depth_stencil_view.Get());


	scene::_render(immediate_context.Get(), delta_time);



#ifdef ENABLE_DIRECT2D
#if 0
	d2d1_device_context->BeginDraw();
	const wchar_t message[]{ L"" };
	d2d1_device_context->DrawTextW(message, _countof(message), dwrite_text_format.Get(),
		D2D1::RectF(10.0f, 0.0f + 10.0f, static_cast<FLOAT>(framebuffer_dimensions.cx), static_cast<FLOAT>(framebuffer_dimensions.cy)),
		d2d_solid_color_brush.Get(), D2D1_DRAW_TEXT_OPTIONS_NONE);
	hr = d2d1_device_context->EndDraw();
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
#endif
#endif
}

bool framework::uninitialize()
{
	return scene::_uninitialize(device.Get());
}

framework::~framework()
{

}
void framework::tell_fullscreen_state(BOOL fullscreen)
{
	fullscreen_mode = fullscreen;
	if (fullscreen)
	{
		GetWindowRect(hwnd, &windowed_rect);
		SetWindowLongPtrA(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU | WS_THICKFRAME));

		RECT fullscreen_window_rect;

		HRESULT hr{ E_FAIL };
		if (swap_chain)
		{
			Microsoft::WRL::ComPtr<IDXGIOutput> dxgi_output;
			hr = swap_chain->GetContainingOutput(&dxgi_output);
			if (hr == S_OK)
			{
				DXGI_OUTPUT_DESC output_desc;
				hr = dxgi_output->GetDesc(&output_desc);
				if (hr == S_OK)
				{
					fullscreen_window_rect = output_desc.DesktopCoordinates;
				}
			}
		}
		if (hr != S_OK)
		{
			DEVMODE devmode = {};
			devmode.dmSize = sizeof(DEVMODE);
			EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devmode);

			fullscreen_window_rect = {
				devmode.dmPosition.x,
				devmode.dmPosition.y,
				devmode.dmPosition.x + static_cast<LONG>(devmode.dmPelsWidth),
				devmode.dmPosition.y + static_cast<LONG>(devmode.dmPelsHeight)
			};
		}
		SetWindowPos(
			hwnd,
#ifdef _DEBUG
			NULL,
#else
			HWND_TOPMOST,
#endif
			fullscreen_window_rect.left,
			fullscreen_window_rect.top,
			fullscreen_window_rect.right,
			fullscreen_window_rect.bottom,
			SWP_FRAMECHANGED | SWP_NOACTIVATE);

		ShowWindow(hwnd, SW_MAXIMIZE);
	}
	else
	{
		SetWindowLongPtrA(hwnd, GWL_STYLE, windowed_style);
		SetWindowPos(
			hwnd,
			HWND_NOTOPMOST,
			windowed_rect.left,
			windowed_rect.top,
			windowed_rect.right - windowed_rect.left,
			windowed_rect.bottom - windowed_rect.top,
			SWP_FRAMECHANGED | SWP_NOACTIVATE);

		ShowWindow(hwnd, SW_NORMAL);
	}
}
void framework::on_size_changed(UINT64 width, UINT height)
{
	HRESULT hr = S_OK;

	if (width == 0 || height == 0)
	{
		return;
	}

	//バッファやその他のリソースのサイズを変更する必要があるかどうかを判断
	if (width != framebuffer_dimensions.cx || height != framebuffer_dimensions.cy)
	{
		framebuffer_dimensions.cx = static_cast<LONG>(width);
		framebuffer_dimensions.cy = height;

		immediate_context->Flush();
		immediate_context->ClearState();

		Microsoft::WRL::ComPtr<IDXGIFactory2> dxgi_factory2;
		hr = swap_chain->GetParent(IID_PPV_ARGS(dxgi_factory2.GetAddressOf()));
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		DXGI_SWAP_CHAIN_DESC1 swap_chain_desc1{};
		swap_chain_desc1.Width = static_cast<UINT>(framebuffer_dimensions.cx);
		swap_chain_desc1.Height = framebuffer_dimensions.cy;
		swap_chain_desc1.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		swap_chain_desc1.Stereo = FALSE;
		swap_chain_desc1.SampleDesc.Count = 1;
		swap_chain_desc1.SampleDesc.Quality = 0;
		swap_chain_desc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swap_chain_desc1.BufferCount = 2;
		swap_chain_desc1.Scaling = DXGI_SCALING_STRETCH;
		swap_chain_desc1.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		swap_chain_desc1.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
		hr = dxgi_factory2->CreateSwapChainForHwnd(device.Get(), hwnd, &swap_chain_desc1, nullptr, nullptr, swap_chain.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
#if 1
		hr = dxgi_factory2->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
#endif

		Microsoft::WRL::ComPtr<IDXGIFactory> dxgi_factory;
		hr = swap_chain->GetParent(IID_PPV_ARGS(dxgi_factory.GetAddressOf()));
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		
		hr = dxgi_factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_ALT_ENTER);
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer;
		hr = swap_chain->GetBuffer(0, IID_PPV_ARGS(back_buffer.GetAddressOf()));
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		hr = device->CreateRenderTargetView(back_buffer.Get(), NULL, render_target_view.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		Microsoft::WRL::ComPtr<ID3D11Texture2D> depth_stencil_buffer;
		D3D11_TEXTURE2D_DESC texture2d_desc{};
		texture2d_desc.Width = static_cast<UINT>(framebuffer_dimensions.cx);
		texture2d_desc.Height = framebuffer_dimensions.cy;
		texture2d_desc.MipLevels = 1;
		texture2d_desc.ArraySize = 1;
		texture2d_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		texture2d_desc.SampleDesc.Count = 1;
		texture2d_desc.SampleDesc.Quality = 0;
		texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
		texture2d_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		texture2d_desc.CPUAccessFlags = 0;
		texture2d_desc.MiscFlags = 0;
		hr = device->CreateTexture2D(&texture2d_desc, NULL, depth_stencil_buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
		depth_stencil_view_desc.Format = texture2d_desc.Format;
		depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		depth_stencil_view_desc.Texture2D.MipSlice = 0;
		hr = device->CreateDepthStencilView(depth_stencil_buffer.Get(), &depth_stencil_view_desc, depth_stencil_view.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		D3D11_VIEWPORT viewport{};
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.Width = static_cast<float>(framebuffer_dimensions.cx);
		viewport.Height = static_cast<float>(framebuffer_dimensions.cy);
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		immediate_context->RSSetViewports(1, &viewport);

#ifdef ENABLE_DIRECT2D
		Microsoft::WRL::ComPtr<IDXGIDevice2> dxgi_device2;
		hr = device.As(&dxgi_device2);
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		Microsoft::WRL::ComPtr<ID2D1Factory1> d2d_factory1;
		D2D1_FACTORY_OPTIONS factory_options{};
#ifdef _DEBUG
		factory_options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
		hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory_options, d2d_factory1.GetAddressOf());

		Microsoft::WRL::ComPtr<ID2D1Device> d2d_device;
		hr = d2d_factory1->CreateDevice(dxgi_device2.Get(), d2d_device.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		hr = d2d_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, d2d1_device_context.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		hr = dxgi_device2->SetMaximumFrameLatency(1);
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		Microsoft::WRL::ComPtr<IDXGISurface2> dxgi_surface2;
		hr = swap_chain->GetBuffer(0, IID_PPV_ARGS(dxgi_surface2.GetAddressOf()));
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		Microsoft::WRL::ComPtr<ID2D1Bitmap1> d2d_bitmap1;
		hr = d2d1_device_context->CreateBitmapFromDxgiSurface(dxgi_surface2.Get(),
			D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
				D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE)), d2d_bitmap1.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		d2d1_device_context->SetTarget(d2d_bitmap1.Get());

		// DirectWrite用のCOMオブジェクトを作成
		Microsoft::WRL::ComPtr<IDWriteFactory> dwrite_factory;
		hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(dwrite_factory.GetAddressOf()));
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		hr = dwrite_factory->CreateTextFormat(L"Meiryo", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 11, L"", dwrite_text_format.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		hr = dwrite_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		hr = d2d1_device_context->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), d2d_solid_color_brush.ReleaseAndGetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
#endif



		scene::_on_size_changed(device.Get(), framebuffer_dimensions.cx, framebuffer_dimensions.cy);

	}
}