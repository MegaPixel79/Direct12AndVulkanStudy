
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h> // For CommandLineToArgvW


// The min/max macros conflict with like-named member functions.
// Only use std::min and std::max defined in <algorithm>.
#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

// In order to define a function called CreateWindow, the Windows macro needs to
// be undefined.
#if defined(CreateWindow)
#undef CreateWindow
#endif

// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
#include <wrl.h>

// DirectX 12 specific headers.
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

// D3D12 extension library.
#include "d3dx12.h"


// STL Headers
#include <algorithm>
#include <cassert>
#include <chrono>

#include "Helpers.h"

#if _DEBUG

//Set the compiler define D3DCOMPILE_DEBUG to tell the HLSL compiler to include debug information into the shader blob.
#if defined(D3DCOMPILE_DEBUG)
#undef D3DCOMPILE_DEBUG
#endif

#define D3DCOMPILE_DEBUG 1

#endif

using namespace Microsoft::WRL;


// The number of swap chain back buffers.
const uint8_t gNumFrames = 3;

// Use WARP adapter
bool gUseWarp = false;

uint32_t gClientWidth = 1280;
uint32_t gClientHeight = 720;

// Set to true once the DX12 objects have been initialized.
bool gIsInitialized = false;

//Is the App running
bool gAppIsRunning = true;

// Window handle.
HWND ghWnd;

// Window rectangle (used to toggle fullscreen state).
RECT gWindowRect;

// DirectX 12 Objects
ComPtr<ID3D12Device2> gDevice;
ComPtr<ID3D12CommandQueue> gCommandQueue;
ComPtr<IDXGISwapChain4> gSwapChain;
ComPtr<ID3D12Resource> gBackBuffers[gNumFrames];
ComPtr<ID3D12GraphicsCommandList> gCommandList;
ComPtr<ID3D12CommandAllocator> gCommandAllocators[gNumFrames];
ComPtr<ID3D12DescriptorHeap> gRTVDescriptorHeap;
UINT gRTVDescriptorSize;
UINT gCurrentBackBufferIndex;

// Synchronization objects
ComPtr<ID3D12Fence> gFence;
uint64_t gFenceValue = 0;
uint64_t gFrameFenceValues[gNumFrames] = {};
HANDLE   gFenceEvent;

// By default, enable V-Sync.
// Can be toggled with the V key.
bool gVSync = true;
bool gTearingSupported = false;

// By default, use windowed mode.
// Can be toggled with the Alt+Enter or F11
bool gFullscreen = false;


// Window callback function.
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);


void ParseCommandLineArguments()
{
	int32_t argc;
	wchar_t** argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);

	for (int32_t i = 0; i < argc; ++i)
	{
		if (::wcscmp(argv[i], L"-w") == 0 || ::wcscmp(argv[i], L"--width") == 0)
		{
			gClientWidth = ::wcstol(argv[++i], nullptr, 10);
		}
		if (::wcscmp(argv[i], L"-h") == 0 || ::wcscmp(argv[i], L"--height") == 0)
		{
			gClientHeight = ::wcstol(argv[++i], nullptr, 10);
		}
		if (::wcscmp(argv[i], L"-warp") == 0 || ::wcscmp(argv[i], L"--warp") == 0)
		{
			gUseWarp = true;
		}
	}

	// Free memory allocated by CommandLineToArgvW
	::LocalFree(argv);
}

void EnableDebugLayer()
{
#if defined(_DEBUG)
	// Always enable the debug layer before doing anything DX12 related
	// so all possible errors generated while creating DX12 objects
	// are caught by the debug layer.
	ComPtr<ID3D12Debug> debugInterface;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
	debugInterface->EnableDebugLayer();
#endif
}

void RegisterWindowClass(HINSTANCE hInst, const wchar_t* windowClassName)
{
	// Register a window class for creating our render window with.
	WNDCLASSEXW windowClass = {};

	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = &WndProc;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = hInst;
	windowClass.hIcon = ::LoadIcon(hInst, NULL);
	windowClass.hCursor = ::LoadCursor(NULL, IDC_ARROW);
	windowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	windowClass.lpszMenuName = NULL;
	windowClass.lpszClassName = windowClassName;
	windowClass.hIconSm = ::LoadIcon(hInst, NULL);

	static ATOM atom = ::RegisterClassExW(&windowClass);
	assert(atom > 0);
}

