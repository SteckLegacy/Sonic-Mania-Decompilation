#if RETRO_RENDERDEVICE_GCM

// Compat wrappers / fallbacks for PS3 SDK / PSL1GHT function variations
#ifndef cellVideoInit
#define cellVideoInit() ((void)0)
#endif

#ifndef cellVideoGetState
#define cellVideoGetState videoGetState
#endif

#ifndef cellVideoConfigure
#define cellVideoConfigure videoConfigure
#endif

#ifndef cellGcmInit
#define cellGcmInit(cmd_size, io_size, io_addr) \
    do { \
        static gcmContextData *_ctx = nullptr; \
        rsxInit(&_ctx, cmd_size, io_size, io_addr); \
        RenderDevice::gcmContext = _ctx; \
    } while (0)
#endif

#ifndef cellGcmAddressToOffset
#define cellGcmAddressToOffset(addr, offset_ptr) rsxAddressToOffset(addr, offset_ptr)
#endif

#ifndef cellGcmSetDisplayBuffer
#define cellGcmSetDisplayBuffer(id, offset, pitch, width, height) gcmSetDisplayBuffer(id, offset, pitch, width, height)
#endif

#ifndef cellGcmSetWaitFlip
#define cellGcmSetWaitFlip(ctx) gcmSetWaitFlip((gcmContextData*)(ctx))
#endif

#ifndef cellGcmFlush
#define cellGcmFlush(ctx) rsxFlushBuffer((gcmContextData*)(ctx))
#endif

#ifndef cellGcmSetFlipCommand
#define cellGcmSetFlipCommand(ctx, id) gcmSetFlip((gcmContextData*)(ctx), id)
#endif

#ifndef GCM_SURFACE_TYPE_LINEAR
#define GCM_SURFACE_TYPE_LINEAR (1)
#endif
#ifndef GCM_SURFACE_X8R8G8B8
#define GCM_SURFACE_X8R8G8B8 (0x80)
#endif
#ifndef GCM_SURFACE_TARGET_0
#define GCM_SURFACE_TARGET_0 (0x01)
#endif
#ifndef GCM_LOCATION_LOCAL
#define GCM_LOCATION_LOCAL (0)
#endif
#ifndef GCM_SURFACE_ZETA_Z24S8
#define GCM_SURFACE_ZETA_Z24S8 (0x02)
#endif
#ifndef GCM_SURFACE_CENTER_1
#define GCM_SURFACE_CENTER_1 (1)
#endif
#ifndef GCM_LABEL_INDEX
#define GCM_LABEL_INDEX (255)
#endif
#ifndef GCM_TYPE_TRIANGLES
#define GCM_TYPE_TRIANGLES (5)
#endif

#if RETRO_PLATFORM != RETRO_PS3
#ifndef GCM_TRANSFER_LOCAL_TO_LOCAL
#define GCM_TRANSFER_LOCAL_TO_LOCAL (0)
#define GCM_TRANSFER_SURFACE (1)
#define GCM_TRANSFER_CONVERSION_TRUNCATE (0)
#define GCM_TRANSFER_SCALE_FORMAT_A8R8G8B8 (0)
#define GCM_TRANSFER_SURFACE_FORMAT_A8R8G8B8 (1)
#define GCM_TRANSFER_OPERATION_SRCCOPY (3)
#define GCM_TRANSFER_ORIGIN_CORNER (0)
#define GCM_TRANSFER_INTERPOLATOR_NEAREST (0)
#define rsxSetTransferScaleMode(ctx, mode, surf) ((void)0)
#define rsxSetTransferScaleSurface(ctx, scale, surf) ((void)0)
#endif
#endif


using namespace RSDK;

void *RenderDevice::gcmContext = nullptr;
void *RenderDevice::hostCmdBuffer = nullptr;
uint32 RenderDevice::hostCmdBufferSize = 1024 * 1024;

void *RenderDevice::frameBufferLocalMem[2] = {nullptr, nullptr};
uint32 RenderDevice::frameBufferOffset[2] = {0, 0};
uint32 RenderDevice::frameBufferPitch = 0;
uint32 RenderDevice::currentBuffer = 0;

void *RenderDevice::depthBufferLocalMem = nullptr;
uint32 RenderDevice::depthBufferOffset = 0;
static bool firstFrame = true;

void *RenderDevice::textureLocalMem = nullptr;
uint32 RenderDevice::textureOffset = 0;
uint32 RenderDevice::textureWidth = 0;
uint32 RenderDevice::textureHeight = 0;

void *RenderDevice::vertexBufferLocalMem = nullptr;
uint32 RenderDevice::vertexBufferOffset = 0;

void *RenderDevice::screenTextureLocalMem[SCREEN_COUNT] = {nullptr};
uint32 RenderDevice::screenTextureOffset[SCREEN_COUNT] = {0};
uint32 RenderDevice::screenTexturePitch = 0;

void *RenderDevice::rsxFreeLocalMem = nullptr;
uint32 RenderDevice::rsxFreeLocalMemSize = 0;

const uint8 *RenderDevice::vertexShaderBytes = nullptr;
uint32 RenderDevice::vertexShaderSize = 0;
void *RenderDevice::vertexShaderUcode = nullptr;
const void *RenderDevice::fragmentShaderHostDesc = nullptr;
void *RenderDevice::fragmentShaderLocalMem = nullptr;
uint32 RenderDevice::fragmentShaderOffset = 0;
uint32 RenderDevice::fragmentShaderSize = 0;

