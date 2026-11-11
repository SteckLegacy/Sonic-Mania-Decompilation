
RenderDevice::Tiny3DTexture RenderDevice::screenTexture[SCREEN_COUNT];
RenderDevice::Tiny3DTexture RenderDevice::imageTexture;
RenderDevice::Tiny3DTexture RenderDevice::videoTextureY;
RenderDevice::Tiny3DTexture RenderDevice::videoTextureU;
RenderDevice::Tiny3DTexture RenderDevice::videoTextureV;

uint32 RenderDevice::videoWidth  = 0;
uint32 RenderDevice::videoHeight = 0;

uint32 RenderDevice::displayModeIndex = 0;
int32 RenderDevice::displayModeCount  = 0;

unsigned long long RenderDevice::targetFreq = 0;
unsigned long long RenderDevice::curTicks   = 0;
unsigned long long RenderDevice::prevTicks  = 0;

RenderVertex RenderDevice::vertexBuffer[!RETRO_REV02 ? 24 : 60];
uint8 RenderDevice::lastTextureFormat = -1;

bool RenderDevice::tiny3dInitialized = false;

static uint8 *s_texturePool     = nullptr;
static uint32 s_texturePoolSize = 0;
static uint32 s_texturePoolUsed = 0;

static void *AllocTextureMemory(uint32 size, uint32 alignment = 128)
{
    if (!s_texturePool)
        return nullptr;

    uint32 alignedOffset = (s_texturePoolUsed + (alignment - 1)) & ~(alignment - 1);
    if (alignedOffset + size > s_texturePoolSize) {
        PrintLog(PRINT_NORMAL, "Tiny3D: Out of RSX texture pool memory! (Req: %u, Used: %u, Total: %u)", size, alignedOffset, s_texturePoolSize);
        return nullptr;
    }

    void *ptr = s_texturePool + alignedOffset;
    s_texturePoolUsed = alignedOffset + size;
    return ptr;
}

#define NORMALIZE(val, minVal, maxVal) ((float)(val) - (float)(minVal)) / ((float)(maxVal) - (float)(minVal))

bool RenderDevice::Init()
{
    // SDL2 handles non-graphical subsystems: events, audio, input
    SDL_InitSubSystem(SDL_INIT_EVENTS);

    videoSettings.windowed     = false;
    videoSettings.windowWidth  = 1920;
    videoSettings.windowHeight = 1080;

    PrintLog(PRINT_NORMAL, "Tiny3D RenderDevice initializing (1080p)...");

    if (!SetupRendering() || !AudioDevice::Init())
        return false;

    InitInputDevices();
    return true;
}

void RenderDevice::CopyFrameBuffer()
{
    for (int32 s = 0; s < videoSettings.screenCount; ++s) {
        if (!screenTexture[s].pixels)
            continue;

        uint16 *srcFrameBuffer = screens[s].frameBuffer;
        uint16 *dstPixels      = (uint16 *)screenTexture[s].pixels;
        int32 widthBytes       = screens[s].size.x * sizeof(uint16);
        int32 dstPitch16       = screenTexture[s].pitch / sizeof(uint16);

        for (int32 y = 0; y < SCREEN_YSIZE; ++y) {
            memcpy(dstPixels, srcFrameBuffer, widthBytes);
            srcFrameBuffer += screens[s].pitch;
            dstPixels      += dstPitch16;
        }
    }
}

void RenderDevice::DrawQuad(float x, float y, float w, float h, float u1, float v1, float u2, float v2, uint32 color)
{
    tiny3d_SetPolygon(TINY3D_TRIANGLES);

    // Triangle 1: Top-Left, Bottom-Left, Bottom-Right
    tiny3d_VertexPos(x, y, 65535.0f);
    tiny3d_VertexColor(color);
    tiny3d_VertexTexture(u1, v1);

    tiny3d_VertexPos(x, y + h, 65535.0f);
    tiny3d_VertexColor(color);
    tiny3d_VertexTexture(u1, v2);

    tiny3d_VertexPos(x + w, y + h, 65535.0f);
    tiny3d_VertexColor(color);
    tiny3d_VertexTexture(u2, v2);

    // Triangle 2: Top-Left, Bottom-Right, Top-Right
    tiny3d_VertexPos(x, y, 65535.0f);
    tiny3d_VertexColor(color);
    tiny3d_VertexTexture(u1, v1);

    tiny3d_VertexPos(x + w, y + h, 65535.0f);
    tiny3d_VertexColor(color);
    tiny3d_VertexTexture(u2, v2);

    tiny3d_VertexPos(x + w, y, 65535.0f);
    tiny3d_VertexColor(color);
    tiny3d_VertexTexture(u2, v1);

    tiny3d_End();
}