HWND CreateWindow(const wchar_t* windowClassName, HINSTANCE hInst,
	const wchar_t* windowTitle, uint32_t width, uint32_t height)
{
	int screenWidth = ::GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = ::GetSystemMetrics(SM_CYSCREEN);

	RECT windowRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
	::AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

	int windowWidth = windowRect.right - windowRect.left;
	int windowHeight = windowRect.bottom - windowRect.top;

	// Center the window within the screen. Clamp to 0, 0 for the top-left corner.
	int windowX = std::max<int>(0, (screenWidth - windowWidth) / 2);
	int windowY = std::max<int>(0, (screenHeight - windowHeight) / 2);

	HWND hWnd = ::CreateWindowExW(
		NULL,
		windowClassName,
		windowTitle,
		WS_OVERLAPPEDWINDOW,
		windowX,
		windowY,
		windowWidth,
		windowHeight,
		NULL,
		NULL,
		hInst,
		nullptr
	);

	assert(hWnd && "Failed to create window");

	return hWnd;
}


//DX12 Boilerplate code 

ComPtr<IDXGIAdapter4> GetAdapter(bool useWarp)
{
	ComPtr<IDXGIFactory4> dxgiFactory;
	UINT createFactoryFlags = 0;
#if defined(_DEBUG)
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

	ComPtr<IDXGIAdapter1> dxgiAdapter1;
	ComPtr<IDXGIAdapter4> dxgiAdapter4;

	if (useWarp)
	{
		ThrowIfFailed(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter1)));
		ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
	}
	else
	{
		SIZE_T maxDedicatedVideoMemory = 0;
		for (UINT i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i)
		{
			DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
			dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

			// Check to see if the adapter can create a D3D12 device without actually 
			// creating it. The adapter with the largest dedicated video memory
			// is favored.
			if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
				SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(),
					D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)) &&
				dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory)
			{
				maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
				ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
			}
		}
	}

	return dxgiAdapter4;
}

ComPtr<ID3D12Device2> CreateDevice(ComPtr<IDXGIAdapter4> adapter)
{
	ComPtr<ID3D12Device2> d3d12Device2;

	ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12Device2)));

	// Enable debug messages in debug mode.
#if defined(_DEBUG)
	ComPtr<ID3D12InfoQueue> pInfoQueue;
	if (SUCCEEDED(d3d12Device2.As(&pInfoQueue)))
	{
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

		// Suppress whole categories of messages
	 //D3D12_MESSAGE_CATEGORY Categories[] = {};

	 // Suppress messages based on their severity level
		D3D12_MESSAGE_SEVERITY Severities[] =
		{
			D3D12_MESSAGE_SEVERITY_INFO
		};

		// Suppress individual messages by their ID
		D3D12_MESSAGE_ID DenyIds[] = {
			D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
			D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
			D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
		};

		D3D12_INFO_QUEUE_FILTER NewFilter = {};
		//NewFilter.DenyList.NumCategories = _countof(Categories);
		//NewFilter.DenyList.pCategoryList = Categories;
		NewFilter.DenyList.NumSeverities = _countof(Severities);
		NewFilter.DenyList.pSeverityList = Severities;
		NewFilter.DenyList.NumIDs = _countof(DenyIds);
		NewFilter.DenyList.pIDList = DenyIds;

		ThrowIfFailed(pInfoQueue->PushStorageFilter(&NewFilter));
	}
#endif

	return d3d12Device2;
}

//Before creating the swap chain, the command queue must be created first.

//Follows the function that creates a command queue

ComPtr<ID3D12CommandQueue> CreateCommandQueue(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type)
{
	ComPtr<ID3D12CommandQueue> d3d12CommandQueue;

	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type = type;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.NodeMask = 0;

	ThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&d3d12CommandQueue)));

	return d3d12CommandQueue;
}