uint32 RenderDevice::labelVal = 1;

uint64 RenderDevice::targetFreq = 0;
uint64 RenderDevice::curTicks = 0;
uint64 RenderDevice::prevTicks = 0;

RenderVertex RenderDevice::vertexBuffer[60];

void RenderDevice::WaitFinish()
{
#if RETRO_PLATFORM == RETRO_PS3
    if (!gcmContext)
        return;
    gcmContextData *ctx = (gcmContextData *)gcmContext;
    rsxSetWriteBackendLabel(ctx, GCM_LABEL_INDEX, labelVal);
    rsxFlushBuffer(ctx);
    while (*(vu32 *)gcmGetLabelAddress(GCM_LABEL_INDEX) != labelVal)
        usleep(30);
    ++labelVal;
#endif
}

void RenderDevice::WaitRSXIdle()
{
#if RETRO_PLATFORM == RETRO_PS3
    if (!gcmContext)
        return;
    gcmContextData *ctx = (gcmContextData *)gcmContext;
    rsxSetWriteBackendLabel(ctx, GCM_LABEL_INDEX, labelVal);
    rsxSetWaitLabel(ctx, GCM_LABEL_INDEX, labelVal);
    ++labelVal;
    WaitFinish();
#endif
}

void RenderDevice::SetRenderTarget(uint32 bufferIndex)
{
#if RETRO_PLATFORM == RETRO_PS3
    if (!gcmContext)
        return;
    gcmContextData *ctx = (gcmContextData *)gcmContext;

    uint32 width = videoSettings.fsWidth ? (uint32)videoSettings.fsWidth : 1280;
    uint32 height = videoSettings.fsHeight ? (uint32)videoSettings.fsHeight : 720;

    gcmSurface sf;
    memset(&sf, 0, sizeof(sf));
    sf.colorFormat = GCM_SURFACE_X8R8G8B8;
    sf.colorTarget = GCM_SURFACE_TARGET_0;
    sf.colorLocation[0] = GCM_LOCATION_LOCAL;
    sf.colorOffset[0] = frameBufferOffset[bufferIndex];
    sf.colorPitch[0] = frameBufferPitch;

    sf.colorLocation[1] = GCM_LOCATION_LOCAL;
    sf.colorLocation[2] = GCM_LOCATION_LOCAL;
    sf.colorLocation[3] = GCM_LOCATION_LOCAL;
    sf.colorOffset[1] = 0;
    sf.colorOffset[2] = 0;
    sf.colorOffset[3] = 0;
    sf.colorPitch[1] = 64;
    sf.colorPitch[2] = 64;
    sf.colorPitch[3] = 64;

    sf.depthFormat = GCM_SURFACE_ZETA_Z24S8;
    sf.depthLocation = GCM_LOCATION_LOCAL;
    sf.depthOffset = depthBufferOffset;
    sf.depthPitch = frameBufferPitch;

    sf.type = GCM_SURFACE_TYPE_LINEAR;
    sf.antiAlias = GCM_SURFACE_CENTER_1;

    sf.width = width;
    sf.height = height;
    sf.x = 0;
    sf.y = 0;

    rsxSetSurface(ctx, &sf);
#endif
}

ShaderEntry shaderList[SHADER_COUNT];
int32 shaderCount = 0;

// Extern injected Cg shader binary bytes generated by bin2s / Makefile
extern "C" {
extern const uint8_t shader_vpo[] __attribute__((weak));
extern const uint8_t shader_vpo_end[] __attribute__((weak));
extern const uint8_t shader_fpo[] __attribute__((weak));
extern const uint8_t shader_fpo_end[] __attribute__((weak));
}

// Fallback embedded Cg shader microcode buffer
static uint32 cgVertexProgramUcode[64] __attribute__((aligned(16))) = {0};
static uint32 cgFragmentProgramUcode[64] __attribute__((aligned(16))) = {0};

void *RenderDevice::AllocLocalMemory(uint32 alignment, uint32 size)
{
    void *ptr = nullptr;
    uint32 alignedSize = Align64(size);
#if RETRO_PLATFORM == RETRO_PS3
    ptr = rsxMemalign(alignment, alignedSize);
    if (!ptr) {
        ptr = rsxMemalign(64, alignedSize);
    }
#endif
    if (!ptr) {
        ptr = memalign(alignment, alignedSize);
    }
    if (ptr) {
        memset(ptr, 0, alignedSize);
    }
    return ptr;
}

void RenderDevice::FreeLocalMemory(void *ptr)
{
    if (!ptr)
        return;
#if RETRO_PLATFORM == RETRO_PS3
    rsxFree(ptr);
#else
    free(ptr);
#endif
}

bool RenderDevice::AddressToOffset(void *addr, uint32 *offset)
{
    if (!addr || !offset)
        return false;
    return cellGcmAddressToOffset(addr, offset) == 0;
}