void RenderDevice::FlipScreen()
{
    if (windowRefreshDelay > 0) {
        windowRefreshDelay--;
        if (!windowRefreshDelay)
            UpdateGameWindow();
        return;
    }

    float dimAmount = videoSettings.dimMax * videoSettings.dimPercent;

    // Clear screen to black
    tiny3d_Clear(0xFF000000, TINY3D_CLEAR_ALL);

    // Switch to Tiny3D 2D projection (virtual coordinates 848 x 512)
    tiny3d_Project2D();

    tiny3d_AlphaTest(1, 0x00, TINY3D_ALPHA_FUNC_GEQUAL);
    tiny3d_BlendFunc(1,
                     (blend_src_func)(TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA),
                     (blend_dst_func)(TINY3D_BLEND_FUNC_DST_RGB_ONE_MINUS_SRC_ALPHA | TINY3D_BLEND_FUNC_DST_ALPHA_ZERO),
                     (blend_func)(TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD));

    const float virtualWidth  = 848.0f;
    const float virtualHeight = 512.0f;

    // Calculate scale from native game coordinates (pixWidth x SCREEN_YSIZE) to virtual (848 x 512)
    float scaleX = virtualWidth / (float)videoSettings.pixWidth;
    float scaleY = virtualHeight / (float)SCREEN_YSIZE;

    // Dim factor color multiplier
    uint8 dimAlpha = (uint8)(dimAmount * 255.0f);
    uint32 quadColor = (0xFF << 24) | (dimAlpha << 16) | (dimAlpha << 8) | dimAlpha;

    int32 startVert = 0;

    auto DrawScreenQuad = [&](int32 vertIdx, Tiny3DTexture &tex) {
        float x1 = vertexBuffer[vertIdx].pos.x * scaleX;
        float y1 = vertexBuffer[vertIdx].pos.y * scaleY;
        float x2 = vertexBuffer[vertIdx + 2].pos.x * scaleX;
        float y2 = vertexBuffer[vertIdx + 2].pos.y * scaleY;
        float w  = x2 - x1;
        float h  = y2 - y1;

        float u1 = vertexBuffer[vertIdx].tex.x;
        float v1 = vertexBuffer[vertIdx].tex.y;
        float u2 = vertexBuffer[vertIdx + 2].tex.x;
        float v2 = vertexBuffer[vertIdx + 2].tex.y;

        tiny3d_SetTextureWrap(0, tex.offset, tex.width, tex.height, tex.pitch,
                              tex.format, TEXTWRAP_CLAMP, TEXTWRAP_CLAMP, TEXTURE_NEAREST);

        DrawQuad(x1, y1, w, h, u1, v1, u2, v2, quadColor);
    };

    switch (videoSettings.screenCount) {
        default:
        case 0:
            if (imageTexture.pixels) {
                float maxU = (videoWidth > 0 && imageTexture.width > 0) ? ((float)videoWidth / (float)imageTexture.width) : 1.0f;
                float maxV = (videoHeight > 0 && imageTexture.height > 0) ? ((float)videoHeight / (float)imageTexture.height) : 1.0f;

                tiny3d_SetTextureWrap(0, imageTexture.offset, imageTexture.width, imageTexture.height, imageTexture.pitch,
                                      imageTexture.format, TEXTWRAP_CLAMP, TEXTWRAP_CLAMP, TEXTURE_LINEAR);

                DrawQuad(0.0f, 0.0f, virtualWidth, virtualHeight, 0.0f, 0.0f, maxU, maxV, quadColor);
            }
            break;

            case 1:
                startVert = 0;
                DrawScreenQuad(startVert, screenTexture[0]);
                break;

            case 2:
#if RETRO_REV02
                startVert = startVertex_2P[0];
#else
                startVert = 6;
#endif
                DrawScreenQuad(startVert, screenTexture[0]);

#if RETRO_REV02
                startVert = startVertex_2P[1];
#else
                startVert = 12;
#endif
                DrawScreenQuad(startVert, screenTexture[1]);
                break;

#if RETRO_REV02
            case 3:
                startVert = startVertex_3P[0];
                DrawScreenQuad(startVert, screenTexture[0]);

                startVert = startVertex_3P[1];
                DrawScreenQuad(startVert, screenTexture[1]);

                startVert = startVertex_3P[2];
                DrawScreenQuad(startVert, screenTexture[2]);
                break;

            case 4:
                startVert = 30;
                DrawScreenQuad(startVert, screenTexture[0]);

                startVert = 36;
                DrawScreenQuad(startVert, screenTexture[1]);

                startVert = 42;
                DrawScreenQuad(startVert, screenTexture[2]);

                startVert = 48;
                DrawScreenQuad(startVert, screenTexture[3]);
                break;
#endif
        }
    }

    // Present frame on RSX via Tiny3D with hardware VSync (tiny3d_Flip synchronizes flip and waits for vblank)
    tiny3d_Flip();
}

