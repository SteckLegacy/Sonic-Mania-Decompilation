#ifndef GCMRENDERDEVICE_HPP
#define GCMRENDERDEVICE_HPP

class RenderDevice : public RenderDeviceBase
{
public:
    struct WindowInfo {
        struct {
            uint32 width;
            uint32 height;
            uint32 refresh_rate;
        } * displays;
        struct {
            uint32 x, y, width, height;
            float minZ, maxZ;
        } viewport;
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

    static void SetWindowTitle() {}
    static void ShowCursor(bool32 shown) {}
    static bool GetCursorPos(Vector2 *pos)
    {
        if (pos) {
            pos->x = 0;
            pos->y = 0;
        }
        return true;
    }

    // GCM / RSX Specific context variables
    static void *gcmContext;
    static void *hostCmdBuffer;
    static uint32 hostCmdBufferSize;

    static void *frameBufferLocalMem[2];
    static uint32 frameBufferOffset[2];
    static uint32 frameBufferPitch;
    static uint32 currentBuffer;

    static void *depthBufferLocalMem;
    static uint32 depthBufferOffset;

    static void *textureLocalMem;
    static uint32 textureOffset;
    static uint32 textureWidth;
    static uint32 textureHeight;

    static void *vertexBufferLocalMem;
    static uint32 vertexBufferOffset;

    static void *screenTextureLocalMem[SCREEN_COUNT];
    static uint32 screenTextureOffset[SCREEN_COUNT];
    static uint32 screenTexturePitch;

    static void *rsxFreeLocalMem;
    static uint32 rsxFreeLocalMemSize;

    static const uint8 *vertexShaderBytes;
    static uint32 vertexShaderSize;
    static void *fragmentShaderLocalMem;
    static uint32 fragmentShaderOffset;
    static uint32 fragmentShaderSize;

    static uint64 targetFreq;
    static uint64 curTicks;
    static uint64 prevTicks;

    static RenderVertex vertexBuffer[60];

private:
    static bool SetupRendering();
    static void InitVertexBuffer();
    static bool InitGraphicsAPI();
    static void GetDisplays();
    static void SetRenderTarget();
};

struct ShaderEntry : public ShaderEntryBase {
    void *vertexProgram;
    void *fragmentProgram;
    uint32 fragmentOffset;
};

#endif // GCMRENDERDEVICE_HPP