bool RenderDevice::InitGraphicsAPI()
{
    // REQ 1: Order of initialization for GCM:
    // 1) cellVideoInit()
    // 2) cellVideoGetState() / query video output
    // 3) cellVideoConfigure() / set native resolution
    // 4) cellGcmInit() / initialize GCM

    PrintLog(PRINT_NORMAL, "GCM: Starting RSX/GCM Graphics API initialization...");

#if RETRO_INPUTDEVICE_SDL2 || RETRO_AUDIODEVICE_SDL2
    SDL_InitSubSystem(SDL_INIT_EVENTS);
#endif

#if RETRO_PLATFORM == RETRO_PS3
    cellVideoInit();
    PrintLog(PRINT_NORMAL, "GCM: cellVideoInit() called.");

    videoState state;
    memset(&state, 0, sizeof(videoState));
    if (cellVideoGetState(0, 0, &state) != 0) {
        PrintLog(PRINT_NORMAL, "GCM: Failed to query video state.");
        return false;
    }
    PrintLog(PRINT_NORMAL, "GCM: Video state queried (resolution id: %d, state: %d).", state.displayMode.resolution, state.state);

    videoResolution res;
    memset(&res, 0, sizeof(videoResolution));
    if (videoGetResolution(state.displayMode.resolution, &res) != 0) {
        res.width = 1280;
        res.height = 720;
    }
    PrintLog(PRINT_NORMAL, "GCM: Target display resolution: %dx%d.", res.width, res.height);

    videoConfiguration vconfig;
    memset(&vconfig, 0, sizeof(videoConfiguration));
    vconfig.resolution = state.displayMode.resolution;
    vconfig.format = VIDEO_BUFFER_FORMAT_XRGB;
    vconfig.pitch = (res.width * 4 + 63) & ~63; // Declared in multiples of 64 bytes
    vconfig.aspect = VIDEO_ASPECT_16_9;

    if (cellVideoConfigure(0, &vconfig, NULL, 0) != 0) {
        PrintLog(PRINT_NORMAL, "GCM: Warning: cellVideoConfigure failed, continuing with current state.");
    }
    else {
        PrintLog(PRINT_NORMAL, "GCM: cellVideoConfigure() succeeded (pitch: %d).", vconfig.pitch);
    }

    // Wait until RSX video state is ready with timeout
    int timeout = 100;
    do {
        usleep(10000);
        memset(&state, 0, sizeof(videoState));
        if (cellVideoGetState(0, 0, &state) != 0)
            break;
    } while (state.state == 3 && --timeout > 0);

    videoSettings.fsWidth = res.width;
    videoSettings.fsHeight = res.height;

    viewSize.x = (float)res.width;
    viewSize.y = (float)res.height;

    int32 maxPixHeight = 0;
    int32 screenWidth = 0;
    for (int32 s = 0; s < SCREEN_COUNT; ++s) {
        if (videoSettings.pixHeight > maxPixHeight)
            maxPixHeight = videoSettings.pixHeight;

        screens[s].size.y = videoSettings.pixHeight;

        float viewAspect = viewSize.x / viewSize.y;
        screenWidth = (int32)((viewAspect * videoSettings.pixHeight) + 3) & 0xFFFFFFFC;
        if (screenWidth < videoSettings.pixWidth)
            screenWidth = videoSettings.pixWidth;

#if !RETRO_USE_ORIGINAL_CODE
        if (customSettings.maxPixWidth && screenWidth > customSettings.maxPixWidth)
            screenWidth = customSettings.maxPixWidth;
#else
        if (screenWidth > DEFAULT_PIXWIDTH)
            screenWidth = DEFAULT_PIXWIDTH;
#endif

        memset(&screens[s].frameBuffer, 0, sizeof(screens[s].frameBuffer));
        SetScreenSize(s, screenWidth, screens[s].size.y);
    }

    pixelSize.x = (float)screens[0].size.x;
    pixelSize.y = (float)screens[0].size.y;

    displayInfo.viewport.x = 0;
    displayInfo.viewport.y = 0;
    displayInfo.viewport.width = res.width;
    displayInfo.viewport.height = res.height;
    displayInfo.viewport.minZ = 0.0f;
    displayInfo.viewport.maxZ = 1.0f;

    // REQ 2: Allocate host command buffer with 1MB boundary alignment and size as multiple of 1024 bytes
    uint32 cmdBufferSize = 0x10000;  // 64KB command buffer (multiple of 1024)
    hostCmdBufferSize = 1024 * 1024; // 1MB shared IO memory (multiple of 1024)
    hostCmdBuffer = memalign(1024 * 1024, hostCmdBufferSize);
    if (!hostCmdBuffer) {
        PrintLog(PRINT_NORMAL, "GCM: Failed to allocate aligned host command buffer.");
        return false;
    }
    PrintLog(PRINT_NORMAL, "GCM: Host command buffer allocated (%p, %d bytes).", hostCmdBuffer, hostCmdBufferSize);

    cellGcmInit(cmdBufferSize, hostCmdBufferSize, hostCmdBuffer);
    PrintLog(PRINT_NORMAL, "GCM: cellGcmInit() completed.");

    // REQ 3: Framebuffer allocation in RSX local memory (VRAM):
    // - 24 MB total framebuffer pool (8 MB per buffer x 3: Color 0, Color 1, Depth/Z)
    // - Byte pitch aligned to multiples of 64 bytes
    // - Base address aligned to 64KB boundary
    uint32 width = res.width;
    uint32 height = res.height;
    frameBufferPitch = (width * 4 + 63) & ~63; // 64-byte pitch alignment
    uint32 perBufferFbSize = (24 * 1024 * 1024) / 3; // 8 MB per buffer (24 MB total pool, 64KB aligned)

    for (int i = 0; i < 2; ++i) {
        frameBufferLocalMem[i] = AllocLocalMemory(64 * 1024, perBufferFbSize);
        if (!frameBufferLocalMem[i]) {
            PrintLog(PRINT_NORMAL, "GCM: Failed to allocate 64KB-aligned RSX framebuffer %d.", i);
            return false;
        }

        AddressToOffset(frameBufferLocalMem[i], &frameBufferOffset[i]);
        cellGcmSetDisplayBuffer(i, frameBufferOffset[i], frameBufferPitch, width, height);
        PrintLog(PRINT_NORMAL, "GCM: Display buffer %d allocated at offset 0x%08X (addr: %p, pitch: %d).", i, frameBufferOffset[i], frameBufferLocalMem[i], frameBufferPitch);
    }

    // Allocate 32-bit Z-Buffer (Depth buffer) in RSX memory (8 MB)
    depthBufferLocalMem = AllocLocalMemory(64 * 1024, perBufferFbSize);
    if (depthBufferLocalMem) {
        AddressToOffset(depthBufferLocalMem, &depthBufferOffset);
        PrintLog(PRINT_NORMAL, "GCM: Depth buffer allocated at offset 0x%08X (addr: %p).", depthBufferOffset, depthBufferLocalMem);
    }

    // Allocate RSX vertex buffer memory
    vertexBufferLocalMem = AllocLocalMemory(128, sizeof(RenderVertex) * 60);
    if (vertexBufferLocalMem) {
        AddressToOffset(vertexBufferLocalMem, &vertexBufferOffset);
        PrintLog(PRINT_NORMAL, "GCM: Vertex buffer allocated at offset 0x%08X (addr: %p).", vertexBufferOffset, vertexBufferLocalMem);
    }

    // Allocate RSX screen textures
    screenTexturePitch = Align64(512 * 4);
    uint32 screenTexSize = screenTexturePitch * 512;
    for (int s = 0; s < SCREEN_COUNT; ++s) {
        screenTextureLocalMem[s] = AllocLocalMemory(64, screenTexSize);
        if (screenTextureLocalMem[s]) {
            AddressToOffset(screenTextureLocalMem[s], &screenTextureOffset[s]);
            PrintLog(PRINT_NORMAL, "GCM: Screen texture %d allocated at offset 0x%08X (pitch: %d).", s, screenTextureOffset[s], screenTexturePitch);
        }
    }

    // Set remaining free RSX local memory pool (200 MB)
    rsxFreeLocalMemSize = 200 * 1024 * 1024;

    currentBuffer = 0;
    firstFrame = true;

    // REQ 4: Cg Vertex and Fragment Shader setup & offset calculation
    vertexShaderBytes = shader_vpo ? shader_vpo : (const uint8 *)cgVertexProgramUcode;
    vertexShaderSize  = (shader_vpo && shader_vpo_end) ? (uint32)(shader_vpo_end - shader_vpo) : sizeof(cgVertexProgramUcode);
    vertexShaderUcode = nullptr;

    const void *fShaderData = shader_fpo ? (const void *)shader_fpo : (const void *)cgFragmentProgramUcode;
    fragmentShaderSize = (shader_fpo && shader_fpo_end) ? (uint32)(shader_fpo_end - shader_fpo) : sizeof(cgFragmentProgramUcode);
    fragmentShaderHostDesc = fShaderData;

    PrintLog(PRINT_NORMAL, "GCM: Loading injected shaders (vpo: %p, size: %d, fpo: %p, size: %d).",
             shader_vpo, vertexShaderSize, shader_fpo, fragmentShaderSize);

    if (vertexShaderBytes) {
        u32 vpsize = 0;
        rsxVertexProgramGetUCode((const rsxVertexProgram *)vertexShaderBytes, &vertexShaderUcode, &vpsize);
        PrintLog(PRINT_NORMAL, "GCM: Vertex shader ucode extracted: %p (size: %d).", vertexShaderUcode, vpsize);
    }

    void *fp_ucode = nullptr;
    u32 fpsize = 0;
    if (fragmentShaderHostDesc) {
        rsxFragmentProgramGetUCode((const rsxFragmentProgram *)fragmentShaderHostDesc, &fp_ucode, &fpsize);
        PrintLog(PRINT_NORMAL, "GCM: Fragment shader ucode extracted: %p (size: %d).", fp_ucode, fpsize);
    }

    if (!fp_ucode || !fpsize) {
        fp_ucode = (void *)fShaderData;
        fpsize = fragmentShaderSize;
    }

    fragmentShaderLocalMem = AllocLocalMemory(64, fpsize);
    if (fragmentShaderLocalMem) {
        memcpy(fragmentShaderLocalMem, fp_ucode, fpsize);
        if (!AddressToOffset(fragmentShaderLocalMem, &fragmentShaderOffset)) {
            PrintLog(PRINT_NORMAL, "GCM: Failed to map Cg fragment program address to RSX offset.");
        }
        else {
            PrintLog(PRINT_NORMAL, "GCM: Fragment shader microcode mapped to RSX offset 0x%08X.", fragmentShaderOffset);
        }
    }

    // Set GCM flip mode (VSYNC)
    gcmSetFlipMode(GCM_FLIP_VSYNC);

    WaitRSXIdle();

    SetRenderTarget(currentBuffer);
    PrintLog(PRINT_NORMAL, "GCM: Set initial render target %d successfully.", currentBuffer);

#endif // RETRO_PLATFORM == RETRO_PS3

    PrintLog(PRINT_NORMAL, "GCM: Graphics API initialization completed successfully.");
    return true;
}

