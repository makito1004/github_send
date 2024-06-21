#pragma once

#include <windows.h>
#include <memory>
#include <sstream>

#include "misc.h"
#include "high_resolution_timer.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern ImWchar glyphRangesJapanese[];
#endif

#include <d3d11_1.h>
#include <dxgi1_6.h>
#include <wrl.h>

#ifdef ENABLE_DIRECT2D
#include <d2d1_1.h>
#include <dwrite.h>
#pragma comment(lib,"d2d1.lib")
#pragma comment(lib,"dwrite.lib")
#endif

class framework
{
public:
	CONST HWND hwnd;

	Microsoft::WRL::ComPtr<IDXGIAdapter3> adapter;
	size_t video_memory_usage()
	{
		DXGI_QUERY_VIDEO_MEMORY_INFO video_memory_info;
		adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &video_memory_info);
		return video_memory_info.CurrentUsage / 1024 / 1024;
	}

	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediate_context;

	Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain;

	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target_view;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depth_stencil_view;

#ifdef ENABLE_DIRECT2D
	Microsoft::WRL::ComPtr<ID2D1DeviceContext> d2d1_device_context;
	Microsoft::WRL::ComPtr<IDWriteTextFormat> dwrite_text_format;
	Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> d2d_solid_color_brush;
#endif

	framework(HWND hwnd, BOOL fullscreen);
	~framework();

	framework(const framework&) = delete;
	framework& operator=(const framework&) = delete;
	framework(framework&&) noexcept = delete;
	framework& operator=(framework&&) noexcept = delete;

	int run()
	{
		MSG msg{};

		if (!initialize())
		{
			return 0;
		}

#ifdef USE_IMGUI
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::GetIO().Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", 14.0f, nullptr, glyphRangesJapanese);
		ImGui_ImplWin32_Init(hwnd);
		ImGui_ImplDX11_Init(device.Get(), immediate_context.Get());
		ImGui::StyleColorsDark();
#endif

		while (WM_QUIT != msg.message)
		{
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else
			{
				tictoc.tick();
				calculate_frame_stats();

#ifdef USE_IMGUI
				ImGui_ImplDX11_NewFrame();
				ImGui_ImplWin32_NewFrame();
				ImGui::NewFrame();
#endif
				bool renderable = update(tictoc.time_interval());
				if (renderable)
				{
					render(tictoc.time_interval());
				}

#ifdef USE_IMGUI
				ImGui::Render();
				ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
#endif
				if (swap_chain)
				{
					UINT sync_interval{ 0 };
					swap_chain->Present(sync_interval, 0);
				}
			}
		}

		

#ifdef USE_IMGUI
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
#endif

		BOOL fullscreen{};
		swap_chain->GetFullscreenState(&fullscreen, 0);
		if (fullscreen)
		{
			swap_chain->SetFullscreenState(FALSE, 0);
		}

		return uninitialize() ? static_cast<int>(msg.wParam) : 0;
	}

	LRESULT CALLBACK handle_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
#ifdef USE_IMGUI
		if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) { return true; }
#endif
		switch (msg)
		{
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			BeginPaint(hwnd, &ps);

			EndPaint(hwnd, &ps);
			break;
		}
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		case WM_CREATE:
			break;
		case WM_KEYDOWN:
			if (wparam == VK_ESCAPE)
			{
				PostMessage(hwnd, WM_CLOSE, 0, 0);
			}
			break;
		case WM_ENTERSIZEMOVE:
			tictoc.stop();
			break;
		case WM_EXITSIZEMOVE:
			tictoc.start();
			break;
		case WM_SIZE:
		{
#if 1
			RECT client_rect{};
			GetClientRect(hwnd, &client_rect);
			on_size_changed(static_cast<UINT64>(client_rect.right - client_rect.left), client_rect.bottom - client_rect.top);
#endif
			break;
		}
		default:
			return DefWindowProc(hwnd, msg, wparam, lparam);
		}
		return 0;
	}

private:
	bool initialize();
	bool update(float delta_time);
	void render(float delta_time);
	bool uninitialize();

private:
	high_resolution_timer tictoc;
	uint32_t frames_per_second{ 0 };
	float count_by_seconds{ 0.0f };
	void calculate_frame_stats()
	{
		if (++frames_per_second, (tictoc.time_stamp() - count_by_seconds) >= 1.0f)
		{
			float fps = static_cast<float>(frames_per_second);
			std::wostringstream oss;
#if 0
			oss.precision(6);
			oss << L"X3DGP" << L" : FPS : " << fps << L" / " << L"Frame Time : " << 1000.0f / fps << L" (ms)";
#else
			oss << L"FPS : " << fps << L" / " << L"VIDEO_MEMORY_USAGE : " << video_memory_usage() << L"MB";
#endif
			SetWindowTextW(hwnd, oss.str().c_str());

			frames_per_second = 0;
			count_by_seconds += 1.0f;
		}
	}

	SIZE framebuffer_dimensions;
	bool fullscreen_mode = false;
	
	RECT windowed_rect; // フルスクリーンモードを終了するときに復元できるように、初期ウィンドウの矩形を保存する。
	DWORD windowed_style; // 初期ウィンドウスタイルを保存する

	void tell_fullscreen_state(BOOL fullscreen);
	void on_size_changed(UINT64 width, UINT height);
};