void RenderDevice::Release(bool32 isRefresh)
{
    for (int32 s = 0; s < SCREEN_COUNT; ++s) {
        screenTexture[s].pixels = nullptr;
        screenTexture[s].offset = 0;
    }

    imageTexture.pixels = nullptr;
    imageTexture.offset = 0;
    videoTextureY.pixels = nullptr;
    videoTextureY.offset = 0;
    videoTextureU.pixels = nullptr;
    videoTextureU.offset = 0;
    videoTextureV.pixels = nullptr;
    videoTextureV.offset = 0;

    s_texturePoolUsed = 0;

    if (!isRefresh) {
        if (displayInfo.displays)
            free(displayInfo.displays);
        displayInfo.displays = nullptr;

        if (tiny3dInitialized) {
            tiny3d_Exit();
            tiny3dInitialized = false;
        }

        SDL_QuitSubSystem(SDL_INIT_EVENTS);

        if (scanlines)
            free(scanlines);
        scanlines = nullptr;
    }
}

void RenderDevice::RefreshWindow()
{
    videoSettings.windowState = WINDOWSTATE_UNINITIALIZED;

    Release(true);

    GetDisplays();

    if (!InitGraphicsAPI() || !InitShaders())
        return;

    videoSettings.windowState = WINDOWSTATE_ACTIVE;
}

void RenderDevice::InitFPSCap() {}

bool RenderDevice::CheckFPSCap()
{
    // Hardware VSync is handled directly by RSX flip synchronization in tiny3d_Flip()
    return true;
}

void RenderDevice::UpdateFPSCap() {}

void RenderDevice::InitVertexBuffer()
{
    RenderVertex vertBuffer[sizeof(rsdkVertexBuffer) / sizeof(RenderVertex)];
    memcpy(vertBuffer, rsdkVertexBuffer, sizeof(rsdkVertexBuffer));

    int32 vertCount = (RETRO_REV02 ? 60 : 24) - 6;

    for (int32 v = 0; v < vertCount; ++v) {
        RenderVertex *vertex = &vertBuffer[v];
        vertex->pos.x        = NORMALIZE(vertex->pos.x, -1.0, 1.0) * videoSettings.pixWidth;
        vertex->pos.y        = (1.0 - NORMALIZE(vertex->pos.y, -1.0, 1.0)) * SCREEN_YSIZE;

        if (vertex->tex.x)
            vertex->tex.x = screens[0].size.x * (1.0 / textureSize.x);

        if (vertex->tex.y)
            vertex->tex.y = screens[0].size.y * (1.0 / textureSize.y);
    }

    for (int32 v = 0; v < 6; ++v) {
        RenderVertex *vertex = &vertBuffer[vertCount + v];
        vertex->pos.x        = NORMALIZE(vertex->pos.x, -1.0, 1.0) * videoSettings.pixWidth;
        vertex->pos.y        = (1.0 - NORMALIZE(vertex->pos.y, -1.0, 1.0)) * SCREEN_YSIZE;

        if (vertex->tex.x)
            vertex->tex.x = 1.0f;

        if (vertex->tex.y)
            vertex->tex.y = 1.0f;
    }

    memcpy(vertexBuffer, vertBuffer, sizeof(vertBuffer));
}

