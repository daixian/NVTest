﻿//--------------------------------------------------------------------------------------
// File: Tutorial02.cpp
//
// This application displays a triangle using Direct3D 11
//
// http://msdn.microsoft.com/en-us/library/windows/apps/ff729719.aspx
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <windows.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <directxcolors.h>
#include "resource.h"

//#include "nvapi.h"
//#pragma comment(lib, "nvapi.lib")


#define RELEASE_D3D11_1_OBJECT(o)   \
  if (o != NULL)                    \
  {                                 \
    o->Release();                   \
    o = NULL;                       \
  }



using namespace DirectX;

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct SimpleVertex
{
    XMFLOAT3 Pos;
};


//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
HINSTANCE               g_hInst = nullptr;
HWND                    g_hWnd = nullptr;
D3D_DRIVER_TYPE         g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL       g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device*           g_pd3dDevice = nullptr;
ID3D11Device1*          g_pd3dDevice1 = nullptr;
ID3D11DeviceContext*    g_pImmediateContext = nullptr;
ID3D11DeviceContext1*   g_pImmediateContext1 = nullptr;
IDXGISwapChain*         g_pSwapChain = nullptr;
IDXGISwapChain1*        g_pSwapChain1 = nullptr;

ID3D11RenderTargetView* g_backBufferRenderTargetView = nullptr;
ID3D11DepthStencilView*   g_backBufferDepthStencilView = NULL;
ID3D11RenderTargetView*   g_rightRenderTargetView = NULL;

ID3D11Buffer*             g_staticShaderConstantsBuffer = NULL;
ID3D11Buffer*             g_dynamicShaderConstantsBuffer = NULL;

ID3D11VertexShader*     g_pVertexShader = nullptr;
ID3D11PixelShader*      g_pPixelShader = nullptr;
ID3D11InputLayout*      g_pVertexLayout = nullptr;
ID3D11Buffer*           g_pVertexBuffer = nullptr;


//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HRESULT InitWindow( HINSTANCE hInstance, int nCmdShow );
HRESULT InitDevice();
void CleanupDevice();
LRESULT CALLBACK    WndProc( HWND, UINT, WPARAM, LPARAM );
void Render();

void clearScene(int eye);
void setRenderTarget(int eye);
bool createBackBufferViews();
//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow )
{
    UNREFERENCED_PARAMETER( hPrevInstance );
    UNREFERENCED_PARAMETER( lpCmdLine );

    if( FAILED( InitWindow( hInstance, nCmdShow ) ) )
        return 0;

    if( FAILED( InitDevice() ) ) {
        CleanupDevice();
        return 0;
    }

    // Main message loop
    MSG msg = {0};
    while( WM_QUIT != msg.message ) {
        if( PeekMessage( &msg, nullptr, 0, 0, PM_REMOVE ) ) {
            TranslateMessage( &msg );
            DispatchMessage( &msg );
        } else {
            Render();
        }
    }

    CleanupDevice();

    return ( int )msg.wParam;
}