bool RenderDevice::Init()
{
    if (!InitGraphicsAPI())
        return false;

    InitVertexBuffer();
    InitShaders();

    videoSettings.shaderSupport = true;
    videoSettings.screenCount = 1;
    videoSettings.windowState = WINDOWSTATE_ACTIVE;

    SetupRendering();

    return true;
}

void RenderDevice::InitVertexBuffer()
{
    RenderVertex vertBuffer[sizeof(rsdkVertexBuffer) / sizeof(RenderVertex)];
    memcpy(vertBuffer, rsdkVertexBuffer, sizeof(rsdkVertexBuffer));

    float viewW = videoSettings.fsWidth ? (float)videoSettings.fsWidth : 1280.0f;
    float viewH = videoSettings.fsHeight ? (float)videoSettings.fsHeight : 720.0f;
    float x = 0.5f / viewW;
    float y = 0.5f / viewH;

    int32 vertCount = (RETRO_REV02 ? 60 : 24) - 6;
    for (int32 v = 0; v < vertCount; ++v) {
        RenderVertex *vertex = &vertBuffer[v];
        vertex->pos.x = vertex->pos.x - x;
        vertex->pos.y = vertex->pos.y + y;

        if (vertex->tex.x)
            vertex->tex.x = screens[0].size.x * (1.0f / 512.0f);

        if (vertex->tex.y)
            vertex->tex.y = screens[0].size.y * (1.0f / 512.0f);
    }

    memcpy(vertexBuffer, vertBuffer, sizeof(vertBuffer));
    if (vertexBufferLocalMem) {
        memcpy(vertexBufferLocalMem, vertBuffer, sizeof(vertBuffer));
    }
}