bool RenderDevice::InitGraphicsAPI()
{
    videoSettings.shaderSupport = false;

    viewSize.x = 1920;
    viewSize.y = 1080;

    int32 maxPixHeight = 0;
    int32 screenWidth  = 0;

    for (int32 s = 0; s < 4; ++s) {
        if (videoSettings.pixHeight > maxPixHeight)
            maxPixHeight = videoSettings.pixHeight;

        screens[s].size.y = videoSettings.pixHeight;

        float viewAspect = (float)viewSize.x / (float)viewSize.y;
        screenWidth      = (int32)((viewAspect * videoSettings.pixHeight) + 3) & 0xFFFFFFFC;
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

    pixelSize.x = screens[0].size.x;
    pixelSize.y = screens[0].size.y;

    if (pixelSize.x <= 512 && maxPixHeight <= 256) {
        textureSize.x = 512.0f;
        textureSize.y = 256.0f;
    }
    else {
        textureSize.x = 1024.0f;
        textureSize.y = 512.0f;
    }

    s_texturePoolUsed = 0;

    // Allocate RSX texture memory for screen textures (RGB565)
    for (int32 s = 0; s < SCREEN_COUNT; ++s) {
        screenTexture[s].width  = (uint32)textureSize.x;
        screenTexture[s].height = (uint32)textureSize.y;
        screenTexture[s].pitch  = screenTexture[s].width * sizeof(uint16);
        screenTexture[s].format = TINY3D_TEX_FORMAT_R5G6B5;

        uint32 texSize = screenTexture[s].pitch * screenTexture[s].height;
        screenTexture[s].pixels = AllocTextureMemory(texSize);
        if (!screenTexture[s].pixels) {
            PrintLog(PRINT_NORMAL, "Tiny3D: Failed to allocate screen texture %d", s);
            return false;
        }
        screenTexture[s].offset = tiny3d_TextureOffset(screenTexture[s].pixels);
        memset(screenTexture[s].pixels, 0, texSize);
    }

    // Allocate RSX texture memory for imageTexture (ARGB8888)
    imageTexture.width  = RETRO_VIDEO_TEXTURE_W;
    imageTexture.height = RETRO_VIDEO_TEXTURE_H;
    imageTexture.pitch  = imageTexture.width * sizeof(uint32);
    imageTexture.format = TINY3D_TEX_FORMAT_A8R8G8B8;
    uint32 imgSize      = imageTexture.pitch * imageTexture.height;
    imageTexture.pixels = AllocTextureMemory(imgSize);
    if (imageTexture.pixels) {
        imageTexture.offset = tiny3d_TextureOffset(imageTexture.pixels);
        memset(imageTexture.pixels, 0, imgSize);
    }

    // Allocate RSX texture memory for YUV planar video textures (L8)
    videoTextureY.width  = RETRO_VIDEO_TEXTURE_W;
    videoTextureY.height = RETRO_VIDEO_TEXTURE_H;
    videoTextureY.pitch  = videoTextureY.width;
    videoTextureY.format = TINY3D_TEX_FORMAT_L8;
    videoTextureY.pixels = AllocTextureMemory(videoTextureY.pitch * videoTextureY.height);
    if (videoTextureY.pixels)
        videoTextureY.offset = tiny3d_TextureOffset(videoTextureY.pixels);

    videoTextureU.width  = RETRO_VIDEO_TEXTURE_W / 2;
    videoTextureU.height = RETRO_VIDEO_TEXTURE_H / 2;
    videoTextureU.pitch  = videoTextureU.width;
    videoTextureU.format = TINY3D_TEX_FORMAT_L8;
    videoTextureU.pixels = AllocTextureMemory(videoTextureU.pitch * videoTextureU.height);
    if (videoTextureU.pixels)
        videoTextureU.offset = tiny3d_TextureOffset(videoTextureU.pixels);

    videoTextureV.width  = RETRO_VIDEO_TEXTURE_W / 2;
    videoTextureV.height = RETRO_VIDEO_TEXTURE_H / 2;
    videoTextureV.pitch  = videoTextureV.width;
    videoTextureV.format = TINY3D_TEX_FORMAT_L8;
    videoTextureV.pixels = AllocTextureMemory(videoTextureV.pitch * videoTextureV.height);
    if (videoTextureV.pixels)
        videoTextureV.offset = tiny3d_TextureOffset(videoTextureV.pixels);

    lastShaderID = -1;
    InitVertexBuffer();
    engine.inFocus          = 1;
    videoSettings.viewportX = 0;
    videoSettings.viewportY = 0;
    videoSettings.viewportW = 1.0f / (float)viewSize.x;
    videoSettings.viewportH = 1.0f / (float)viewSize.y;

    return true;
}

void RenderDevice::LoadShader(const char *fileName, bool32 linear)
{
    PrintLog(PRINT_NORMAL, "Tiny3D: Native fixed-function shaders active");
}

bool RenderDevice::InitShaders()
{
    for (int32 s = 0; s < SHADER_COUNT; ++s)
        shaderList[s].linear = true;

    shaderCount            = 1;
    videoSettings.shaderID = 0;
    return true;
}

bool RenderDevice::SetupRendering()
{
    if (!tiny3dInitialized) {
        int res = tiny3d_Init(1024 * 1024);
        if (res != TINY3D_OK) {
            PrintLog(PRINT_NORMAL, "ERROR: tiny3d_Init failed with code %d", res);
            return false;
        }
        tiny3dInitialized = true;

        // Allocate a persistent 32MB texture pool in RSX memory via Tiny3D
        s_texturePoolSize = 32 * 1024 * 1024;
        s_texturePool     = (uint8 *)tiny3d_AllocTexture(s_texturePoolSize);
        if (!s_texturePool) {
            PrintLog(PRINT_NORMAL, "ERROR: tiny3d_AllocTexture failed to allocate %u bytes", s_texturePoolSize);
            return false;
        }
        s_texturePoolUsed = 0;
    }

    GetDisplays();

    if (!InitGraphicsAPI() || !InitShaders())
        return false;

    int32 size = videoSettings.pixWidth >= SCREEN_YSIZE ? videoSettings.pixWidth : SCREEN_YSIZE;
    scanlines  = (ScanlineInfo *)malloc(size * sizeof(ScanlineInfo));
    if (scanlines)
        memset(scanlines, 0, size * sizeof(ScanlineInfo));

    videoSettings.windowState = WINDOWSTATE_ACTIVE;
    videoSettings.dimMax      = 1.0f;
    videoSettings.dimPercent  = 1.0f;

    return true;
}

void RenderDevice::GetDisplays()
{
    displayWidth[0]  = 1920;
    displayHeight[0] = 1080;
    displayModeIndex = 0;
    displayCount     = 1;

    if (displayInfo.displays)
        free(displayInfo.displays);

    displayInfo.displays = (decltype(displayInfo.displays))malloc(sizeof(SDL_DisplayMode));
    if (displayInfo.displays) {
        displayInfo.displays[0].width        = 1920;
        displayInfo.displays[0].height       = 1080;
        displayInfo.displays[0].refresh_rate = 60;
    }

    videoSettings.fsWidth     = 1920;
    videoSettings.fsHeight    = 1080;
    videoSettings.refreshRate = 60;
}

void RenderDevice::GetWindowSize(int32 *width, int32 *height)
{
    if (width)
        *width = 1920;
    if (height)
        *height = 1080;
}

void RenderDevice::ProcessEvent(SDL_Event event)
{
    switch (event.type) {
        case SDL_CONTROLLERDEVICEADDED: {
            SDL_GameController *game_controller = SDL_GameControllerOpen(event.cdevice.which);

            if (game_controller != NULL) {
                uint32 id;
                char idBuffer[0x20];
                sprintf_s(idBuffer, sizeof(idBuffer), "SDLDevice%d", SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(game_controller)));
                GenerateHashCRC(&id, idBuffer);

                if (SKU::InitSDL2InputDevice(id, game_controller) == NULL)
                    SDL_GameControllerClose(game_controller);
            }

            break;
        }

        case SDL_CONTROLLERDEVICEREMOVED: {
            uint32 id;
            char idBuffer[0x20];
            sprintf_s(idBuffer, sizeof(idBuffer), "SDLDevice%d", event.cdevice.which);
            GenerateHashCRC(&id, idBuffer);

            RemoveInputDevice(InputDeviceFromID(id));
            break;
        }

        case SDL_APP_WILLENTERFOREGROUND:
#if RETRO_REV02
            SKU::userCore->focusState = 0;
#endif
            break;

        case SDL_APP_WILLENTERBACKGROUND:
#if RETRO_REV02
            SKU::userCore->focusState = 1;
#endif
            break;

        case SDL_APP_TERMINATING: isRunning = false; break;

        case SDL_KEYDOWN:
#if !RETRO_REV02
            ++RSDK::SKU::buttonDownCount;
#endif
            switch (event.key.keysym.scancode) {
                default:
#if RETRO_INPUTDEVICE_KEYBOARD
                    SKU::UpdateKeyState(event.key.keysym.scancode);
#endif
                    break;

#if !RETRO_REV02 && RETRO_INPUTDEVICE_KEYBOARD
                case SDL_SCANCODE_ESCAPE:
                    SKU::UpdateKeyState(event.key.keysym.scancode);
                    RSDK::SKU::specialKeyStates[0] = true;
                    break;
                case SDL_SCANCODE_RETURN:
                    SKU::UpdateKeyState(event.key.keysym.scancode);
                    RSDK::SKU::specialKeyStates[1] = true;
                    break;
#endif

#if !RETRO_USE_ORIGINAL_CODE
                case SDL_SCANCODE_F1:
                    if (engine.devMenu) {
                        sceneInfo.listPos--;
                        if (sceneInfo.listPos < sceneInfo.listCategory[sceneInfo.activeCategory].sceneOffsetStart
                            || sceneInfo.listPos >= sceneInfo.listCategory[sceneInfo.activeCategory].sceneOffsetEnd) {
                            sceneInfo.activeCategory--;
                            if (sceneInfo.activeCategory >= sceneInfo.categoryCount) {
                                sceneInfo.activeCategory = sceneInfo.categoryCount - 1;
                            }
                            sceneInfo.listPos = sceneInfo.listCategory[sceneInfo.activeCategory].sceneOffsetEnd - 1;
                        }
#if RETRO_REV0U
                        switch (engine.version) {
                            default: break;
                            case 5: LoadScene(); break;
                            case 4:
                            case 3: RSDK::Legacy::stageMode = RSDK::Legacy::STAGEMODE_LOAD; break;
                        }
#else
                        LoadScene();
#endif
                    }
                    break;

                case SDL_SCANCODE_F2:
                    if (engine.devMenu) {
                        sceneInfo.listPos++;
                        if (sceneInfo.listPos >= sceneInfo.listCategory[sceneInfo.activeCategory].sceneOffsetEnd || sceneInfo.listPos == 0) {
                            sceneInfo.activeCategory++;
                            if (sceneInfo.activeCategory >= sceneInfo.categoryCount) {
                                sceneInfo.activeCategory = 0;
                            }
                            sceneInfo.listPos = sceneInfo.listCategory[sceneInfo.activeCategory].sceneOffsetStart;
                        }
#if RETRO_REV0U
                        switch (engine.version) {
                            default: break;
                            case 5: LoadScene(); break;
                            case 4:
                            case 3: RSDK::Legacy::stageMode = RSDK::Legacy::STAGEMODE_LOAD; break;
                        }
#else
                        LoadScene();
#endif
                    }
                    break;
#endif

                case SDL_SCANCODE_F3:
                    if (userShaderCount)
                        videoSettings.shaderID = (videoSettings.shaderID + 1) % userShaderCount;
                    break;

#if !RETRO_USE_ORIGINAL_CODE
                case SDL_SCANCODE_F4:
                    if (engine.devMenu)
                        engine.showEntityInfo ^= 1;
                    break;

                case SDL_SCANCODE_F5:
                    if (engine.devMenu) {
#if RETRO_USE_MOD_LOADER
                        if (event.key.keysym.mod & KMOD_LCTRL)
                            RefreshModFolders();
#endif
#if RETRO_REV0U
                        switch (engine.version) {
                            default: break;
                            case 5: LoadScene(); break;
                            case 4:
                            case 3: RSDK::Legacy::stageMode = RSDK::Legacy::STAGEMODE_LOAD; break;
                        }
#else
                        LoadScene();
#endif
                    }
                    break;

                case SDL_SCANCODE_F6:
                    if (engine.devMenu && videoSettings.screenCount > 1)
                        videoSettings.screenCount--;
                    break;

                case SDL_SCANCODE_F7:
                    if (engine.devMenu && videoSettings.screenCount < SCREEN_COUNT)
                        videoSettings.screenCount++;
                    break;

                case SDL_SCANCODE_F8:
                    if (engine.devMenu)
                        engine.showUpdateRanges ^= 1;
                    break;

                case SDL_SCANCODE_F9:
                    if (engine.devMenu)
                        showHitboxes ^= 1;
                    break;

                case SDL_SCANCODE_F10:
                    if (engine.devMenu)
                        engine.showPaletteOverlay ^= 1;
                    break;
#endif
                case SDL_SCANCODE_BACKSPACE:
                    if (engine.devMenu)
                        engine.gameSpeed = engine.fastForwardSpeed;
                    break;

                case SDL_SCANCODE_F11:
                case SDL_SCANCODE_INSERT:
                    if (engine.devMenu)
                        engine.frameStep = true;
                    break;

                case SDL_SCANCODE_F12:
                case SDL_SCANCODE_PAUSE:
                    if (engine.devMenu) {
#if RETRO_REV0U
                        switch (engine.version) {
                            default: break;
                            case 5:
                                if (sceneInfo.state != ENGINESTATE_NONE)
                                    sceneInfo.state ^= ENGINESTATE_STEPOVER;
                                break;
                            case 4:
                            case 3:
                                if (RSDK::Legacy::stageMode != ENGINESTATE_NONE)
                                    RSDK::Legacy::stageMode ^= RSDK::Legacy::STAGEMODE_STEPOVER;
                                break;
                        }
#else
                        if (sceneInfo.state != ENGINESTATE_NONE)
                            sceneInfo.state ^= ENGINESTATE_STEPOVER;
#endif
                    }
                    break;
            }
            break;

        case SDL_KEYUP:
#if !RETRO_REV02
            --RSDK::SKU::buttonDownCount;
#endif
            switch (event.key.keysym.scancode) {
                default:
#if RETRO_INPUTDEVICE_KEYBOARD
                    SKU::ClearKeyState(event.key.keysym.scancode);
#endif
                    break;

#if !RETRO_REV02 && RETRO_INPUTDEVICE_KEYBOARD
                case SDL_SCANCODE_ESCAPE:
                    RSDK::SKU::specialKeyStates[0] = false;
                    SKU::ClearKeyState(event.key.keysym.scancode);
                    break;

                case SDL_SCANCODE_RETURN:
                    RSDK::SKU::specialKeyStates[1] = false;
                    SKU::ClearKeyState(event.key.keysym.scancode);
                    break;
#endif
                case SDL_SCANCODE_BACKSPACE: engine.gameSpeed = 1; break;
            }
            break;

        case SDL_QUIT: isRunning = false; break;
    }
}

