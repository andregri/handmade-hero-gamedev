/*
	$File:
	$Date:
	$Revision:
	$Creator: andregri
	$Licence:
*/

#include <Windows.h>

#define internal static  		// internal function: can be called only in the file where it is defined.
#define local_persist static   	// allocated until the program runs in the scope and keep its value.
#define global_variable static  // static global variable is only visible in the file where it is initialized and is automatically initialized to zero.

// TODO(andrea) this is a global for now.
global_variable bool Running;
global_variable BITMAPINFO BitMapInfo;
global_variable void *BitmapMemory;
global_variable HBITMAP BitmapHandle;
global_variable HDC BitmapDeviceContext;

internal void Win32ResizeDIBSection(int Width, int Height)
{
	// TODO(andre): Bulletproof this.
	// Maybe don't free first, free after, then free first if that fails.

	if(BitmapHandle)
	{
		DeleteObject(BitmapHandle);  // Delete a bitmap.
	}
	
	if(!BitmapDeviceContext)
	{
		// TODO(andre): Should we recreate these under certain special circumstances?
		BitmapDeviceContext = CreateCompatibleDC(0);
	}
	
	BitMapInfo.bmiHeader.biSize = sizeof(BitMapInfo.bmiHeader);
	BitMapInfo.bmiHeader.biWidth = Width;
	BitMapInfo.bmiHeader.biHeight = Height;
	BitMapInfo.bmiHeader.biPlanes = 1;
	BitMapInfo.bmiHeader.biBitCount = 32;
	BitMapInfo.bmiHeader.biCompression = BI_RGB;

	// TODO(andre): maybe we can just allocate this ourselves?
	
	BitmapHandle = CreateDIBSection(
		BitmapDeviceContext, &BitMapInfo,
		DIB_RGB_COLORS,
		&BitmapMemory,
		0, 0);
}

internal void Win32UpdateWindow(HDC DeviceContext, int X, int Y, int Width, int Height)
{
	// rectangle to rectangle copy
	StretchDIBits(
		DeviceContext,
		X, Y, Width, Height,
		X, Y, Width, Height,
		BitmapMemory,
		&BitMapInfo,
		DIB_RGB_COLORS, SRCCOPY);  
}

LRESULT CALLBACK Win32MainWindowCallback(
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
			RECT ClientRect;
			GetClientRect(Window, &ClientRect);  // get the part of the window where you can actually draw, for instance not the borders.
			int Height = ClientRect.bottom - ClientRect.top;
			int Width = ClientRect.right - ClientRect.left;
			Win32ResizeDIBSection(Width, Height);
			OutputDebugStringA("WM_SIZE\n");
		} break;

		case WM_DESTROY:
		{
			// TODO(andrea): handle this as an error - recreate window?
			Running = false;
			OutputDebugStringA("WM_DESTROY\n");
		} break;

		case WM_CLOSE:
		{
			// TODO(andre): handle this with a message to the user?
			Running = false;
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
			Win32UpdateWindow(DeviceContext, X, Y, Width, Height);
			local_persist DWORD Operation = WHITENESS;
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
	WindowClass.lpfnWndProc = Win32MainWindowCallback; // Window Procedure
	WindowClass.hInstance = Instance;
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
			Running = true;
			while(Running)
			{
				MSG Message;
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