//Support in in the Windows Display Driver Model (WDDM) for variable refresh rates was added in version 2.1. 
//WDDM 2.1 was added in the Windows 10 Anniversary Update (version 1607) which introduced the DXGI 1.5 API to the Windows 10 SDK.

//We check for tearing support in order to know wheter we are allowed to create the right swap chain to support for variable refresh rate for displays that supports it.

bool CheckTearingSupport()
{
	BOOL allowTearing = FALSE;

	// Rather than create the DXGI 1.5 factory interface directly, we create the
	// DXGI 1.4 interface and query for the 1.5 interface. This is to enable the 
	// graphics debugging tools which will not support the 1.5 factory interface 
	// until a future update.
	ComPtr<IDXGIFactory4> factory4;
	if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory4))))
	{
		ComPtr<IDXGIFactory5> factory5;
		if (SUCCEEDED(factory4.As(&factory5)))
		{
			if (FAILED(factory5->CheckFeatureSupport(
				DXGI_FEATURE_PRESENT_ALLOW_TEARING, // <-- If we support this feature our adapter can support displays with variable refresh rate
				&allowTearing, sizeof(allowTearing))))
			{
				allowTearing = FALSE;
			}
		}
	}

	return allowTearing == TRUE;
}

//We are ready to create the swap chain 

ComPtr<IDXGISwapChain4> CreateSwapChain(HWND     hWnd
	                                  , ComPtr<ID3D12CommandQueue> CommandQueue
	                                  , uint32_t Width
	                                  , uint32_t Height
	                                  , uint32_t BufferCount)
{
	ComPtr<IDXGISwapChain4> dxgiSwapChain4;
	ComPtr<IDXGIFactory4> dxgiFactory4;

	UINT CreateFactoryFlags = 0;

#if defined(_DEBUG)
	CreateFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	ThrowIfFailed( CreateDXGIFactory2(CreateFactoryFlags, IID_PPV_ARGS(&dxgiFactory4)) );

	DXGI_SWAP_CHAIN_DESC1 SwapChainDesc = {};
	SwapChainDesc .Width       = Width;
	SwapChainDesc .Height      = Height;
	SwapChainDesc .Format      = DXGI_FORMAT_R8G8B8A8_UNORM; //If we want to let the hw perform gamma correction for us we should use _SRGB format instead
	SwapChainDesc .Stereo      = FALSE;
	SwapChainDesc .SampleDesc  = { 1, 0 };
	SwapChainDesc .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	SwapChainDesc .BufferCount = BufferCount;
	SwapChainDesc .Scaling     = DXGI_SCALING_STRETCH;
	SwapChainDesc .SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	SwapChainDesc .AlphaMode   = DXGI_ALPHA_MODE_UNSPECIFIED;

	// It is recommended to always allow tearing if tearing support is available.
	SwapChainDesc .Flags       = CheckTearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	//Create Swapchain
	ComPtr<IDXGISwapChain1> swapChain1;
	ThrowIfFailed(dxgiFactory4->CreateSwapChainForHwnd(	CommandQueue.Get(),
														hWnd,
														&SwapChainDesc,
														nullptr,
														nullptr,
														&swapChain1) );

	// Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen
	// will be handled manually.
	ThrowIfFailed(dxgiFactory4->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));

	ThrowIfFailed(swapChain1.As(&dxgiSwapChain4));

	return dxgiSwapChain4;
}

//Ready to create a Descriptor Heap

ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(  ComPtr<ID3D12Device2> Device
												  , D3D12_DESCRIPTOR_HEAP_TYPE Type
	                                              , uint32_t NumDescriptors)
{
	ComPtr<ID3D12DescriptorHeap> DescriptorHeap;

	D3D12_DESCRIPTOR_HEAP_DESC Desc = {};
	Desc.NumDescriptors = NumDescriptors;
	Desc.Type           = Type;

	ThrowIfFailed( Device->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(&DescriptorHeap)) );

	return DescriptorHeap;
}