//--------------------------------------------------------------------------------------
// Register class and create window
//--------------------------------------------------------------------------------------
HRESULT InitWindow( HINSTANCE hInstance, int nCmdShow )
{
    // Register class
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof( WNDCLASSEX );
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon( hInstance, ( LPCTSTR )IDI_TUTORIAL1 );
    wcex.hCursor = LoadCursor( nullptr, IDC_ARROW );
    wcex.hbrBackground = ( HBRUSH )( COLOR_WINDOW + 1 );
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = L"TutorialWindowClass";
    wcex.hIconSm = LoadIcon( wcex.hInstance, ( LPCTSTR )IDI_TUTORIAL1 );
    if( !RegisterClassEx( &wcex ) )
        return E_FAIL;

    // Create window
    g_hInst = hInstance;
    RECT rc = { 0, 0, 800, 600 };
    AdjustWindowRect( &rc, WS_OVERLAPPEDWINDOW, FALSE );
    g_hWnd = CreateWindow( L"TutorialWindowClass", L"Direct3D 11 Tutorial 2: Rendering a Triangle",
                           WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                           CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance,
                           nullptr );
    if( !g_hWnd )
        return E_FAIL;

    ShowWindow( g_hWnd, nCmdShow );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Helper for compiling shaders with D3DCompile
//
// With VS 11, we could load up prebuilt .cso files instead...
//--------------------------------------------------------------------------------------
HRESULT CompileShaderFromFile( const WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut )
{
    HRESULT hr = S_OK;

    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows
    // the shaders to be optimized and to run exactly the way they will run in
    // the release configuration of this program.
    dwShaderFlags |= D3DCOMPILE_DEBUG;

    // Disable optimizations to further improve shader debugging
    dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ID3DBlob* pErrorBlob = nullptr;
    hr = D3DCompileFromFile( szFileName, nullptr, nullptr, szEntryPoint, szShaderModel,
                             dwShaderFlags, 0, ppBlobOut, &pErrorBlob );
    if( FAILED(hr) ) {
        if( pErrorBlob ) {
            OutputDebugStringA( reinterpret_cast<const char*>( pErrorBlob->GetBufferPointer() ) );
            pErrorBlob->Release();
        }
        return hr;
    }
    if( pErrorBlob ) pErrorBlob->Release();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create Direct3D device and swap chain
//--------------------------------------------------------------------------------------
HRESULT InitDevice()
{

    HRESULT hr = S_OK;

    RECT rc;
    GetClientRect( g_hWnd, &rc );
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_DRIVER_TYPE driverTypes[] = {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };
    UINT numDriverTypes = ARRAYSIZE( driverTypes );

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
    };
    UINT numFeatureLevels = ARRAYSIZE( featureLevels );

    for( UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++ ) {
        g_driverType = driverTypes[driverTypeIndex];
        hr = D3D11CreateDevice( nullptr, g_driverType, nullptr, createDeviceFlags, featureLevels, numFeatureLevels,
                                D3D11_SDK_VERSION, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext );

        if ( hr == E_INVALIDARG ) {
            // DirectX 11.0 platforms will not recognize D3D_FEATURE_LEVEL_11_1 so we need to retry without it
            hr = D3D11CreateDevice( nullptr, g_driverType, nullptr, createDeviceFlags, &featureLevels[1], numFeatureLevels - 1,
                                    D3D11_SDK_VERSION, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext );
        }

        if( SUCCEEDED( hr ) )
            break;
    }
    if( FAILED( hr ) )
        return hr;

    // Obtain DXGI factory from device (since we used nullptr for pAdapter above)
    IDXGIFactory1* dxgiFactory = nullptr;
    {
        IDXGIDevice* dxgiDevice = nullptr;
        hr = g_pd3dDevice->QueryInterface( __uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice) );
        if (SUCCEEDED(hr)) {
            IDXGIAdapter* adapter = nullptr;
            hr = dxgiDevice->GetAdapter(&adapter);
            if (SUCCEEDED(hr)) {
                hr = adapter->GetParent( __uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory) );
                adapter->Release();
            }
            dxgiDevice->Release();
        }
    }
    if (FAILED(hr))
        return hr;

    // Create swap chain
    IDXGIFactory2* dxgiFactory2 = nullptr;
    hr = dxgiFactory->QueryInterface( __uuidof(IDXGIFactory2), reinterpret_cast<void**>(&dxgiFactory2) );
    if ( dxgiFactory2 ) {
        // DirectX 11.1 or later
        hr = g_pd3dDevice->QueryInterface( __uuidof(ID3D11Device1), reinterpret_cast<void**>(&g_pd3dDevice1) );
        if (SUCCEEDED(hr)) {
            (void) g_pImmediateContext->QueryInterface( __uuidof(ID3D11DeviceContext1), reinterpret_cast<void**>(&g_pImmediateContext1) );
        }

        BOOL isWinStereo = dxgiFactory2->IsWindowedStereoEnabled();
        if (!isWinStereo)
        {
            MessageBox(nullptr,
                L"IsWindowedStereoEnabled = false.", L"Error", MB_OK);
        }


        DXGI_SWAP_CHAIN_DESC1 sd = {};
        sd.Width = width;
        sd.Height = height;
        sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount = 2;
        sd.Stereo = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

        // Set up the full screen swap chain descriptor.
        DXGI_RATIONAL refreshRate;
        refreshRate.Numerator = 120;
        refreshRate.Denominator = 1;

        DXGI_SWAP_CHAIN_FULLSCREEN_DESC swapChainFullScreenDesc;
        swapChainFullScreenDesc.RefreshRate = refreshRate;
        swapChainFullScreenDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
        swapChainFullScreenDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
        swapChainFullScreenDesc.Windowed = FALSE; //窗口模式


        hr = dxgiFactory2->CreateSwapChainForHwnd( g_pd3dDevice, g_hWnd, &sd, nullptr, nullptr, &g_pSwapChain1 );
        if (SUCCEEDED(hr)) {
            hr = g_pSwapChain1->QueryInterface( __uuidof(IDXGISwapChain), reinterpret_cast<void**>(&g_pSwapChain) );
        }

        dxgiFactory2->Release();
    } else {
        // DirectX 11.0 systems
        DXGI_SWAP_CHAIN_DESC sd = {}; //描述一个交换链
        sd.BufferCount = 1;
        sd.BufferDesc.Width = width;
        sd.BufferDesc.Height = height;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 120;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = g_hWnd;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.Windowed = TRUE;

        hr = dxgiFactory->CreateSwapChain( g_pd3dDevice, &sd, &g_pSwapChain );
    }

    // Note this tutorial doesn't handle full-screen swapchains so we block the ALT+ENTER shortcut
    dxgiFactory->MakeWindowAssociation( g_hWnd, DXGI_MWA_NO_ALT_ENTER );

    dxgiFactory->Release();

    if (FAILED(hr))
        return hr;

    //// Create a render target view
    //ID3D11Texture2D* pBackBuffer = nullptr;
    //hr = g_pSwapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), reinterpret_cast<void**>( &pBackBuffer ) );
    //if( FAILED( hr ) )
    //    return hr;

    //hr = g_pd3dDevice->CreateRenderTargetView( pBackBuffer, nullptr, &g_pRenderTargetView );
    //pBackBuffer->Release();
    //if( FAILED( hr ) )
    //    return hr;

    //g_pImmediateContext->OMSetRenderTargets( 1, &g_pRenderTargetView, nullptr );

    if (!createBackBufferViews()) {
        return hr;
    }

    // Setup the viewport
    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)width;
    vp.Height = (FLOAT)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    g_pImmediateContext->RSSetViewports( 1, &vp );

    // Compile the vertex shader
    ID3DBlob* pVSBlob = nullptr;
    hr = CompileShaderFromFile( L"Tutorial02.fx", "VS", "vs_4_0", &pVSBlob );
    if( FAILED( hr ) ) {
        MessageBox( nullptr,
                    L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK );
        return hr;
    }

    // Create the vertex shader
    hr = g_pd3dDevice->CreateVertexShader( pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &g_pVertexShader );
    if( FAILED( hr ) ) {
        pVSBlob->Release();
        return hr;
    }

    // Define the input layout
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT numElements = ARRAYSIZE( layout );

    // Create the input layout
    hr = g_pd3dDevice->CreateInputLayout( layout, numElements, pVSBlob->GetBufferPointer(),
                                          pVSBlob->GetBufferSize(), &g_pVertexLayout );
    pVSBlob->Release();
    if( FAILED( hr ) )
        return hr;

    // Set the input layout
    g_pImmediateContext->IASetInputLayout( g_pVertexLayout );

    // Compile the pixel shader
    ID3DBlob* pPSBlob = nullptr;
    hr = CompileShaderFromFile( L"Tutorial02.fx", "PS", "ps_4_0", &pPSBlob );
    if( FAILED( hr ) ) {
        MessageBox( nullptr,
                    L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK );
        return hr;
    }

    //// Create the pixel shader
    hr = g_pd3dDevice->CreatePixelShader( pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &g_pPixelShader );
    pPSBlob->Release();
    if( FAILED( hr ) )
        return hr;

    // Create vertex buffer
    SimpleVertex vertices[] = {
        XMFLOAT3( 0.0f, 0.5f, 0.5f ),
        XMFLOAT3( 0.5f, -0.5f, 0.5f ),
        XMFLOAT3( -0.5f, -0.5f, 0.5f ),
    };
    D3D11_BUFFER_DESC bd = {}; //描述一个buffer资源
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof( SimpleVertex ) * 3;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA InitData = {};
    InitData.pSysMem = vertices;
    hr = g_pd3dDevice->CreateBuffer( &bd, &InitData, &g_pVertexBuffer );
    if( FAILED( hr ) )
        return hr;

    // Set vertex buffer
    UINT stride = sizeof( SimpleVertex );
    UINT offset = 0;
    g_pImmediateContext->IASetVertexBuffers( 0, 1, &g_pVertexBuffer, &stride, &offset );

    // Set primitive topology
    g_pImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Clean up the objects we've created
