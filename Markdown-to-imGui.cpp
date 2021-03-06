// Dear ImGui: standalone example application for DirectX 11
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#include "imGui/imgui.h"
#include "imGui/backends/imgui_impl_win32.h"
#include "imGui/backends/imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>

#pragma comment(lib,"d3d11.lib")

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <deque>

typedef void (*ImGuiDemoMarkerCallback)(const char* file, int line, const char* section, void* user_data);
extern ImGuiDemoMarkerCallback  GImGuiDemoMarkerCallback2;
extern void* GImGuiDemoMarkerCallback2UserData;
ImGuiDemoMarkerCallback         GImGuiDemoMarkerCallback2 = NULL;
void* GImGuiDemoMarkerCallback2UserData = NULL;
#define IMGUI_DEMO_MARKER(section)  do { if (GImGuiDemoMarkerCallback2 != NULL) GImGuiDemoMarkerCallback2(__FILE__, __LINE__, section, GImGuiDemoMarkerCallback2UserData); } while (0)

// Data
static ID3D11Device* g_pd3dDevice = NULL;
static ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
static IDXGISwapChain* g_pSwapChain = NULL;
static ID3D11RenderTargetView* g_mainRenderTargetView = NULL;

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

void GetDesktopResolution(int& horizontal, int& vertical)
{
    RECT desktop;

    // Get a handle to the desktop window
    const HWND hDesktop = GetDesktopWindow();

    // Get the size of screen to the variable desktop
    GetWindowRect(hDesktop, &desktop);

    // The top left corner will have coordinates (0,0)
    // and the bottom right corner will have coordinates
    // (horizontal, vertical)
    horizontal = desktop.right;
    vertical = desktop.bottom;
}

double screenDpi()
{
    SetProcessDPIAware(); //true

    HDC screen = GetDC(NULL);

    double hPixelsPerInch = GetDeviceCaps(screen, LOGPIXELSX);
    double vPixelsPerInch = GetDeviceCaps(screen, LOGPIXELSY);

    ReleaseDC(NULL, screen);

    return (hPixelsPerInch + vPixelsPerInch) * 0.5 / 96;
}