//We define a function to create the RTV for each backbuffer of our swapchain
//For each back buffer of the swap chain, a single RTV is used to describe the resource.

void UpdateRenderTargetViews( ComPtr<ID3D12Device2>        device
	                        , ComPtr<IDXGISwapChain4>      swapChain
	                        , ComPtr<ID3D12DescriptorHeap> descriptorHeap)
{
	auto RTVDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	CD3DX12_CPU_DESCRIPTOR_HANDLE RTVHandle(descriptorHeap->GetCPUDescriptorHandleForHeapStart());

	for (int i = 0; i < gNumFrames; ++i)
	{
		ComPtr<ID3D12Resource> backBuffer;
		ThrowIfFailed(swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));

		device->CreateRenderTargetView(backBuffer.Get(), nullptr, RTVHandle);

		gBackBuffers[i] = backBuffer;

		RTVHandle.Offset(RTVDescriptorSize);
	}
}

//Create a Command Allocator
/*  
    A command allocator is the backing memory used by a command list. 
	A command allocator is created using the ID3D12Device::CreateCommandAllocator method and must specify the type of command list the allocator will be used with. 	
	The command allocator does not provide any functionality and can only be accessed indirectly through a command list.
	The CreateCommandAllocator function is used to create the command allocator for the application. 	
	In order to achieve maximum frame-rates for the application, one command allocator per "in-flight" command list should be created. 	
	The CreateCommandAllocator function shown here only creates a single command allocator but this function will be used later to create multiple allocators for the demo.
*/

ComPtr<ID3D12CommandAllocator> CreateCommandAllocator( ComPtr<ID3D12Device2>   Device
	                                                 , D3D12_COMMAND_LIST_TYPE Type)
{
	ComPtr<ID3D12CommandAllocator> CommandAllocator;
	ThrowIfFailed( Device->CreateCommandAllocator(Type, IID_PPV_ARGS(&CommandAllocator)) );
	return CommandAllocator;
}


//Create a Command List

//The CreateCommandList function is used to create a command list for the application.

ComPtr<ID3D12GraphicsCommandList> CreateCommandList(  ComPtr<ID3D12Device2> Device
	                                                , ComPtr<ID3D12CommandAllocator> CommandAllocator
	                                                , D3D12_COMMAND_LIST_TYPE Type)
{
	ComPtr<ID3D12GraphicsCommandList> CommandList;

	ThrowIfFailed( Device->CreateCommandList(0, Type, CommandAllocator.Get(), nullptr, IID_PPV_ARGS(&CommandList)) );

	//Before to reset a command list, we must close it.
	ThrowIfFailed( CommandList->Close() );

	return CommandList;
}


//The next functions deal with GPU synchronization.

ComPtr<ID3D12Fence> CreateFence(ComPtr<ID3D12Device2> Device)
{
	ComPtr<ID3D12Fence> Fence;

	ThrowIfFailed( Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Fence)) );

	return Fence;
}

//An OS event handle is used to allow the CPU thread to wait until the fence has been signaled with a particular value.
//In the next section, the OS event handle is created.

HANDLE CreateEventHandle()
{
	HANDLE FenceEvent;

	FenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(FenceEvent && "Failed to create fence event.");

	return FenceEvent;
}

//Signal the Fence

/*
	The Signal function is used to signal the fence **FROM** the GPU. It should be noted that when using the ID3D12CommandQueue::Signal method to signal a fence from the GPU, the fence is not signaled immediatly but is only signaled once the GPU command queue has reached that point during execution. 
	Any commands that have been queued before the signal method was invoked must complete execution before the fence will be signaled.
*/

uint64_t Signal(   ComPtr<ID3D12CommandQueue> CommandQueue
	             , ComPtr<ID3D12Fence> Fence
	             , uint64_t& FenceValue)
{
	uint64_t FenceValueForSignal = ++FenceValue;
	ThrowIfFailed(CommandQueue->Signal(Fence.Get(), FenceValueForSignal));

	return FenceValueForSignal;
}


//Wait for Fence Value

