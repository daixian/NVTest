#include "dx3dv9.h"
#include "windows.h"

#pragma comment(lib,"d3d9.lib")
#pragma comment(lib,"nvapi.lib")

#define D3D_EXCEPTION(M) printf("Error: %s\n", M); fflush(stdout); return;
#define D3D_EXCEPTIONF(F, ...) printf("Error: " F " %s\n", __VA_ARGS__); fflush(stdout); return;

dx3dv9::dx3dv9()
{
}


dx3dv9::~dx3dv9()
{
}

void dx3dv9::GLD3DBuffers_create(d3dvBuffers* d3dv_buff, void* window_handle, bool vsync, bool stereo)
{
    ZeroMemory(d3dv_buff, sizeof(d3dvBuffers));
    d3dv_buff->stereo = stereo;

    HWND hWnd = (HWND)window_handle;
    HRESULT result;

    // Fullscreen?
    WINDOWINFO windowInfo;
    result = GetWindowInfo(hWnd, &windowInfo);
    if (FAILED(result)) {
        D3D_EXCEPTION("Could not get window info");
    }
    BOOL fullscreen = (windowInfo.dwExStyle & WS_EX_TOPMOST) != 0;
    if (fullscreen) {
        printf(" We are in fullscreen mode: %lu\n", hWnd);

        // Doesn't work well :/
        fullscreen = FALSE;
    } else {
        printf(" We are in windowed mode\n");
    }

    // 窗口大小
    RECT windowRect;
    result = GetClientRect(hWnd, &windowRect);
    if (FAILED(result)) {
        D3D_EXCEPTION("Could not get window size");
    }
    unsigned int width = windowRect.right - windowRect.left;
    unsigned int height = windowRect.bottom - windowRect.top;
    d3dv_buff->width = width;
    d3dv_buff->height = height;
    printf(" Window size: %u, %u\n", width, height);

    HDC dc = GetDC(hWnd);

    // 是否我们在 WDDM OS?
    OSVERSIONINFO osVersionInfo;
    osVersionInfo.dwOSVersionInfoSize = sizeof(osVersionInfo);
    if (!GetVersionEx(&osVersionInfo)) {
        D3D_EXCEPTION("Could not get Windows version");
    }
    BOOL wddm = osVersionInfo.dwMajorVersion == 6;
    if (wddm) {
        printf(" This operating system uses WDDM (it's Windows Vista or later)\n");
    } else {
        printf(" This operating system does not use WDDM (it's prior to Windows Vista)\n");
    }

    IDirect3D9* d3d;
    if (wddm) {
        // We are dynamically linking to the Direct3DCreate9Ex function, because we
        // don't want to add a run-time dependency for it in our executable, which
        // would make it not run in Windows XP.
        //
        // See: http://msdn.microsoft.com/en-us/library/cc656710.aspx

        HMODULE d3dLibrary = LoadLibrary(L"d3d9.dll");
        if (!d3dLibrary) {
            D3D_EXCEPTION("Failed to link to d3d9.dll");
        }
        d3dv_buff->d3dLibrary = d3dLibrary;

        //下面这几句跑不起来
        //DIRECT3DCREATE9EXFUNCTION pfnDirect3DCreate9Ex = (DIRECT3DCREATE9EXFUNCTION)GetProcAddress(d3dLibrary, "Direct3DCreate9Ex");
        //if (!pfnDirect3DCreate9Ex) {
        //    D3D_EXCEPTION("Failed to link to Direct3D 9Ex (WDDM)");
        //}
        //IDirect3D9 *d3dEx;
        //result = (*pfnDirect3DCreate9Ex)(D3D_SDK_VERSION, (IDirect3D9Ex**)&d3dEx);
        //if (result != D3D_OK) {
        //    D3D_EXCEPTION("Failed to activate Direct3D 9Ex (WDDM)");
        //}

        //if (d3dEx->lpVtbl->QueryInterface(d3dEx, &IID_IDirect3D9, (LPVOID*)&d3d) != S_OK) {
        //    D3D_EXCEPTION("Failed to cast Direct3D 9Ex to Direct3D 9");
        //}

        //printf(" Activated Direct3D 9Ex (WDDM)\n");
    } else {
        d3d = Direct3DCreate9(D3D_SDK_VERSION);
        if (!d3d) {
            D3D_EXCEPTION("Failed to activate Direct3D 9 (no WDDM)");
        }
        printf(" Activated Direct3D 9 (no WDDM)\n");
    }

    // Find display mode format
    //
    // (Our buffers will be the same to avoid conversion overhead)
    D3DDISPLAYMODE d3dDisplayMode;
    result = d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3dDisplayMode);
    if (result != D3D_OK) {
        D3D_EXCEPTION("Failed to retrieve adapter display mode");
    }
    D3DFORMAT d3dBufferFormat = d3dDisplayMode.Format;
    printf(" Retrieved adapter display mode, format: %u\n", d3dBufferFormat);

    // Make sure devices can support the required formats
    result = d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3dBufferFormat,
                                    D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D16);
    if (result != D3D_OK) {
        D3D_EXCEPTION("Our required formats are not supported");
    }
    printf(" Our required formats are supported\n");

    // Get the device capabilities
    D3DCAPS9 d3dCaps;
    result = d3d->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &d3dCaps);
    if (result != D3D_OK) {
        D3D_EXCEPTION("Failed to retrieve device capabilities");
    }
    printf(" Retrieved device capabilities\n");

    // Verify that we can do hardware vertex processing
    if (d3dCaps.VertexProcessingCaps == 0) {
        D3D_EXCEPTION("Hardware vertex processing is not supported");
    }
    printf(" Hardware vertex processing is supported\n");

    // Register stereo (must happen *before* device creation)
    if (d3dv_buff->stereo) {
        if (NvAPI_Initialize() != NVAPI_OK) {
            D3D_EXCEPTION("Failed to initialize NV API");
        }
        printf(" Initialized NV API:\n");

        if (NvAPI_Stereo_CreateConfigurationProfileRegistryKey(NVAPI_STEREO_DX9_REGISTRY_PROFILE) != NVAPI_OK) {
            D3D_EXCEPTION("Failed to register stereo profile");
        }
        printf("  Registered stereo profile\n");

        if (NvAPI_Stereo_Enable() != NVAPI_OK) {
            D3D_EXCEPTION("Could not enable NV stereo");
        }
        printf("  Enabled stereo\n");
    }

    D3DPRESENT_PARAMETERS d3dPresent;
    ZeroMemory(&d3dPresent, sizeof(d3dPresent));
    d3dPresent.BackBufferCount = 1;
    d3dPresent.BackBufferFormat = d3dBufferFormat;
    d3dPresent.BackBufferWidth = width;
    d3dPresent.BackBufferHeight = height;
    d3dPresent.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dPresent.MultiSampleQuality = 0;
    d3dPresent.MultiSampleType = D3DMULTISAMPLE_NONE;
    d3dPresent.EnableAutoDepthStencil = TRUE;
    d3dPresent.AutoDepthStencilFormat = D3DFMT_D16;
    d3dPresent.Flags = D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL;
    d3dPresent.PresentationInterval = vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
    if (fullscreen) {
        //d3dPresent.FullScreen_RefreshRateInHz = 60;
        d3dPresent.Windowed = FALSE;
    } else {
        d3dPresent.Windowed = TRUE;
        d3dPresent.hDeviceWindow = hWnd; // can be NULL in windowed mode, in which case it will use the arg in CreateDevice
    }
}