//--------------------------------------------------------------------------------------
void CleanupDevice()
{
    if( g_pImmediateContext ) g_pImmediateContext->ClearState();

    if( g_pVertexBuffer ) g_pVertexBuffer->Release();
    if( g_pVertexLayout ) g_pVertexLayout->Release();
    if( g_pVertexShader ) g_pVertexShader->Release();
    if( g_pPixelShader ) g_pPixelShader->Release();
    //if( g_pRenderTargetView ) g_pRenderTargetView->Release();

    RELEASE_D3D11_1_OBJECT(g_staticShaderConstantsBuffer);
    RELEASE_D3D11_1_OBJECT(g_dynamicShaderConstantsBuffer);

    RELEASE_D3D11_1_OBJECT(g_backBufferRenderTargetView);
    RELEASE_D3D11_1_OBJECT(g_backBufferDepthStencilView);
    RELEASE_D3D11_1_OBJECT(g_rightRenderTargetView);

    if( g_pSwapChain1 ) g_pSwapChain1->Release();
    if( g_pSwapChain ) g_pSwapChain->Release();
    if( g_pImmediateContext1 ) g_pImmediateContext1->Release();
    if( g_pImmediateContext ) g_pImmediateContext->Release();
    if( g_pd3dDevice1 ) g_pd3dDevice1->Release();
    if( g_pd3dDevice ) g_pd3dDevice->Release();
}