//CPU builds a command list and send it to the command queue on the GPU and wait for a fence value. 
//As soon as the GPU reach that fence, after having executed all the queued commands, it'll signal that fence.

void WaitForFenceValue( ComPtr<ID3D12Fence> Fence
	                 ,  uint64_t FenceValue
	                 ,  HANDLE FenceEvent
	                 ,  std::chrono::milliseconds Duration = std::chrono::milliseconds::max() )
{
	if (Fence->GetCompletedValue() < FenceValue)
	{
		ThrowIfFailed(Fence->SetEventOnCompletion(FenceValue, FenceEvent));
		::WaitForSingleObject(FenceEvent, static_cast<DWORD>(Duration.count()));
	}
}


//Flush the GPU

/*
	It is sometimes useful to wait until all previously executed commands have finished executing before doing something 
	(for example, resizing the swap chain buffers requires any references to the buffers to be released). 
	For this, the Flush function is used to ensure the GPU has finished processing all commands before continuing.
*/

void Flush(  ComPtr<ID3D12CommandQueue> CommandQueue
	       , ComPtr<ID3D12Fence> Fence
	       , uint64_t& FenceValue
	       , HANDLE FenceEvent)
{
	uint64_t FenceValueForSignal = Signal(CommandQueue, Fence, FenceValue);
	WaitForFenceValue(Fence, FenceValueForSignal, FenceEvent);
}


//Typical Update function

void Update()
{
	static uint64_t frameCounter = 0;
	static double elapsedSeconds = 0.0;
	static std::chrono::high_resolution_clock clock;
	static auto t0 = clock.now();

	frameCounter++;
	auto t1 = clock.now();
	auto deltaTime = t1 - t0;
	t0 = t1;

	elapsedSeconds += deltaTime.count() * 1e-9;
	if (elapsedSeconds > 1.0)
	{
		char buffer[500];
		auto fps = frameCounter / elapsedSeconds;
		sprintf_s(buffer, 500, "FPS: %f\n", fps);
		OutputDebugString(buffer);

		frameCounter = 0;
		elapsedSeconds = 0.0;
	}
}




void Render(float* ClearColor)
{
	//Beginning of the frame
	auto commandAllocator = gCommandAllocators[gCurrentBackBufferIndex];
	auto backBuffer = gBackBuffers[gCurrentBackBufferIndex];

	//Before any commands can be recorded into the command list, the command allocator and command list needs to be reset to their initial state.
	commandAllocator->Reset();
	gCommandList->Reset(commandAllocator.Get(), nullptr);

	//Before the render target can be cleared, it must be transitioned to the RENDER_TARGET state.

	// Clear the render target.
	{
		CD3DX12_RESOURCE_BARRIER Barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

		gCommandList->ResourceBarrier(1, &Barrier);
	
		//FLOAT ClearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };

		//Now the back buffer can be cleared.
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(gRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), gCurrentBackBufferIndex, gRTVDescriptorSize);

		gCommandList->ClearRenderTargetView(rtv, ClearColor, 0, nullptr);	
	}



	//Perform draws/dispatch here




	// Present
	{
		CD3DX12_RESOURCE_BARRIER Barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

		gCommandList->ResourceBarrier(1, &Barrier);
		
		//After transitioning to the correct state, the command list that contains the resource transition barrier must be executed on the command queue.
		ThrowIfFailed( gCommandList->Close() );

		ID3D12CommandList* const commandLists[] = { gCommandList.Get() };

		gCommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);	


		UINT SyncInterval = gVSync ? 1 : 0;
		UINT PresentFlags = gTearingSupported && !gVSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
		
		ThrowIfFailed( gSwapChain->Present(SyncInterval, PresentFlags) );

		gFrameFenceValues[gCurrentBackBufferIndex] = Signal(gCommandQueue, gFence, gFenceValue);

		//After signaling the command queue, the index of the current back buffer is updated.
		gCurrentBackBufferIndex = gSwapChain->GetCurrentBackBufferIndex();

		//Before overwriting the contents of the current back buffer with the content of the next frame, the CPU thread is stalled using the WaitForFenceValue function described earlier.
		WaitForFenceValue(gFence, gFrameFenceValues[gCurrentBackBufferIndex], gFenceEvent);
	}

}

