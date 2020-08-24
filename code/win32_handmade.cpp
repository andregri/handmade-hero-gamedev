/*
	$File:
	$Date:
	$Revision:
	$Creator: andregri
	$Licence:
*/

#include <Windows.h>
#include <stdint.h>
#include <xinput.h>

#define internal static		   // internal function: can be called only in the file where it is defined.
#define local_persist static   // allocated until the program runs in the scope and keep its value.
#define global_variable static // static global variable is only visible in the file where it is initialized and is automatically initialized to zero.

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

struct win32_offscreen_buffer
{
	BITMAPINFO Info;
	void *Memory;
	int Width;
	int Height;
	int Pitch;
	int BytesPerPixel;
};

struct win32_window_dimension
{
	int Width;
	int Height;
};


// Load xinput functions manually because in the documentation it says it supports only Windows8 and Windows Vista ??

// NOTE(andre): XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState) // Define the function prototype so that if we change the signature here, it changes everywhere.
typedef X_INPUT_GET_STATE(x_input_get_state); // this macro will produce -> typedef DWORD WINAPI x_input_get_state(DWORD dwUserIndex, XINPUT_STATE* pState); -> create a typedef for this function so we can easily make a pointer.
X_INPUT_GET_STATE(XInputGetStateStub)  // Define stubs
{
	return 0;
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// NOTE(andre): XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state); // this macro will producetypedef DWORD WINAPI x_input_set_state(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration);
X_INPUT_SET_STATE(XInputSetStateStub)
{
	return 0;
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

// Load windows functions of xinput
internal void
Win32LoadXInput(void)
{
	HMODULE XInputLibrary = LoadLibraryA("xinput1_3.dll");
	if(XInputLibrary)
	{
		XInputGetState = (x_input_get_state*)GetProcAddress(XInputLibrary, "XInputGetState");
		XInputSetState = (x_input_set_state*)GetProcAddress(XInputLibrary, "XInputSetState");
	}
}

// TODO(andrea): this is a global for now.
global_variable bool GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackbuffer;

internal win32_window_dimension
Win32GetWindowDimension(HWND Window)
{
	win32_window_dimension Result;

	RECT ClientRect;
	GetClientRect(Window, &ClientRect); // get the part of the window where you can actually draw, for instance not the borders.
	Result.Height = ClientRect.bottom - ClientRect.top;  // Left and Top are always zero.
	Result.Width = ClientRect.right - ClientRect.left;

	return Result;
}

internal void
RenderWeirdGradient(win32_offscreen_buffer &Buffer, int XOffset, int YOffset)
{
	/*
						 WIDTH ->
						 0                                               Width*BytesPerPixel
	BitmapMemory -> 	 Row0: BB GG RR xx BB GG RR xx BB GG RR xx . . .
	BitmapMem + Pitch -> Row1: BB GG RR xx BB GG RR xx BB GG RR xx . . .
	*/

	// TODO(andre): Let's see what the optimizer does if we pass by value instead of by-reference.

	uint8 *Row = (uint8 *)Buffer.Memory;
	for (int Y = 0; Y < Buffer.Height; ++Y)
	{
		uint32 *Pixel = (uint32 *)Row;
		for (int X = 0; X < Buffer.Width; ++X)
		{
			/*
				LITTLE ENDIAN architecture
				(least significant byte in the lowest address)
				Pixel in memory:   	RR GG BB xx
				Loaded in: 			xx BB GG RR
				Microsoft wanted:	xx RR GG BB
				Windows mem order:  BB GG RR xx (little endian)
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
		Row += Buffer.Pitch;  // Move to the next row. (Pitch is measured in bytes (uint8))
	}
}

internal void
Win32ResizeDIBSection(win32_offscreen_buffer &Buffer, int Width, int Height)
{
	// TODO(andre): Bulletproof this.
	// Maybe don't free first, free after, then free first if that fails.

	if (Buffer.Memory)
	{
		VirtualFree(Buffer.Memory, 0, MEM_RELEASE);
	}

	Buffer.Width = Width;
	Buffer.Height = Height;
	Buffer.BytesPerPixel = 4;

	// NOTE(andre): when the biHeight is negative, this is the clue to Windows
	// to treat this bitmap as top-down, not bottom-up, meaning that the first
	// three bytes of the image are the color for the top-left pixel in the
	// bitmap, not the bottom left!
	Buffer.Info.bmiHeader.biSize = sizeof(Buffer.Info.bmiHeader);
	Buffer.Info.bmiHeader.biWidth = Buffer.Width;
	Buffer.Info.bmiHeader.biHeight = -Buffer.Height; // negative is top-down bitmap and origin is upper-left.
	Buffer.Info.bmiHeader.biPlanes = 1;
	Buffer.Info.bmiHeader.biBitCount = 32;
	Buffer.Info.bmiHeader.biCompression = BI_RGB;

	// NOTE(andre): Thank you to Chris Hecker of Spy Party fame for clarifying
	// the deal with StretchBits and BitBlt!
	// No more DC for us.

	int BitmapMemorySize = (Buffer.Width * Buffer.Height) * Buffer.BytesPerPixel;
	Buffer.Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);

	Buffer.Pitch = Width * Buffer.BytesPerPixel;

	// TODO(andre): probably clear this to black.
}

internal void
Win32DisplayBufferInWindow(win32_offscreen_buffer &Buffer, HDC DeviceContext,
						   int WindowWidth, int WindowHeight)
{
	// TODO(andre): aspect ratio correction

	// rectangle to rectangle copy
	StretchDIBits(
		DeviceContext,
		0, 0, WindowWidth, WindowHeight,	// Destination		
		0, 0, Buffer.Width, Buffer.Height,	// Source
		Buffer.Memory,
		&Buffer.Info,
		DIB_RGB_COLORS, SRCCOPY);
}

internal LRESULT CALLBACK
Win32MainWindowCallback(HWND Window,
						UINT Message,
						WPARAM WParam,
						LPARAM LParam)
{
	LRESULT Result = 0;

	switch (Message)
	{
		case WM_CLOSE:
		{
			// TODO(andre): handle this with a message to the user?
			GlobalRunning = false;
			OutputDebugStringA("WM_CLOSE\n");
		} break;

		case WM_ACTIVATEAPP:
		{
			OutputDebugStringA("WM_ACTIVATEAPP\n");
		} break;

		case WM_DESTROY:
		{
			// TODO(andrea): handle this as an error - recreate window?
			GlobalRunning = false;
			OutputDebugStringA("WM_DESTROY\n");
		} break;

		case WM_SIZE:
		{
			OutputDebugStringA("WM_SIZE\n");
		} break;

		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			uint32 VKCode = WParam;  						// WParam has the virtual key code.
			bool WasDown = ((LParam & (1 << 30)) != 0);      	// the 30th bit of LParam has the previous key state.
			bool IsDown = ((LParam & (1 << 31)) == 0);

			if(WasDown != IsDown)
			{
				if(VKCode == 'W')
				{
					OutputDebugStringA("W\n");
				}
				else if(VKCode == 'S')
				{
					OutputDebugStringA("W\n");
				}
				else if(VKCode == 'A')
				{
					OutputDebugStringA("W\n");
				}
				else if(VKCode == 'D')
				{
					OutputDebugStringA("W\n");
				}
				else if(VKCode == VK_UP)
				{
					OutputDebugStringA("W\n");
				}
				else if(VKCode == VK_DOWN)
				{
					OutputDebugStringA("W\n");
				}
				else if(VKCode == VK_LEFT)
				{
					OutputDebugStringA("W\n");
				}
				else if(VKCode == VK_RIGHT)
				{
					OutputDebugStringA("W\n");
				}
				else if(VKCode == VK_ESCAPE)
				{
					OutputDebugStringA("ESCAPE: ");
					if(IsDown)
					{
						OutputDebugStringA("is down\n");
					}
					if(WasDown)
					{
						OutputDebugStringA("was down\n");
					}
				}
				else if(VKCode == VK_SPACE)
				{
					OutputDebugStringA("W\n");
				}
			}
		} break;
		
		case WM_PAINT:
		{
			PAINTSTRUCT Paint;
			HDC DeviceContext = BeginPaint(Window, &Paint);
			int X = Paint.rcPaint.left;
			int Y = Paint.rcPaint.top;
			int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
			int Width = Paint.rcPaint.right - Paint.rcPaint.left;

			win32_window_dimension Dimension = Win32GetWindowDimension(Window);
			Win32DisplayBufferInWindow(GlobalBackbuffer, DeviceContext,
									   Dimension.Width, Dimension.Height);
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
	HINSTANCE Instance,
	HINSTANCE PrevInstance,
	PSTR CommandLine,
	INT ShowCode)
{
	Win32LoadXInput();

	WNDCLASSA WindowClass = {};

	Win32ResizeDIBSection(GlobalBackbuffer, 1288, 728);

	WindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;	   // re-paint the whole window, not just the new part
	WindowClass.lpfnWndProc = Win32MainWindowCallback; // Window Procedure
	WindowClass.hInstance = Instance;
	WindowClass.lpszClassName = "HandmadHeroWindowClass";

	if (RegisterClassA(&WindowClass))
	{
		HWND Window = CreateWindowExA(
			0,
			WindowClass.lpszClassName,
			"Handmade Hero",
			WS_OVERLAPPEDWINDOW | WS_VISIBLE,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			0,
			0,
			Instance,
			0);
		if (Window)
		{
			// NOTE(andre): Since we specified CS_OWNDC, we can just get one 
			// device context and use it forever because we are not sharing it
			// with anyone.
			HDC DeviceContext = GetDC(Window);
			
			int XOffset = 0;
			int YOffset = 0;

			GlobalRunning = true;
			while (GlobalRunning)
			{
				MSG Message;
				while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
				{
					if (Message.message == WM_QUIT)
					{
						GlobalRunning = false;
					}
					TranslateMessage(&Message);
					DispatchMessageA(&Message);
				}

				// TODO(andre): should we poll this more frequently?
				for (DWORD ControllerIndex = 0;
					ControllerIndex < XUSER_MAX_COUNT;
					++ControllerIndex)
				{
					XINPUT_STATE ControllerState;
					if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
					{
						// NOTE(andre): This controller is plugged in
						// TODO(andre): see if ControllerState.dwPacketNumber increments too rapidly
						XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;
						
						bool Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
						bool Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
						bool Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
						bool Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
						bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
						bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
						bool LeftShoulder = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
						bool RightShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
						bool AButton = (Pad->wButtons & XINPUT_GAMEPAD_A);
						bool BButton = (Pad->wButtons & XINPUT_GAMEPAD_B);
						bool XButton = (Pad->wButtons & XINPUT_GAMEPAD_X);
						bool YButton = (Pad->wButtons & XINPUT_GAMEPAD_Y);

						int16 StickX = Pad->sThumbLX;
						int16 StrickY = Pad->sThumbLY;

						if (AButton)
						{
							++YOffset;
						}
					}
					else
					{
						// NOTE(andre): The controller is not available
					}
				}

				XINPUT_VIBRATION Vibration;
				Vibration.wLeftMotorSpeed  = 60000;
				Vibration.wRightMotorSpeed = 60000;
				XInputSetState(0, &Vibration);

				RenderWeirdGradient(GlobalBackbuffer, XOffset, YOffset);

				HDC DeviceContext = GetDC(Window);

				win32_window_dimension Dimension = Win32GetWindowDimension(Window);
				Win32DisplayBufferInWindow(GlobalBackbuffer, DeviceContext,
										   Dimension.Width, Dimension.Height);
				ReleaseDC(Window, DeviceContext);

				XOffset += 1;
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