//--------------------------------------------------------------------------------------
// Called every time the application receives a message
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    PAINTSTRUCT ps;
    HDC hdc;

    switch( message ) {
    case WM_PAINT:
        hdc = BeginPaint( hWnd, &ps );
        EndPaint( hWnd, &ps );
        break;

    case WM_DESTROY:
        PostQuitMessage( 0 );
        break;

    // Note that this tutorial does not handle resizing (WM_SIZE) requests,
    // so we created the window without the resize border.

    default:
        return DefWindowProc( hWnd, message, wParam, lParam );
    }

    return 0;
}

//--------------------------------------------------------------------------------------
// Render a frame
//--------------------------------------------------------------------------------------
void Render()
{
    setRenderTarget(0);

    // Clear the back buffer
    //g_pImmediateContext->ClearRenderTargetView(g_backBufferRenderTargetView, Colors::MidnightBlue );
    clearScene(0);

    // Render a triangle
    g_pImmediateContext->VSSetShader( g_pVertexShader, nullptr, 0 );
    g_pImmediateContext->PSSetShader( g_pPixelShader, nullptr, 0 );
    g_pImmediateContext->Draw( 3, 0 );

    setRenderTarget(1);

    clearScene(1);

    g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
    g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);
    g_pImmediateContext->Draw(3, 0);

    // Present the information rendered to the back buffer to the front buffer (the screen)
    g_pSwapChain->Present( 1, 0 );
}

void setRenderTarget(int eye)
{
    switch (eye) {
    case 0:
        g_pImmediateContext1->OMSetRenderTargets(1, &g_backBufferRenderTargetView, nullptr); //g_backBufferDepthStencilView 这里如果设置了深度那么就会不存在currentRenderTargetView
        g_backBufferRenderTargetView->Release();
        break;
    case 1:
        g_pImmediateContext1->OMSetRenderTargets(1, &g_rightRenderTargetView, nullptr);
        g_rightRenderTargetView->Release();
        break;
    default:
        break;
    }
}