void RenderDevice::CopyFrameBuffer()
{
    for (int32 s = 0; s < videoSettings.screenCount; ++s) {
        uint16 *src = screens[s].frameBuffer;
        if (!src)
            continue;

        int32 width  = screens[s].size.x;
        int32 height = screens[s].size.y;
        if (width <= 0 || height <= 0 || width > 512 || height > 512)
            continue;

        uint32 *dstTex = screenTextureLocalMem[s] ? (uint32 *)screenTextureLocalMem[s] : (uint32 *)frameBufferLocalMem[currentBuffer];
        if (!dstTex)
            continue;

        int32 pitchStride = screenTextureLocalMem[s] ? (screenTexturePitch / 4) : (frameBufferPitch / 4);
        for (int32 y = 0; y < height; ++y) {
            for (int32 x = 0; x < width; ++x) {
                uint16 pixel = src[y * screens[s].pitch + x];
                uint32 r = (pixel & 0xF800) >> 11;
                uint32 g = (pixel & 0x07E0) >> 5;
                uint32 b = (pixel & 0x001F);

                r = (r << 3) | (r >> 2);
                g = (g << 2) | (g >> 4);
                b = (b << 3) | (b >> 2);

                dstTex[y * pitchStride + x] = (0xFF << 24) | (r << 16) | (g << 8) | b;
            }
        }
    }
}