bool RenderDevice::ProcessEvents()
{
    SDL_Event sdlEvent;

    while (SDL_PollEvent(&sdlEvent)) {
        ProcessEvent(sdlEvent);

        if (!isRunning)
            return false;
    }

    return isRunning;
}

void RenderDevice::SetupImageTexture(int32 width, int32 height, uint8 *imagePixels)
{
    lastTextureFormat = SHADER_RGB_IMAGE;

    if (!imageTexture.pixels)
        return;

    uint32 *pixels        = (uint32 *)imageTexture.pixels;
    uint32 *imagePixels32 = (uint32 *)imagePixels;
    int32 pitch           = (imageTexture.pitch / sizeof(uint32)) - width;

    for (int32 y = 0; y < height; ++y) {
        for (int32 x = 0; x < width; ++x) {
            *pixels++ = *imagePixels32++;
        }
        pixels += pitch;
    }
}

static inline uint8_t ClampU8(int32_t val)
{
    if (val < 0)
        return 0;
    if (val > 255)
        return 255;
    return (uint8_t)val;
}

void RenderDevice::SetupVideoTexture_YUV420(int32 width, int32 height, uint8 *yPlane, uint8 *uPlane, uint8 *vPlane, int32 strideY, int32 strideU,
                                            int32 strideV)
{
    lastTextureFormat = SHADER_RGB_IMAGE;
    videoWidth        = (uint32)width;
    videoHeight       = (uint32)height;

    if (!imageTexture.pixels)
        return;

    uint32 *dstARGB    = (uint32 *)imageTexture.pixels;
    int32 dstPitch32   = imageTexture.pitch / sizeof(uint32);

    for (int32 y = 0; y < height; ++y) {
        const uint8 *ySrc = yPlane + y * strideY;
        const uint8 *uSrc = uPlane + (y >> 1) * strideU;
        const uint8 *vSrc = vPlane + (y >> 1) * strideV;
        uint32 *rowDst    = dstARGB + y * dstPitch32;

        for (int32 x = 0; x < width; ++x) {
            int32 Y = (int32)ySrc[x] - 16;
            int32 U = (int32)uSrc[x >> 1] - 128;
            int32 V = (int32)vSrc[x >> 1] - 128;

            int32 C = Y < 0 ? 0 : Y;
            int32 r = (298 * C + 409 * V + 128) >> 8;
            int32 g = (298 * C - 100 * U - 208 * V + 128) >> 8;
            int32 b = (298 * C + 516 * U + 128) >> 8;

            rowDst[x] = (0xFF << 24) | (ClampU8(r) << 16) | (ClampU8(g) << 8) | ClampU8(b);
        }
    }
}