void clearScene(int eye)
{
    ID3D11RenderTargetView* currentRenderTargetView;
    g_pImmediateContext1->OMGetRenderTargets(1, &currentRenderTargetView, NULL);

    // Clear the back buffer.
    if (currentRenderTargetView) {
        if (eye == 0) {
            float clearColor[4] = { 0.0f, 1.0f, 0.0f, 1.0f }; // red, green, blue, alpha
            g_pImmediateContext1->ClearRenderTargetView(currentRenderTargetView, clearColor);
            g_pImmediateContext1->ClearDepthStencilView(g_backBufferDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
        } else {
            float clearColor[4] = { 1.0f, 0.0f, 1.0f, 1.0f }; // red, green, blue, alpha
            g_pImmediateContext1->ClearRenderTargetView(currentRenderTargetView, clearColor);
            g_pImmediateContext1->ClearDepthStencilView(g_backBufferDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
        }

    }
}

bool createBackBufferViews()
{
    // Obtain the back buffer for this window which will be used
    // for the final 3D render target.
    ID3D11Texture2D* backBuffer = NULL;
    if (FAILED(g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))))
        return false;

    //为后台缓冲区的RenderTargetView设置描述符。
    CD3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc(D3D11_RTV_DIMENSION_TEXTURE2DARRAY, DXGI_FORMAT_B8G8R8A8_UNORM, 0, 0, 1);

    //在渲染目标上创建一个视图界面，用于单声道或左眼视图的绑定。
    if (FAILED(g_pd3dDevice->CreateRenderTargetView(backBuffer, nullptr, &g_backBufferRenderTargetView))) { //只能为null，否则失败
        backBuffer->Release();
        return false;
    }

    // 立体交换链具有阵列资源，因此创建第二个渲染目标。
    CD3D11_RENDER_TARGET_VIEW_DESC renderTargetViewRightDesc(D3D11_RTV_DIMENSION_TEXTURE2DARRAY, DXGI_FORMAT_B8G8R8A8_UNORM, 0, 1, 1);
    if (FAILED(g_pd3dDevice->CreateRenderTargetView(backBuffer, nullptr, &g_rightRenderTargetView))) {
        backBuffer->Release();
        return false;
    }

    // Release the back buffer.
    backBuffer->Release();

    // Cache the render target dimensions in our helper class for convenient use 在我们的帮助器类中缓存渲染目标维度以方便使用
    D3D11_TEXTURE2D_DESC backBufferDesc = { 0 };
    backBuffer->GetDesc(&backBufferDesc);

    // Set up the descriptor for the depth/stencil buffer. 设置深度/模板缓冲区的描述符。
    CD3D11_TEXTURE2D_DESC depthStencilDesc(
        DXGI_FORMAT_D24_UNORM_S8_UINT,
        backBufferDesc.Width,
        backBufferDesc.Height,
        1,
        1,
        D3D11_BIND_DEPTH_STENCIL);

    // Allocate a 2-D surface as the depth/stencil buffer. 将2-D表面分配为深度/模板缓冲区。
    ID3D11Texture2D* depthStencil = NULL;
    if (FAILED(g_pd3dDevice->CreateTexture2D(&depthStencilDesc, NULL, &depthStencil)))
        return false;

    // Create a depth stencil view on this surface to use on bind. 在此曲面上创建深度模板视图以在绑定时使用。
    CD3D11_DEPTH_STENCIL_VIEW_DESC viewDesc(D3D11_DSV_DIMENSION_TEXTURE2D);
    HRESULT isDepthStencilViewCreated = g_pd3dDevice->CreateDepthStencilView(depthStencil, &viewDesc, &g_backBufferDepthStencilView);
    depthStencil->Release();

    if (FAILED(isDepthStencilViewCreated))
        return false;

    // Create a viewport of the full window size. 创建完整窗口大小的视口。
    CD3D11_VIEWPORT viewport(
        0.0f,
        0.0f,
        static_cast<float>(backBufferDesc.Width),
        static_cast<float>(backBufferDesc.Height));

    // Set the current viewport.
    g_pImmediateContext1->RSSetViewports(1, &viewport);
    return true;
}