void applyUserLang(ImGuiIO& io, double dpi)
{
    switch (GetUserDefaultUILanguage() & 0x3ff)
    {
    case LANG_JAPANESE:
        io.Fonts->AddFontFromFileTTF("font/NotoSansJP-Regular.otf", 18 * dpi, NULL, io.Fonts->GetGlyphRangesJapanese());
        break;
    case LANG_CHINESE_TRADITIONAL: // need some fix
        io.Fonts->AddFontFromFileTTF("font/NotoSansTC-Regular.otf", 18 * dpi, NULL, io.Fonts->GetGlyphRangesChineseFull());
        break;
    case LANG_CHINESE_SIMPLIFIED:
        io.Fonts->AddFontFromFileTTF("font/NotoSansSC-Regular.otf", 18 * dpi, NULL, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
        break;
    case LANG_KOREAN:
        io.Fonts->AddFontFromFileTTF("font/NotoSansKR-Regular.otf", 18 * dpi, NULL, io.Fonts->GetGlyphRangesKorean());
        break;
    default:
        io.Fonts->AddFontFromFileTTF("font/NotoSans-Regular.ttf", 18 * dpi, NULL, io.Fonts->GetGlyphRangesCyrillic());
        break;
    }

    io.Fonts->Build();
}

// Simple helper function to load an image into a DX11 texture with common settings
bool LoadTextureFromFile(const char* filename, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height)
{
    // Load from disk into a raw RGBA buffer
    int image_width = 0;
    int image_height = 0;
    unsigned char* image_data = stbi_load(filename, &image_width, &image_height, NULL, 4);
    if (image_data == NULL)
        return false;

    // Create texture
    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = image_width;
    desc.Height = image_height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;

    ID3D11Texture2D* pTexture = NULL;
    D3D11_SUBRESOURCE_DATA subResource;
    subResource.pSysMem = image_data;
    subResource.SysMemPitch = desc.Width * 4;
    subResource.SysMemSlicePitch = 0;
    g_pd3dDevice->CreateTexture2D(&desc, &subResource, &pTexture);

    // Create texture view
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = desc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;
    g_pd3dDevice->CreateShaderResourceView(pTexture, &srvDesc, out_srv);
    pTexture->Release();

    *out_width = image_width;
    *out_height = image_height;
    stbi_image_free(image_data);

    return true;
}

std::deque<bool> check;

std::vector<ID3D11ShaderResourceView*> textures;
std::vector<int> textures_width;
std::vector<int> textures_height;

std::vector<std::string> read(std::string fname)
{
    std::ifstream ifs(fname);
    std::string str;

    std::vector<std::string> lines;

    int my_image_width = 0;
    int my_image_height = 0;
    int image_i = 0;

    if (ifs.fail()) {
        return lines;
    }
    while (getline(ifs, str)) {
        lines.push_back(str);
        if (str.size() > 0 && str.c_str()[0] == '!' && str.find("(") && str.find(")"))
        {
            ID3D11ShaderResourceView* temp;
            int width;
            int height;
            textures.push_back(temp);
            textures_width.push_back(width);
            textures_height.push_back(height);
            fname = str.substr(str.find_first_of("(") + 1, str.find_first_of(")") - 1 - str.find_first_of("("));
            LoadTextureFromFile(fname.c_str(), &textures.at(textures.size()-1), &textures_width.at(textures.size() - 1), &textures_height.at(textures.size() - 1));
        }
    }

    check.resize(lines.size());

    return lines;
}

int write_sentence(std::vector<std::string> lines, int start, int image_i)
{
    int i = start;
    std::string fname;

    int my_image_width = 0;
    int my_image_height = 0;

    if (lines[i].size() > 1 && lines[i].c_str()[0] == '[' && lines[i].c_str()[1] == ']')
    {
        ImGui::Checkbox(lines[i].replace(0, 2, "").c_str(), &check[i]);
        return ++i;
    }
    if (lines[i].size() > 0 && lines[i].c_str()[0] == '-')
    {
        ImGui::BulletText(lines[i].replace(0, 2, "").c_str());
        return ++i;
    }
    if (lines[i].size() > 0 && lines[i].c_str()[0] == '!' && lines[i].find("(") && lines[i].find(")"))
    {
        ImGui::Image((void*)textures.at(image_i), ImVec2(textures_width.at(image_i), textures_height.at(image_i)));

        return ++i;
    }
    ImGui::Text(lines[i].c_str());
    return ++i;
}

int write(std::vector<std::string> lines)
{
    int i = 0;
    int image_i = -1;
    int crntChptrName = -1;
    bool flg = true;
    bool node = true;

    while (i < (int)lines.size())
    {
        if (lines[i].size() > 0 && lines[i].c_str()[0] == '!' && lines[i].find("(") && lines[i].find(")"))
        {
            image_i++;
        }
        if (lines[i].size() > 1 && lines[i].c_str()[0] == '#' && lines[i].c_str()[1] != '#')
        {
            IMGUI_DEMO_MARKER(lines[i].replace(0, 1, "").c_str());
            flg = ImGui::CollapsingHeader(lines[i].replace(0, 1, "").c_str());
            node = true;
            crntChptrName = i;
            i++;
            continue;
        }
        if (!flg)
        {
            i++;
            continue;
        }
        if (lines[i].size() > 1 && lines[i].c_str()[0] == '#' && lines[i].c_str()[1] == '#')
        {
            IMGUI_DEMO_MARKER((lines[crntChptrName].replace(0, 1, "") + "/" + lines[i].replace(0, 2, "")).c_str());
            node = ImGui::TreeNode(lines[i].replace(0, 2, "").c_str());
            if (node)
            {
                i++;
                while (i < (int)lines.size())
                {
                    if (lines[i].size() > 0 && lines[i].c_str()[0] == '!' && lines[i].find("(") && lines[i].find(")"))
                    {
                        image_i++;
                    }
                    if (lines[i].size() > 0 && lines[i].c_str()[0] == '#')
                    {
                        break;
                    }
                    i = write_sentence(lines, i, image_i);
                }
                ImGui::TreePop();
            }
            continue;
        }
        if (!node)
        {
            i++;
            continue;
        }
        i = write_sentence(lines, i, image_i);
    }

    return i;
}

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Main code
int main(int, char**)
{
    int horizontal;
    int vertical;

    GetDesktopResolution(horizontal, vertical);

    // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("ImGui Example"), NULL };
    ::RegisterClassEx(&wc);
    HWND hwnd = ::CreateWindow(wc.lpszClassName, _T("Markdown to imgui example"), WS_OVERLAPPEDWINDOW, 0, 0, horizontal * screenDpi(), vertical * screenDpi(), NULL, NULL, wc.hInstance, NULL);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);



    //
    //
    // read markdown file.
    std::string fname;

    switch (GetUserDefaultUILanguage() & 0x3ff)
    {
    case LANG_JAPANESE:
        fname = "test-jp.md";
        break;
    default:
        fname = "test.md";
        break;
    }

    std::vector<std::string> lines = read(fname);
    // read markdown file end.
    //
    //

    //
    // Font has to be built before new frame.
    // https://github.com/ocornut/imgui/blob/master/docs/FONTS.md
    // select font depends on the user language.
    applyUserLang(io, screenDpi());
    //
    //
    //



    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        MSG msg;
        while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();

        ImGui::NewFrame();

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
        {
            //ImGui::ShowDemoWindow(&show_demo_window);
        }

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
        {
            ImGui::Begin("Markdown read test");

            ImGui::SetWindowFontScale(1.0f);

            write(lines);

            //ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0); // Present with vsync
        //g_pSwapChain->Present(0, 0); // Present without vsync
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}