void RenderDevice::FlipScreen()
{
    if (windowRefreshDelay > 0) {
        if (!--windowRefreshDelay)
            UpdateGameWindow();
        return;
    }

#if RETRO_PLATFORM == RETRO_PS3
    if (gcmContext) {
        gcmContextData *ctx = (gcmContextData *)gcmContext;

        uint32 width = videoSettings.fsWidth ? (uint32)videoSettings.fsWidth : 1280;
        uint32 height = videoSettings.fsHeight ? (uint32)videoSettings.fsHeight : 720;

        // 1. Setup Render Target Surface (Color buffer + Z/Depth buffer)
        SetRenderTarget(currentBuffer);

        // 2. Set Draw Environment (Color masks, Viewport, Scissor)
        rsxSetColorMask(ctx, GCM_COLOR_MASK_B | GCM_COLOR_MASK_G | GCM_COLOR_MASK_R | GCM_COLOR_MASK_A);
        rsxSetColorMaskMrt(ctx, 0);

        for (int i = 0; i < 8; ++i)
            rsxSetViewportClip(ctx, i, width, height);

        const f32 vpScale[4]  = { (f32)width * 0.5f, (f32)height * -0.5f, 0.5f, 0.0f };
        const f32 vpOffset[4] = { (f32)width * 0.5f, (f32)height * 0.5f, 0.5f, 0.0f };
        rsxSetViewport(ctx, 0, 0, width, height, 0.0f, 1.0f, vpScale, vpOffset);
        rsxSetScissor(ctx, 0, 0, width, height);

        // 3. Clear Screen (Color + Depth)
        rsxSetClearColor(ctx, 0x00000000);
        rsxSetClearDepthStencil(ctx, 0xFFFFFF00);
        rsxClearSurface(ctx, GCM_CLEAR_R | GCM_CLEAR_G | GCM_CLEAR_B | GCM_CLEAR_A | GCM_CLEAR_S | GCM_CLEAR_Z);

        // 4. Set Pipeline States (Cull, Depth)
        rsxSetCullFaceEnable(ctx, GCM_FALSE);
        rsxSetDepthTestEnable(ctx, GCM_FALSE);

        // 5. Bind Shaders
        if (vertexShaderBytes && vertexShaderUcode) {
            rsxLoadVertexProgram(ctx, (const rsxVertexProgram *)vertexShaderBytes, vertexShaderUcode);
        }
        if (fragmentShaderHostDesc && fragmentShaderOffset) {
            rsxLoadFragmentProgramLocation(ctx, (const rsxFragmentProgram *)fragmentShaderHostDesc, fragmentShaderOffset, GCM_LOCATION_LOCAL);
        }

        // 6. Bind Vertex Buffer Stream Attributes
        if (vertexBufferLocalMem && vertexBufferOffset) {
            // Attribute 0 (Position): Float3
            rsxBindVertexArrayAttrib(ctx, 0, 0, vertexBufferOffset + (uint32)offsetof(RenderVertex, pos), sizeof(RenderVertex), 3, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_LOCAL);
            // Attribute 1 (Color): UByte4
            rsxBindVertexArrayAttrib(ctx, 1, 0, vertexBufferOffset + (uint32)offsetof(RenderVertex, color), sizeof(RenderVertex), 4, GCM_VERTEX_DATA_TYPE_U8, GCM_LOCATION_LOCAL);
            // Attribute 2 (TexCoord): Float2
            rsxBindVertexArrayAttrib(ctx, 2, 0, vertexBufferOffset + (uint32)offsetof(RenderVertex, tex), sizeof(RenderVertex), 2, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_LOCAL);
        }

        // Texture setup helper lambda
        auto bindScreenTexture = [&](int screenIdx) {
            rsxInvalidateTextureCache(ctx, GCM_INVALIDATE_TEXTURE);

            gcmTexture tex;
            memset(&tex, 0, sizeof(tex));
            tex.format = GCM_TEXTURE_FORMAT_A8R8G8B8 | GCM_TEXTURE_FORMAT_LIN;
            tex.mipmap = 1;
            tex.dimension = GCM_TEXTURE_DIMS_2D;
            tex.cubemap = GCM_FALSE;
            tex.remap = ((GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_B_SHIFT) |
                         (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_G_SHIFT) |
                         (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_R_SHIFT) |
                         (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_A_SHIFT) |
                         (GCM_TEXTURE_REMAP_COLOR_B << GCM_TEXTURE_REMAP_COLOR_B_SHIFT) |
                         (GCM_TEXTURE_REMAP_COLOR_G << GCM_TEXTURE_REMAP_COLOR_G_SHIFT) |
                         (GCM_TEXTURE_REMAP_COLOR_R << GCM_TEXTURE_REMAP_COLOR_R_SHIFT) |
                         (GCM_TEXTURE_REMAP_COLOR_A << GCM_TEXTURE_REMAP_COLOR_A_SHIFT));
            tex.width = 512;
            tex.height = 512;
            tex.depth = 1;
            tex.pitch = screenTexturePitch;
            tex.location = GCM_LOCATION_LOCAL;
            tex.offset = screenTextureOffset[screenIdx];
            rsxLoadTexture(ctx, 0, &tex);
            rsxTextureControl(ctx, 0, GCM_TRUE, 0 << 8, 12 << 8, GCM_TEXTURE_MAX_ANISO_1);
            rsxTextureFilter(ctx, 0, 0, GCM_TEXTURE_LINEAR, GCM_TEXTURE_LINEAR, GCM_TEXTURE_CONVOLUTION_QUINCUNX);
            rsxTextureWrapMode(ctx, 0, GCM_TEXTURE_CLAMP_TO_EDGE, GCM_TEXTURE_CLAMP_TO_EDGE, GCM_TEXTURE_CLAMP_TO_EDGE, 0, GCM_TEXTURE_ZFUNC_LESS, 0);
        };

        // 7. Draw Screen Primitives
        int32 startVert = 0;
        switch (videoSettings.screenCount) {
            default:
            case 0:
#if RETRO_REV02
                startVert = 54;
#else
                startVert = 18;
#endif
                bindScreenTexture(0);
                rsxDrawVertexArray(ctx, GCM_TYPE_TRIANGLES, startVert, 6);
                break;

            case 1:
                bindScreenTexture(0);
                rsxDrawVertexArray(ctx, GCM_TYPE_TRIANGLES, 0, 6);
                break;

            case 2:
#if RETRO_REV02
                startVert = startVertex_2P[0];
#else
                startVert = 6;
#endif
                bindScreenTexture(0);
                rsxDrawVertexArray(ctx, GCM_TYPE_TRIANGLES, startVert, 6);

#if RETRO_REV02
                startVert = startVertex_2P[1];
#else
                startVert = 12;
#endif
                bindScreenTexture(1);
                rsxDrawVertexArray(ctx, GCM_TYPE_TRIANGLES, startVert, 6);
                break;

#if RETRO_REV02
            case 3:
                bindScreenTexture(0);
                rsxDrawVertexArray(ctx, GCM_TYPE_TRIANGLES, startVertex_3P[0], 6);
                bindScreenTexture(1);
                rsxDrawVertexArray(ctx, GCM_TYPE_TRIANGLES, startVertex_3P[1], 6);
                bindScreenTexture(2);
                rsxDrawVertexArray(ctx, GCM_TYPE_TRIANGLES, startVertex_3P[2], 6);
                break;

            case 4:
                bindScreenTexture(0);
                rsxDrawVertexArray(ctx, GCM_TYPE_TRIANGLES, 30, 6);
                bindScreenTexture(1);
                rsxDrawVertexArray(ctx, GCM_TYPE_TRIANGLES, 36, 6);
                bindScreenTexture(2);
                rsxDrawVertexArray(ctx, GCM_TYPE_TRIANGLES, 42, 6);
                bindScreenTexture(3);
                rsxDrawVertexArray(ctx, GCM_TYPE_TRIANGLES, 48, 6);
                break;
#endif
        }

        // 8. REQ 5: CPU-GPU Synchronization
        if (lastShaderID != videoSettings.shaderID) {
            lastShaderID = videoSettings.shaderID;
        }

        if (firstFrame) {
            gcmResetFlipStatus();
        }

        // Issue RSX flip command for current buffer and flush to RSX
        gcmSetFlip(ctx, currentBuffer);
        rsxFlushBuffer(ctx);

        // Instruct RSX to wait for flip before processing subsequent frame commands
        gcmSetWaitFlip(ctx);

        // Wait for flip to complete on display controller
        int flipTimeout = 200;
        while (gcmGetFlipStatus() != 0 && --flipTimeout > 0) {
            usleep(200);
        }
        gcmResetFlipStatus();

        firstFrame = false;

        // Swap backbuffer index
        currentBuffer ^= 1;
    }
#endif
}