//Resize swap chain buffer if either fullscreen or the user just resizes the window
void Resize(uint32_t width, uint32_t height)
{
	if (gClientWidth != width || gClientHeight != height)
	{
		// Don't allow 0 size swap chain back buffers.
		gClientWidth = std::max(1u, width);
		gClientHeight = std::max(1u, height);

		// Flush the GPU queue to make sure the swap chain's back buffers
		// are not being referenced by an in-flight command list.
		Flush(gCommandQueue, gFence, gFenceValue, gFenceEvent);

		for (int i = 0; i < gNumFrames; ++i)
		{
			// Any references to the back buffers must be released
			// before the swap chain can be resized.
			gBackBuffers[i].Reset();
			gFrameFenceValues[i] = gFrameFenceValues[gCurrentBackBufferIndex];
		}

		DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
		ThrowIfFailed(gSwapChain->GetDesc(&swapChainDesc));
		ThrowIfFailed(gSwapChain->ResizeBuffers( gNumFrames, gClientWidth, gClientHeight, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags) );

		gCurrentBackBufferIndex = gSwapChain->GetCurrentBackBufferIndex();

		UpdateRenderTargetViews(gDevice, gSwapChain, gRTVDescriptorHeap);
	}
}

//Setting Fullscreen mode
void SetFullscreen(bool fullscreen)
{
	if (gFullscreen != fullscreen)
	{
		gFullscreen = fullscreen;

		if (gFullscreen) // Switching to fullscreen.
		{
			// Store the current window dimensions so they can be restored 
			// when switching out of fullscreen state.
			::GetWindowRect(ghWnd, &gWindowRect);

			// Set the window style to a borderless window so the client area fills
			// the entire screen.
			UINT windowStyle = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
			::SetWindowLongW(ghWnd, GWL_STYLE, windowStyle);

			// Query the name of the nearest display device for the window.
			// This is required to set the fullscreen dimensions of the window
			// when using a multi-monitor setup.
			HMONITOR hMonitor = ::MonitorFromWindow(ghWnd, MONITOR_DEFAULTTONEAREST);
			MONITORINFOEX monitorInfo = {};
			monitorInfo.cbSize = sizeof(MONITORINFOEX);
			::GetMonitorInfo(hMonitor, &monitorInfo);

			::SetWindowPos(ghWnd, HWND_TOP,
				monitorInfo.rcMonitor.left,
				monitorInfo.rcMonitor.top,
				monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
				monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
				SWP_FRAMECHANGED | SWP_NOACTIVATE);

			::ShowWindow(ghWnd, SW_MAXIMIZE);
		}
		else
		{
			// Restore all the window decorators.
			::SetWindowLong(ghWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);

			::SetWindowPos(ghWnd, HWND_NOTOPMOST,
				gWindowRect.left,
				gWindowRect.top,
				gWindowRect.right - gWindowRect.left,
				gWindowRect.bottom - gWindowRect.top,
				SWP_FRAMECHANGED | SWP_NOACTIVATE);

			::ShowWindow(ghWnd, SW_NORMAL);
		}
	}
}

