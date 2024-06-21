#include <time.h>

#include "framework.h"

#if 1
CONST LONG SCREEN_WIDTH{ 1680 };
CONST LONG SCREEN_HEIGHT{ 715 };
#else
CONST LONG SCREEN_WIDTH{ 1980 };
CONST LONG SCREEN_HEIGHT{ 1080 };
#endif

LRESULT CALLBACK window_procedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	framework* p{ reinterpret_cast<framework*>(GetWindowLongPtr(hwnd, GWLP_USERDATA)) };
	return p ? p->handle_message(hwnd, msg, wparam, lparam) : DefWindowProc(hwnd, msg, wparam, lparam);
}

int WINAPI WinMain(_In_ HINSTANCE instance, _In_opt_  HINSTANCE prev_instance, _In_ LPSTR cmd_line, _In_ int cmd_show)
{
#if 0
	// Windows 10 Creators update adds Per Monitor V2 DPI awareness context.
	// Using this awareness context allows the client area of the window
	// to achieve 100% scaling while still allowing non-client window content to
	// be rendered in a DPI sensitive fashion.
	// @see https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setthreaddpiawarenesscontext
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#endif

	// 呼び出し元のスレッドが使用する COM ライブラリを初期化し、スレッドの同時実行モデルを設定
	HRESULT hr = CoInitializeEx(NULL, COINITBASE_MULTITHREADED);
	_ASSERT_EXPR(SUCCEEDED(hr), "Fail to initializes COM library");
	srand(static_cast<unsigned int>(time(nullptr)));

#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	WNDCLASSEXW wcex{};
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = window_procedure;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = instance;
	wcex.hIcon = 0;
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = L"X3DGP";
	wcex.hIconSm = 0;
	RegisterClassExW(&wcex);

	RECT rc{ 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	HWND hwnd = CreateWindowExW(0, L"X3DGP", L"", WS_OVERLAPPEDWINDOW ^ WS_MAXIMIZEBOX ^ WS_THICKFRAME | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, instance, NULL);
	ShowWindow(hwnd, cmd_show);

	framework framework(hwnd, TRUE);
	SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&framework));
	return framework.run();
}