void RenderDevice::Release(bool32 isRefresh)
{
#if RETRO_PLATFORM == RETRO_PS3
    FreeLocalMemory(rsxFreeLocalMem);
    rsxFreeLocalMem = nullptr;

    FreeLocalMemory(fragmentShaderLocalMem);
    fragmentShaderLocalMem = nullptr;

    FreeLocalMemory(depthBufferLocalMem);
    depthBufferLocalMem = nullptr;

    FreeLocalMemory(vertexBufferLocalMem);
    vertexBufferLocalMem = nullptr;

    for (int s = 0; s < SCREEN_COUNT; ++s) {
        FreeLocalMemory(screenTextureLocalMem[s]);
        screenTextureLocalMem[s] = nullptr;
    }

    for (int i = 0; i < 2; ++i) {
        FreeLocalMemory(frameBufferLocalMem[i]);
        frameBufferLocalMem[i] = nullptr;
    }

    if (hostCmdBuffer) {
        free(hostCmdBuffer);
        hostCmdBuffer = nullptr;
    }
#endif
}

void RenderDevice::RefreshWindow() {}

void RenderDevice::GetWindowSize(int32 *width, int32 *height)
{
    if (width)
        *width = videoSettings.fsWidth ? videoSettings.fsWidth : 1280;
    if (height)
        *height = videoSettings.fsHeight ? videoSettings.fsHeight : 720;
}

void RenderDevice::SetupImageTexture(int32 width, int32 height, uint8 *imagePixels)
{
    if (!frameBufferLocalMem[currentBuffer] || !imagePixels)
        return;

    uint32 *dst = (uint32 *)frameBufferLocalMem[currentBuffer];
    uint32 *src = (uint32 *)imagePixels;
    int32 copyW = width < (int32)videoSettings.fsWidth ? width : videoSettings.fsWidth;
    int32 copyH = height < (int32)videoSettings.fsHeight ? height : videoSettings.fsHeight;
    int32 pitchStride = frameBufferPitch / 4;

    for (int32 y = 0; y < copyH; ++y) {
        for (int32 x = 0; x < copyW; ++x) {
            dst[y * pitchStride + x] = src[y * width + x];
        }
    }
}

void RenderDevice::SetupVideoTexture_YUV420(int32 width, int32 height, uint8 *yPlane, uint8 *uPlane, uint8 *vPlane, int32 strideY, int32 strideU,
                                           int32 strideV)
{
    if (!frameBufferLocalMem[currentBuffer] || !yPlane || !uPlane || !vPlane)
        return;

    uint32 *dst = (uint32 *)frameBufferLocalMem[currentBuffer];
    int32 copyW = width < (int32)videoSettings.fsWidth ? width : videoSettings.fsWidth;
    int32 copyH = height < (int32)videoSettings.fsHeight ? height : videoSettings.fsHeight;
    int32 pitchStride = frameBufferPitch / 4;

    for (int32 y = 0; y < copyH; ++y) {
        for (int32 x = 0; x < copyW; ++x) {
            int32 Y = yPlane[y * strideY + x];
            int32 U = uPlane[(y / 2) * strideU + (x / 2)] - 128;
            int32 V = vPlane[(y / 2) * strideV + (x / 2)] - 128;

            int32 r = Y + ((1436 * V) >> 10);
            int32 g = Y - ((352 * U + 731 * V) >> 10);
            int32 b = Y + ((1814 * U) >> 10);

            r = r < 0 ? 0 : (r > 255 ? 255 : r);
            g = g < 0 ? 0 : (g > 255 ? 255 : g);
            b = b < 0 ? 0 : (b > 255 ? 255 : b);

            dst[y * pitchStride + x] = (0xFF << 24) | (r << 16) | (g << 8) | b;
        }
    }
}

void RenderDevice::SetupVideoTexture_YUV422(int32 width, int32 height, uint8 *yPlane, uint8 *uPlane, uint8 *vPlane, int32 strideY, int32 strideU,
                                           int32 strideV)
{
    SetupVideoTexture_YUV420(width, height, yPlane, uPlane, vPlane, strideY, strideU, strideV);
}

