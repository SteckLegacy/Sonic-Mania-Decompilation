#ifndef TINY3D_RENDERDEVICE_HPP
#define TINY3D_RENDERDEVICE_HPP

using ShaderEntry = ShaderEntryBase;

class RenderDevice : public RenderDeviceBase
{
public:
    struct WindowInfo {
        union {
            struct {
                uint32 _pad;
                int32 width;
                int32 height;
                int32 refresh_rate;
            };
            SDL_DisplayMode internal;
        } * displays;
        SDL_Rect viewport;
    };
    static WindowInfo displayInfo;

    static bool Init();
    static void CopyFrameBuffer();
    static void FlipScreen();
    static void Release(bool32 isRefresh);

    static void RefreshWindow();
    static void GetWindowSize(int32 *width, int32 *height);

    static void SetupImageTexture(int32 width, int32 height, uint8 *imagePixels);
    static void SetupVideoTexture_YUV420(int32 width, int32 height, uint8 *yPlane, uint8 *uPlane, uint8 *vPlane, int32 strideY, int32 strideU,
                                         int32 strideV);
    static void SetupVideoTexture_YUV422(int32 width, int32 height, uint8 *yPlane, uint8 *uPlane, uint8 *vPlane, int32 strideY, int32 strideU,
                                         int32 strideV);
    static void SetupVideoTexture_YUV444(int32 width, int32 height, uint8 *yPlane, uint8 *uPlane, uint8 *vPlane, int32 strideY, int32 strideU,
                                         int32 strideV);

    static bool ProcessEvents();

    static void InitFPSCap();
    static bool CheckFPSCap();
    static void UpdateFPSCap();

    static bool InitShaders();
    static void LoadShader(const char *fileName, bool32 linear);

    static inline void ShowCursor(bool32 shown) { SDL_ShowCursor(shown); }
    static inline bool GetCursorPos(Vector2 *pos)
    {
        SDL_GetMouseState(&pos->x, &pos->y);
        return true;
    }

    static inline void SetWindowTitle() {}

    // Tiny3D RSX Texture Buffer Info
    struct Tiny3DTexture {
        void *pixels;
        u32 offset;
        uint32 width;
        uint32 height;
        uint32 pitch;
        text_format format;
    };

    static Tiny3DTexture screenTexture[SCREEN_COUNT];
    static Tiny3DTexture imageTexture;
    static Tiny3DTexture videoTextureY;
    static Tiny3DTexture videoTextureU;
    static Tiny3DTexture videoTextureV;

private:
    static bool SetupRendering();
    static void InitVertexBuffer();
    static bool InitGraphicsAPI();

    static void GetDisplays();
    static void ProcessEvent(SDL_Event event);

    static void DrawQuad(float x, float y, float w, float h, float u1, float v1, float u2, float v2, uint32 color);

    static uint32 displayModeIndex;
    static int32 displayModeCount;

    static unsigned long long targetFreq;
    static unsigned long long curTicks;
    static unsigned long long prevTicks;

    static RenderVertex vertexBuffer[!RETRO_REV02 ? 24 : 60];
    static uint8 lastTextureFormat;

    static bool tiny3dInitialized;
};

#endif // TINY3D_RENDERDEVICE_HPP