//WndProc
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (gIsInitialized)
	{
		switch (message)
		{
		case WM_PAINT:
		{
			//Update();
			//float Color[] = { 1.0f,0.0f,0.0f,1.0f };
			//Render(Color);
		}
			break; 

		case WM_SYSKEYDOWN:
		case WM_KEYDOWN:
		{
			bool AltButtonDown = (::GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

			switch (wParam)
			{
			case 'V':
				gVSync = !gVSync;
				break;
			case VK_ESCAPE:
				::PostQuitMessage(0);
				break;
			case VK_RETURN:
				
			case VK_F11:
				if (AltButtonDown)
				{
					SetFullscreen(!gFullscreen);
				}
				break;
			}
		}
		break;
		// The default window procedure will play a system notification sound 
		// when pressing the Alt+Enter keyboard combination if this message is 
		// not handled.
		case WM_SYSCHAR:
			break;

		case WM_QUIT:
			gAppIsRunning = false;
			break;

		case WM_SIZE:
		{
			RECT clientRect = {};
			::GetClientRect(ghWnd, &clientRect);

			int width = clientRect.right - clientRect.left;
			int height = clientRect.bottom - clientRect.top;

			Resize(width, height);
		}
		break;

		case WM_DESTROY:
			gAppIsRunning = false;
			break;

		default:
			return ::DefWindowProcW(hwnd, message, wParam, lParam);
		}
	}
	else
	{
		return ::DefWindowProcW(hwnd, message, wParam, lParam);
	}

	return 0;
}

//This program Entry Point
int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
	// Windows 10 Creators update adds Per Monitor V2 DPI awareness context.
	// Using this awareness context allows the client area of the window 
	// to achieve 100% scaling while still allowing non-client window content to 
	// be rendered in a DPI sensitive fashion.
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	// Window class name. Used for registering / creating the window.
	const wchar_t* windowClassName = L"DX12WindowClass";
	ParseCommandLineArguments();

	//Enable Debug Layer
	EnableDebugLayer();

	//Check for tearing support
	gTearingSupported = CheckTearingSupport();

	//Register window class
	RegisterWindowClass(hInstance, windowClassName);
	ghWnd = CreateWindow(windowClassName, hInstance, L"Learning DirectX 12",gClientWidth, gClientHeight);

	//Initialize the global window rect variable.
	::GetWindowRect(ghWnd, &gWindowRect);

	//DX12 Objects creation
	ComPtr<IDXGIAdapter4> dxgiAdapter4 = GetAdapter(gUseWarp);

	//Let's create a device
	gDevice = CreateDevice(dxgiAdapter4);

	//Let's create a command queue
	gCommandQueue = CreateCommandQueue(gDevice, D3D12_COMMAND_LIST_TYPE_DIRECT);

	gSwapChain = CreateSwapChain(ghWnd, gCommandQueue, gClientWidth, gClientHeight, gNumFrames);

	gCurrentBackBufferIndex = gSwapChain->GetCurrentBackBufferIndex();

	gRTVDescriptorHeap = CreateDescriptorHeap(gDevice, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, gNumFrames);

	gRTVDescriptorSize = gDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	UpdateRenderTargetViews(gDevice, gSwapChain, gRTVDescriptorHeap);

	//Command list and command allocator creation
	for (int i = 0; i < gNumFrames; ++i)
	{
		gCommandAllocators[i] = CreateCommandAllocator(gDevice, D3D12_COMMAND_LIST_TYPE_DIRECT);
	}
	gCommandList = CreateCommandList(gDevice, gCommandAllocators[gCurrentBackBufferIndex], D3D12_COMMAND_LIST_TYPE_DIRECT);


	//Create the dx12 fence
	gFence = CreateFence(gDevice);
	//Create the CPU event that we'll use to stall the CPU on the fence value (the fence value will get signaled from the GPU as soon as the GPU will reach the fence)
	gFenceEvent = CreateEventHandle();

	//Everything is initialized now
	gIsInitialized = true;

	//Finally show the window 
	::ShowWindow(ghWnd, SW_SHOW);

	//Enter Application realtime loop
	
	gAppIsRunning = true;
	while(gAppIsRunning) 
	{
		MSG Message = {};
		if (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
		{
			if (Message.message == WM_QUIT)
			{
				gAppIsRunning = false;
			}

			TranslateMessage(&Message);
			DispatchMessage(&Message);
		}

		//Do any game specific update/render here
		float Color[] = { 0.0f,1.0f,0.0f,1.0f };
		Update();
		Render(Color);
	}

	// Make sure the command queue has finished all commands before closing.
	Flush(gCommandQueue, gFence, gFenceValue, gFenceEvent);

	//Close the CPU event 
	::CloseHandle(gFenceEvent);

	return 0;
}
