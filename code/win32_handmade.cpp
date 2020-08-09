/*
	$File:
	$Date:
	$Revision:
	$Creator: andregri
	$Licence:
*/

#include <Windows.h>

LRESULT CALLBACK MainWindowCallback(
	HWND	Window,
	UINT	Message,
	WPARAM 	WParam,
	LPARAM 	LParam)
{
	LRESULT Result = 0;

	switch(Message)
	{
		case WM_SIZE:
		{
			OutputDebugStringA("WM_SIZE\n");
		} break;

		case WM_DESTROY:
		{
			OutputDebugStringA("WM_DESTROY\n");
		} break;

		case WM_CLOSE:
		{
			OutputDebugStringA("WM_CLOSE\n");
		} break;

		case WM_ACTIVATEAPP:
		{
			OutputDebugStringA("WM_ACTIVATEAPP\n");
		} break;

		case WM_PAINT:
		{
			PAINTSTRUCT Paint;
			HDC DeviceContext = BeginPaint(Window, &Paint);
			int X = Paint.rcPaint.left;
			int Y = Paint.rcPaint.top;
			int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
			int Width = Paint.rcPaint.right - Paint.rcPaint.left;
			static DWORD Operation = WHITENESS;
			PatBlt(DeviceContext, X, Y, Width, Height, Operation);
			if(Operation == WHITENESS)
			{
				Operation = BLACKNESS;
			}
			else
			{
				Operation = WHITENESS;
			}
			EndPaint(Window, &Paint);
		} break;

		default:
		{
			OutputDebugStringA("default\n");
			Result = DefWindowProc(Window, Message, WParam, LParam);
		} break;
	}

	return Result;
}

INT WinMain(
	HINSTANCE 	Instance,
	HINSTANCE 	PrevInstance,
	PSTR 		CommandLine,
	INT 		ShowCode)
{
	WNDCLASS WindowClass = {};
	WindowClass.style = CS_OWNDC|CS_HREDRAW|CS_VREDRAW;
	WindowClass.lpfnWndProc = MainWindowCallback; // Window Procedure
	WindowClass.hInstance = Instance;
	//WindowClass.hIcon;
	WindowClass.lpszClassName = "HandmadHeroWindowClass";

	if(RegisterClassA(&WindowClass))
	{
		HWND WindowHandle = CreateWindowExA(
			0, 
			WindowClass.lpszClassName, 
			"Handmade Hero", 
			WS_OVERLAPPEDWINDOW|WS_VISIBLE, 
			CW_USEDEFAULT, 
			CW_USEDEFAULT, 
			CW_USEDEFAULT, 
			CW_USEDEFAULT, 
			0, 
			0, 
			Instance, 
			0);
		if(WindowHandle)
		{
			MSG Message;
			for(;;)
			{
				BOOL MessageResult = GetMessageA(&Message, 0, 0, 0);
				if(MessageResult > 0)
				{
					TranslateMessage(&Message);
					DispatchMessageA(&Message);
				}
				else
				{
					break;
				}
			}
			
		}
		else
		{
			// TODO(andrea): logging
		}
	}
	else
	{
		// TODO(andrea): logging
	}

	// `A` stands for Ascii, so that if a computer uses Unicode to read strings,
	// the suffix `A` tells to read them as Ascii.
	//MessageBoxA(0, "This is handmade hero", "Handmade hero", MB_OK|MB_ICONINFORMATION);
	
	return 0;
}