void RenderDevice::SetupVideoTexture_YUV422(int32 width, int32 height, uint8 *yPlane, uint8 *uPlane, uint8 *vPlane, int32 strideY, int32 strideU,
                                            int32 strideV)
{
    lastTextureFormat = SHADER_RGB_IMAGE;
    videoWidth        = (uint32)width;
    videoHeight       = (uint32)height;

    if (!imageTexture.pixels)
        return;

    uint32 *dstARGB    = (uint32 *)imageTexture.pixels;
    int32 dstPitch32   = imageTexture.pitch / sizeof(uint32);

    for (int32 y = 0; y < height; ++y) {
        const uint8 *ySrc = yPlane + y * strideY;
        const uint8 *uSrc = uPlane + y * strideU;
        const uint8 *vSrc = vPlane + y * strideV;
        uint32 *rowDst    = dstARGB + y * dstPitch32;

        for (int32 x = 0; x < width; ++x) {
            int32 Y = (int32)ySrc[x] - 16;
            int32 U = (int32)uSrc[x >> 1] - 128;
            int32 V = (int32)vSrc[x >> 1] - 128;

            int32 C = Y < 0 ? 0 : Y;
            int32 r = (298 * C + 409 * V + 128) >> 8;
            int32 g = (298 * C - 100 * U - 208 * V + 128) >> 8;
            int32 b = (298 * C + 516 * U + 128) >> 8;

            rowDst[x] = (0xFF << 24) | (ClampU8(r) << 16) | (ClampU8(g) << 8) | ClampU8(b);
        }
    }
}

