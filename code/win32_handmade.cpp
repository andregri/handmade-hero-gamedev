/*
	$File:
	$Date:
	$Revision:
	$Creator: andregri
	$Licence:
*/

#include <Windows.h>
#include <stdint.h>

#define internal static  		// internal function: can be called only in the file where it is defined.
#define local_persist static   	// allocated until the program runs in the scope and keep its value.
#define global_variable static  // static global variable is only visible in the file where it is initialized and is automatically initialized to zero.

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

// TODO(andrea) this is a global for now.
global_variable bool Running;

global_variable BITMAPINFO BitMapInfo;
global_variable void *BitmapMemory;
global_variable int BitmapWidth;
global_variable int BitmapHeight;
global_variable int BytesPerPixel = 4;

internal void
RenderWeirdGradient(int XOffset, int YOffset)
{
	int Pitch = BitmapWidth * BytesPerPixel;
	uint8 *Row = (uint8 *)BitmapMemory;
	for(int Y = 0; Y < BitmapHeight; ++Y)
	{
		//uint8 *Pixel = (uint8 *)Row;
		uint32 *Pixel = (uint32 *)Row;
		for(int X = 0; X < BitmapWidth; ++X)
		{
			/*
				LITTLE ENDIAN architecture
				(least significant byte in the lowest address)
				Pixel in memory:   BB GG RR xx
				Pixel in register: xx RR GG BB
			*/

		/* Very slow code:
			*Pixel = (uint8)(X + XOffset);
			++Pixel;

			*Pixel = (uint8)(Y + YOffset);
			++Pixel;

			*Pixel = 0;
			++Pixel;

			*Pixel = 0;
			++Pixel;
		*/

			// This code is a bit faster: indeed square moves faster
			uint8 Blue = (X + XOffset);
			uint8 Green = (Y + YOffset);
			/*
			Memory:		BB GG RR xx
			Register: 	xx RR GG BB
			Pixel (32-bits)
			*/

			*Pixel++ = ((Green << 8) | Blue); // Blue is in the bottom, Green is shifted because it's just before Blue.
		}
		Row += Pitch;
	}
}

internal void
Win32ResizeDIBSection(int Width, int Height)
{
	// TODO(andre): Bulletproof this.
	// Maybe don't free first, free after, then free first if that fails.

	if(BitmapMemory)
	{
		VirtualFree(BitmapMemory, 0, MEM_RELEASE);
	}

	BitmapWidth = Width;
	BitmapHeight = Height;

	BitMapInfo.bmiHeader.biSize = sizeof(BitMapInfo.bmiHeader);
	BitMapInfo.bmiHeader.biWidth = BitmapWidth;
	BitMapInfo.bmiHeader.biHeight = -BitmapHeight; // negative is top-down bitmap and origin is upper-left.
	BitMapInfo.bmiHeader.biPlanes = 1;
	BitMapInfo.bmiHeader.biBitCount = 32;
	BitMapInfo.bmiHeader.biCompression = BI_RGB;

	// NOTE(andre): Thank you to Chris Hecker of Spy Party fame for clarifying
	// the deal with StretchBits and BitBlt!
	// No more DC for us.

	int BitmapMemorySize = (BitmapWidth * BitmapHeight) * BytesPerPixel;
	BitmapMemory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);

	// TODO(andre): probably clear this to black.
}

internal void
Win32UpdateWindow(HDC DeviceContext, RECT *ClientRect, int X, int Y, int Width, int Height)
{
	int WindowWidth = ClientRect->right - ClientRect->left;
	int WindowHeight = ClientRect->bottom - ClientRect->top;

	// rectangle to rectangle copy
	StretchDIBits(
		DeviceContext,
		0, 0, BitmapWidth, BitmapHeight,
		0, 0, WindowWidth, WindowHeight,
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

			RECT ClientRect;
			GetClientRect(Window, &ClientRect);
			Win32UpdateWindow(DeviceContext, &ClientRect, X, Y, Width, Height);
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
	//WindowClass.style = CS_OWNDC|CS_HREDRAW|CS_VREDRAW;
	WindowClass.lpfnWndProc = Win32MainWindowCallback; // Window Procedure
	WindowClass.hInstance = Instance;
	WindowClass.lpszClassName = "HandmadHeroWindowClass";

	if(RegisterClassA(&WindowClass))
	{
		HWND Window = CreateWindowExA(
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
		if(Window)
		{
			int XOffset = 0;
			int YOffset = 0;

			Running = true;
			while(Running)
			{
				MSG Message;
				//BOOL MessageResult = GetMessageA(&Message, 0, 0, 0); // GetMessageA will block!
				// We want to skip and keep running if there are no messages:
				while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
				{
					if(Message.message == WM_QUIT)
					{
						Running = false;
					}
					TranslateMessage(&Message);
					DispatchMessageA(&Message);
				}
				
				RenderWeirdGradient(XOffset, YOffset);
				
				HDC DeviceContext = GetDC(Window);
				RECT ClientRect;
				GetClientRect(Window, &ClientRect);
				int WindowHeight = ClientRect.bottom - ClientRect.top;
				int WindowWidth = ClientRect.right - ClientRect.left;
				Win32UpdateWindow(DeviceContext, &ClientRect, 0, 0, WindowWidth, WindowHeight);
				ReleaseDC(Window, DeviceContext);

				++XOffset;
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