void RenderDevice::SetupVideoTexture_YUV444(int32 width, int32 height, uint8 *yPlane, uint8 *uPlane, uint8 *vPlane, int32 strideY, int32 strideU,
                                           int32 strideV)
{
    if (!frameBufferLocalMem[currentBuffer] || !yPlane || !uPlane || !vPlane)
        return;

    uint32 *dst = (uint32 *)frameBufferLocalMem[currentBuffer];
    int32 copyW = width < (int32)videoSettings.fsWidth ? width : videoSettings.fsWidth;
    int32 copyH = height < (int32)videoSettings.fsHeight ? height : videoSettings.fsHeight;
    int32 pitchStride = frameBufferPitch / 4;

    for (int32 y = 0; y < copyH; ++y) {
        for (int32 x = 0; x < copyW; ++x) {
            int32 Y = yPlane[y * strideY + x];
            int32 U = uPlane[y * strideU + x] - 128;
            int32 V = vPlane[y * strideV + x] - 128;

            int32 r = Y + ((1436 * V) >> 10);
            int32 g = Y - ((352 * U + 731 * V) >> 10);
            int32 b = Y + ((1814 * U) >> 10);

            r = r < 0 ? 0 : (r > 255 ? 255 : r);
            g = g < 0 ? 0 : (g > 255 ? 255 : g);
            b = b < 0 ? 0 : (b > 255 ? 255 : b);

            dst[y * pitchStride + x] = (0xFF << 24) | (r << 16) | (g << 8) | b;
        }
    }
}

bool RenderDevice::ProcessEvents()
{
#if RETRO_PLATFORM == RETRO_PS3
    sysUtilCheckCallback();
#endif

#if RETRO_INPUTDEVICE_SDL2
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            isRunning = false;
            return false;
        }
    }
#endif
    return isRunning;
}

void RenderDevice::InitFPSCap() {}

bool RenderDevice::CheckFPSCap()
{
    return true;
}

void RenderDevice::UpdateFPSCap() {}

bool RenderDevice::InitShaders()
{
    videoSettings.shaderSupport = true;
    shaderCount = 0;

    // Shader 0: Clean
    StrCopy(shaderList[SHADER_CLEAN].name, "Clean");
    shaderList[SHADER_CLEAN].linear = false;
    shaderList[SHADER_CLEAN].vertexProgram = (void *)vertexShaderBytes;
    shaderList[SHADER_CLEAN].fragmentProgram = fragmentShaderLocalMem;
    shaderList[SHADER_CLEAN].fragmentOffset = fragmentShaderOffset;
    shaderCount++;

    // Shader 1: CRT Yeetron
    StrCopy(shaderList[SHADER_CRT_YEETRON].name, "CRT-Yeetron");
    shaderList[SHADER_CRT_YEETRON].linear = true;
    shaderList[SHADER_CRT_YEETRON].vertexProgram = (void *)vertexShaderBytes;
    shaderList[SHADER_CRT_YEETRON].fragmentProgram = fragmentShaderLocalMem;
    shaderList[SHADER_CRT_YEETRON].fragmentOffset = fragmentShaderOffset;
    shaderCount++;

    // Shader 2: CRT Yee64
    StrCopy(shaderList[SHADER_CRT_YEE64].name, "CRT-Yee64");
    shaderList[SHADER_CRT_YEE64].linear = true;
    shaderList[SHADER_CRT_YEE64].vertexProgram = (void *)vertexShaderBytes;
    shaderList[SHADER_CRT_YEE64].fragmentProgram = fragmentShaderLocalMem;
    shaderList[SHADER_CRT_YEE64].fragmentOffset = fragmentShaderOffset;
    shaderCount++;

    // Shader 3: YUV 420
    StrCopy(shaderList[SHADER_YUV_420].name, "YUV-420");
    shaderList[SHADER_YUV_420].linear = true;
    shaderList[SHADER_YUV_420].vertexProgram = (void *)vertexShaderBytes;
    shaderList[SHADER_YUV_420].fragmentProgram = fragmentShaderLocalMem;
    shaderList[SHADER_YUV_420].fragmentOffset = fragmentShaderOffset;
    shaderCount++;

    return true;
}

void RenderDevice::LoadShader(const char *fileName, bool32 linear)
{
    if (shaderCount >= SHADER_COUNT)
        return;

    ShaderEntry *shader = &shaderList[shaderCount];
    StrCopy(shader->name, fileName);
    shader->linear = linear;
    shader->vertexProgram = (void *)vertexShaderBytes;
    shader->fragmentProgram = fragmentShaderLocalMem;
    shader->fragmentOffset = fragmentShaderOffset;
    shaderCount++;
}

bool RenderDevice::SetupRendering()
{
#if RETRO_PLATFORM == RETRO_PS3
    if (gcmContext) {
        uint16 width = videoSettings.fsWidth ? (uint16)videoSettings.fsWidth : 1280;
        uint16 height = videoSettings.fsHeight ? (uint16)videoSettings.fsHeight : 720;
        rsxSetScissor((gcmContextData *)gcmContext, 0, 0, width, height);
    }
#endif
    return true;
}

void RenderDevice::GetDisplays() {}

#endif // RETRO_RENDERDEVICE_GCM