void RenderDevice::SetupVideoTexture_YUV444(int32 width, int32 height, uint8 *yPlane, uint8 *uPlane, uint8 *vPlane, int32 strideY, int32 strideU,
                                            int32 strideV)
{
    lastTextureFormat = SHADER_RGB_IMAGE;
    videoWidth        = (uint32)width;
    videoHeight       = (uint32)height;

    if (!imageTexture.pixels)
        return;

    uint32 *dstARGB    = (uint32 *)imageTexture.pixels;
    int32 dstPitch32   = imageTexture.pitch / sizeof(uint32);

    for (int32 y = 0; y < height; ++y) {
        const uint8 *ySrc = yPlane + y * strideY;
        const uint8 *uSrc = uPlane + y * strideU;
        const uint8 *vSrc = vPlane + y * strideV;
        uint32 *rowDst    = dstARGB + y * dstPitch32;

        for (int32 x = 0; x < width; ++x) {
            int32 Y = (int32)ySrc[x] - 16;
            int32 U = (int32)uSrc[x] - 128;
            int32 V = (int32)vSrc[x] - 128;

            int32 C = Y < 0 ? 0 : Y;
            int32 r = (298 * C + 409 * V + 128) >> 8;
            int32 g = (298 * C - 100 * U - 208 * V + 128) >> 8;
            int32 b = (298 * C + 516 * U + 128) >> 8;

            rowDst[x] = (0xFF << 24) | (ClampU8(r) << 16) | (ClampU8(g) << 8) | ClampU8(b);
        }
    }
}
