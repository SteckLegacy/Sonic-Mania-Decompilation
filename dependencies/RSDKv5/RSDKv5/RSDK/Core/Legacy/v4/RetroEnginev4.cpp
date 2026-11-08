#include "RetroEnginev4.hpp"

bool32 RSDK::Legacy::v4::LoadGameConfig(const char *filepath)
{
    char strBuffer[0x40];
    StrCopy(gameVerInfo.gameTitle, "Retro-Engine"); // this is the default window name

    globalVariablesCount = 0;
#if RETRO_USE_MOD_LOADER
    modSettings.playerCount = 0;
#endif

    FileInfo info;
    InitFileInfo(&info);

    bool32 loaded = LoadFile(&info, filepath, FMODE_RB);
    if (loaded) {
        ReadString(&info, gameVerInfo.gameTitle);
        ReadString(&info, gameVerInfo.gameSubtitle);

        uint8 buf[3];
        for (int32 c = 0; c < 0x60; ++c) {
            ReadBytes(&info, buf, 3);
            SetPaletteEntry(-1, c, buf[0], buf[1], buf[2]);
        }

        // Read Obect Names
        uint8 objectCount = ReadInt8(&info);
        for (int32 o = 0; o < objectCount; ++o) {
            ReadString(&info, strBuffer);
        }

        // Read Script Paths
        for (int32 s = 0; s < objectCount; ++s) {
            ReadString(&info, strBuffer);
        }

        globalVariablesCount = ReadInt8(&info);
        for (int32 v = 0; v < globalVariablesCount; ++v) {
            // Read Variable Name
            ReadString(&info, globalVariables[v].name);

            // Read Variable Value
            globalVariables[v].value = ReadInt8(&info) << 0;
            globalVariables[v].value |= ReadInt8(&info) << 8;
            globalVariables[v].value |= ReadInt8(&info) << 16;
            globalVariables[v].value |= ReadInt8(&info) << 24;
        }

        // Read SFX
        globalSFXCount = ReadInt8(&info);
        for (int32 s = 0; s < globalSFXCount; ++s) { // Sfx Names
            ReadString(&info, strBuffer);
            SetSfxName(strBuffer, s);
        }

        for (int32 s = 0; s < globalSFXCount; ++s) { // Sfx Paths
            ReadString(&info, strBuffer);
            RSDK::Legacy::LoadSfx(strBuffer, s, SCOPE_GLOBAL);
        }

        // Read Player Names
        uint8 plrCount = ReadInt8(&info);
        for (uint8 p = 0; p < plrCount; ++p) {
            ReadString(&info, strBuffer);

#if RETRO_USE_MOD_LOADER
            // needed for PlayerName[] stuff in scripts
            StrCopy(modSettings.playerNames[p], strBuffer);
            modSettings.playerCount++;
#endif
        }

        // Read ahead and get scene count
        int32 storedPos = info.readPos;

        int32 totalSceneCount   = 0;
        sceneInfo.categoryCount = STAGELIST_MAX;
        for (uint8 c = 0; c < sceneInfo.categoryCount; ++c) {
            int32 count = ReadInt8(&info);
            for (int32 s = 0; s < count; ++s) {
                ReadString(&info, strBuffer);
                ReadString(&info, strBuffer);
                ReadString(&info, strBuffer);
                ReadInt8(&info);
            }

            totalSceneCount += count;
        }

        if (!totalSceneCount)
            totalSceneCount = 1;

        int32 startScene = 0;
#if !RETRO_USE_ORIGINAL_CODE
        if (strlen(currentSceneFolder) && strlen(currentSceneID)) {
#if RETRO_USE_MOD_LOADER
            listData.resize(totalSceneCount + 1);
            sceneInfo.listData = listData.data();
#else
            AllocateStorage((void **)&sceneInfo.listData, sizeof(SceneListEntry) * (totalSceneCount + 1), DATASET_STG, false);
#endif
            SceneListEntry *scene = &sceneInfo.listData[totalSceneCount];
            strcpy(scene->name, "_RSDK_SCENE");
            strcpy(scene->folder, currentSceneFolder);
            strcpy(scene->id, currentSceneID);
            scene->filter = sceneInfo.filter;
            GEN_HASH_MD5(scene->name, scene->hash);

            // Override existing values
            sceneInfo.activeCategory = 0;
            startScene               = totalSceneCount;
            currentSceneFolder[0]    = 0;
            currentSceneID[0]        = 0;
        }
        else {
#endif

#if RETRO_USE_MOD_LOADER
            listData.resize(totalSceneCount);
            sceneInfo.listData = listData.data();
#else
        AllocateStorage((void **)&sceneInfo.listData, sizeof(SceneListEntry) * totalSceneCount, DATASET_STG, false);
#endif //! RETRO_USE_MOD_LOADER

#if !RETRO_USE_ORIGINAL_CODE
        }
#endif

        AllocateStorage((void **)&sceneInfo.listCategory, sizeof(SceneListInfo) * sceneInfo.categoryCount, DATASET_STG, true);

        char categoryNames[STAGELIST_MAX][0x20] = {
            "Presentation",
            "Regular",
            "Special",
            "Bonus",
        };

        // Jump back and properly load scene info
        Seek_Set(&info, storedPos);

        int32 sceneID = 0;
        for (uint8 c = 0; c < sceneInfo.categoryCount; ++c) {
            SceneListInfo *category = &sceneInfo.listCategory[c];
            StrCopy(category->name, categoryNames[c]);
            GEN_HASH_MD5(category->name, category->hash);

            category->sceneOffsetStart = sceneID;
            category->sceneCount       = ReadInt8(&info);
            for (int32 s = 0; s < category->sceneCount; ++s) {
                SceneListEntry *scene = &sceneInfo.listData[sceneID + s];

                ReadString(&info, scene->folder);

                ReadString(&info, scene->id);

                ReadString(&info, scene->name);
                GEN_HASH_MD5(scene->name, scene->hash);

                // highlighted (doesn't care)
                ReadInt8(&info);
            }

            category->sceneOffsetEnd = category->sceneOffsetStart + category->sceneCount;
            sceneID += category->sceneCount;
        }

        CloseFile(&info);

        sceneInfo.listPos = startScene;

#if RETRO_USE_MOD_LOADER
        v4::LoadGameXML();
        SetGlobalVariableByName("options.devMenuFlag", engine.devMenu ? 1 : 0);
        SetGlobalVariableByName("engine.standalone", 0);
#endif

        SetGlobalVariableByName("game.hasPlusDLC", !RSDK_AUTOBUILD);

        usingBytecode = false;
        InitFileInfo(&info);
        if (useDataPack && LoadFile(&info, "Bytecode/GlobalCode.bin", FMODE_RB)) {
            usingBytecode = true;
            CloseFile(&info);
        }
    }

    // These need to be set every time its reloaded
    nativeFunctionCount = 0;

    nativeFunctionCount = 0;
    AddNativeFunction("SetAchievement", SetAchievement);
    AddNativeFunction("SetLeaderboard", SetLeaderboard);
    AddNativeFunction("HapticEffect", HapticEffect);

    // AddNativeFunction("Connect2PVS", Connect2PVS);
    // AddNativeFunction("Disconnect2PVS", Disconnect2PVS);
    // AddNativeFunction("SendEntity", SendEntity);
    // AddNativeFunction("SendValue", SendValue);
    // AddNativeFunction("ReceiveEntity", ReceiveEntity);
    // AddNativeFunction("ReceiveValue", ReceiveValue);
    // AddNativeFunction("TransmitGlobal", TransmitGlobal);
    // AddNativeFunction("ShowPromoPopup", ShowPromoPopup);

    AddNativeFunction("NotifyCallback", NotifyCallback);

#if RETRO_USE_MOD_LOADER
    AddNativeFunction("ExitGame", ExitGame);
    AddNativeFunction("FileExists", FileExists);
    AddNativeFunction("OpenModMenu", OpenModMenu); // Opens the dev menu-based mod menu incase you cant be bothered or smth
    AddNativeFunction("AddAchievement", AddGameAchievement);
    AddNativeFunction("SetAchievementDescription", SetAchievementDescription);
    AddNativeFunction("ClearAchievements", ClearAchievements);
    AddNativeFunction("GetAchievementCount", GetAchievementCount);
    AddNativeFunction("GetAchievement", GetAchievement);
    AddNativeFunction("GetAchievementName", GetAchievementName);
    AddNativeFunction("GetAchievementDescription", GetAchievementDescription);
    AddNativeFunction("GetScreenWidth", GetScreenWidth);
    AddNativeFunction("SetScreenWidth", SetScreenWidth);
    AddNativeFunction("GetWindowScale", GetWindowScale);
    AddNativeFunction("SetWindowScale", SetWindowScale);
    AddNativeFunction("GetWindowScaleMode", GetWindowScaleMode);
    AddNativeFunction("SetWindowScaleMode", SetWindowScaleMode);
    AddNativeFunction("GetWindowFullScreen", GetWindowFullScreen);
    AddNativeFunction("SetWindowFullScreen", SetWindowFullScreen);
    AddNativeFunction("GetWindowBorderless", GetWindowBorderless);
    AddNativeFunction("SetWindowBorderless", SetWindowBorderless);
    AddNativeFunction("GetWindowVSync", GetWindowVSync);
    AddNativeFunction("SetWindowVSync", SetWindowVSync);
    AddNativeFunction("ApplyWindowChanges", ApplyWindowChanges); // Refresh window after changing window options
    AddNativeFunction("GetModCount", GetModCount);
    AddNativeFunction("GetModName", GetModName);
    AddNativeFunction("GetModDescription", GetModDescription);
    AddNativeFunction("GetModAuthor", GetModAuthor);
    AddNativeFunction("GetModVersion", GetModVersion);
    AddNativeFunction("GetModActive", GetModActive);
    AddNativeFunction("SetModActive", SetModActive);
    AddNativeFunction("MoveMod", MoveMod);
    AddNativeFunction("RefreshEngine", RefreshEngine); // Reload engine after changing mod status
#endif

    return loaded;
}

void RSDK::Legacy::v4::ProcessEngine()
{
    switch (gameMode) {
        case ENGINE_DEVMENU:
            ProcessInput();
            currentScreen = &screens[0];

            if (devMenu.state)
                devMenu.state();
            break;

        case ENGINE_MAINGAME: ProcessStage(); break;

        case ENGINE_INITDEVMENU:
            LoadGameConfig("Data/Game/GameConfig.bin");
            InitFirstStage();
            ResetCurrentStageFolder();
            break;

        case ENGINE_WAIT:
            break;
            // case ENGINE_RESETGAME: break;

        case ENGINE_SCRIPTERROR: {
            currentScreen = screens;
            FillScreen(0x000000, 0x10, 0x10, 0x10);

            int32 yOff = DevOutput_GetStringYSize(scriptErrorMessage);
            DrawDevString(scriptErrorMessage, 8, currentScreen->center.y - (yOff >> 1) + 8, 0, 0xF0F0F0);

            ProcessInput();
            if (controller[CONT_ANY].keyStart.press || controller[CONT_ANY].keyA.press) {
                OpenDevMenu();
            }
            else if (controller[CONT_ANY].keyB.press) {
                ResetCurrentStageFolder();
                sceneInfo.activeCategory = 0;
                gameMode                 = ENGINE_MAINGAME;
                stageMode                = STAGEMODE_LOAD;
                sceneInfo.listPos        = 0;
            }
            else if (controller[CONT_ANY].keyC.press) {
                ResetCurrentStageFolder();
#if RETRO_USE_MOD_LOADER
                RefreshModFolders();
#endif
                gameMode  = ENGINE_MAINGAME;
                stageMode = STAGEMODE_LOAD;
            }
            break;
        }

        case ENGINE_INITPAUSE:
        case ENGINE_EXITPAUSE: gameMode = ENGINE_MAINGAME; break;

        case ENGINE_ENDGAME:
        case ENGINE_RESETGAME:
            sceneInfo.activeCategory = 0;
            sceneInfo.listPos        = 0;
            gameMode                 = ENGINE_MAINGAME;
            stageMode                = STAGEMODE_LOAD;
            break;

        default: break;
    }
}

#if RETRO_USE_MOD_LOADER
void RSDK::Legacy::v4::LoadGameXML(bool pal)
{
    FileInfo info;
    SortMods();
    for (int32 m = 0; m < modList.size(); ++m) {
        if (!modList[m].active)
            break;
        SetActiveMod(m);
        InitFileInfo(&info);
        if (LoadFile(&info, "Data/Game/Game.xml", FMODE_RB)) {
            pugi::xml_document *doc = new pugi::xml_document;

            char *xmlData = new char[info.fileSize + 1];
            ReadBytes(&info, xmlData, info.fileSize);
            xmlData[info.fileSize] = 0;
            CloseFile(&info);

            pugi::xml_parse_result result = doc->load_buffer_inplace(xmlData, info.fileSize, pugi::parse_default);
            pugi::xml_node gameElement    = doc->child("game");

            if (result && gameElement) {
                if (pal)
                    LoadXMLPalettes(gameElement);
                else {
                    LoadXMLWindowText(gameElement);
                    LoadXMLVariables(gameElement);
                    LoadXMLObjects(gameElement);
                    LoadXMLSoundFX(gameElement);
                    LoadXMLPlayers(gameElement);
                    LoadXMLStages(gameElement);
                }
            }
            else {
                PrintLog(PRINT_NORMAL, "[MOD] Failed to parse Game.xml file for mod %s", modList[m].id.c_str());
            }

            delete[] xmlData;
            delete doc;
        }
    }
    SetActiveMod(-1);
}

void RSDK::Legacy::v4::LoadXMLWindowText(const pugi::xml_node &gameElement)
{
    pugi::xml_node titleElement = gameElement.child("title");
    if (titleElement) {
        pugi::xml_attribute nameAttr = titleElement.attribute("name");
        if (nameAttr)
            StrCopy(gameVerInfo.gameTitle, nameAttr.value());
    }
}

void RSDK::Legacy::v4::LoadXMLVariables(const pugi::xml_node &gameElement)
{
    pugi::xml_node variablesElement = gameElement.child("variables");
    if (variablesElement) {
        for (pugi::xml_node varElement = variablesElement.child("variable"); varElement;
            varElement                 = varElement.next_sibling("variable")) {
            pugi::xml_attribute nameAttr = varElement.attribute("name");
            const char *varName          = nameAttr ? nameAttr.value() : "unknownVariable";

            pugi::xml_attribute valAttr = varElement.attribute("value");
            int32 varValue              = valAttr ? valAttr.as_int() : 0;

            StrCopy(globalVariables[globalVariablesCount].name, varName);
            globalVariables[globalVariablesCount].value = varValue;
            globalVariablesCount++;
        }
    }
}

void RSDK::Legacy::v4::LoadXMLPalettes(const pugi::xml_node &gameElement)
{
    pugi::xml_node paletteElement = gameElement.child("palette");
    if (paletteElement) {
        for (pugi::xml_node clrElement = paletteElement.child("color"); clrElement;
            clrElement                 = clrElement.next_sibling("color")) {
            pugi::xml_attribute bankAttr = clrElement.attribute("bank");
            int32 clrBank                = bankAttr ? bankAttr.as_int() : 0;

            pugi::xml_attribute indAttr = clrElement.attribute("index");
            int32 clrInd                = indAttr ? indAttr.as_int() : 0;

            pugi::xml_attribute rAttr = clrElement.attribute("r");
            int32 clrR                = rAttr ? rAttr.as_int() : 0;

            pugi::xml_attribute gAttr = clrElement.attribute("g");
            int32 clrG                = gAttr ? gAttr.as_int() : 0;

            pugi::xml_attribute bAttr = clrElement.attribute("b");
            int32 clrB                = bAttr ? bAttr.as_int() : 0;

            SetPaletteEntry(clrBank, clrInd, clrR, clrG, clrB);
        }

        for (pugi::xml_node clrsElement = paletteElement.child("colors"); clrsElement;
            clrsElement                 = clrsElement.next_sibling("colors")) {
            pugi::xml_attribute bankAttr = clrsElement.attribute("bank");
            int32 bank                   = bankAttr ? bankAttr.as_int() : 0;

            pugi::xml_attribute indAttr = clrsElement.attribute("start");
            int32 index                 = indAttr ? indAttr.as_int() : 0;

            std::string text = clrsElement.child_value();
            // working: AABBFF #FFaaFF (12, 32, 34) (145 53 234)
            std::regex search(R"((?:#?([0-9A-F]{2})([0-9A-F]{2})([0-9A-F]{2}))|(?:\((\d+),?\s*(\d+),?\s*(\d+)\)))",
                              std::regex_constants::icase | std::regex_constants::ECMAScript);
            std::smatch match;
            while (std::regex_search(text, match, search)) {
                int32 r, g, b;
                int32 base, start;
                if (match[1].matched) {
                    // we have hex
                    base  = 16;
                    start = 1;
                }
                else {
                    // triplet
                    base  = 10;
                    start = 4;
                }

                r = std::stoi(match[start + 0].str(), nullptr, base);
                g = std::stoi(match[start + 1].str(), nullptr, base);
                b = std::stoi(match[start + 2].str(), nullptr, base);

                SetPaletteEntry(bank, index++, r, g, b);
                text = match.suffix();
            }
        }
    }
}

void RSDK::Legacy::v4::LoadXMLObjects(const pugi::xml_node &gameElement)
{
    modObjCount = 0;

    pugi::xml_node objectsElement = gameElement.child("objects");
    if (objectsElement) {
        for (pugi::xml_node objElement = objectsElement.child("object"); objElement;
            objElement                 = objElement.next_sibling("object")) {
            pugi::xml_attribute nameAttr = objElement.attribute("name");
            const char *objName          = nameAttr ? nameAttr.value() : "unknownObject";

            pugi::xml_attribute scrAttr = objElement.attribute("script");
            const char *objScript       = scrAttr ? scrAttr.value() : "unknownObject.txt";

            uint8 flags = 0;

            // forces the object to be loaded, this means the object doesn't have to be and *SHOULD NOT* be in the stage object list
            // if it is, it'll cause issues!!!!
            pugi::xml_attribute loadAttr = objElement.attribute("forceLoad");
            int32 objForceLoad           = loadAttr ? loadAttr.as_bool() : false;

            flags |= (objForceLoad & 1);

            StrCopy(modTypeNames[modObjCount], objName);
            StrCopy(modScriptPaths[modObjCount], objScript);
            modScriptFlags[modObjCount] = flags;
            modObjCount++;
        }
    }
}

void RSDK::Legacy::v4::LoadXMLSoundFX(const pugi::xml_node &gameElement)
{
    pugi::xml_node soundsElement = gameElement.child("sounds");
    if (soundsElement) {
        for (pugi::xml_node sfxElement = soundsElement.child("soundfx"); sfxElement;
            sfxElement                 = sfxElement.next_sibling("soundfx")) {
            pugi::xml_attribute nameAttr = sfxElement.attribute("name");
            const char *sfxName          = nameAttr ? nameAttr.value() : "unknownSFX";

            pugi::xml_attribute valAttr = sfxElement.attribute("path");
            const char *sfxPath         = valAttr ? valAttr.value() : "unknownSFX.wav";

            SetSfxName(sfxName, globalSFXCount);

            RSDK::Legacy::LoadSfx((char *)sfxPath, globalSFXCount, SCOPE_GLOBAL);
            globalSFXCount++;
        }
    }
}

void RSDK::Legacy::v4::LoadXMLPlayers(const pugi::xml_node &gameElement)
{
    pugi::xml_node playersElement = gameElement.child("players");
    if (playersElement) {
        for (pugi::xml_node plrElement = playersElement.child("player"); plrElement;
            plrElement                 = plrElement.next_sibling("player")) {
            pugi::xml_attribute nameAttr = plrElement.attribute("name");
            const char *plrName          = nameAttr ? nameAttr.value() : "unknownPlayer";

            StrCopy(modSettings.playerNames[modSettings.playerCount++], plrName);
        }
    }
}

void RSDK::Legacy::v4::LoadXMLStages(const pugi::xml_node &gameElement)
{
    const char *elementNames[] = { "presentationStages", "regularStages", "bonusStages", "specialStages" };

    for (int32 l = 0; l < sceneInfo.categoryCount; ++l) {
        pugi::xml_node listElement = gameElement.child(elementNames[l]);
        SceneListInfo *list        = &sceneInfo.listCategory[l];
        if (listElement) {
            for (pugi::xml_node stgElement = listElement.child("stage"); stgElement;
                stgElement                 = stgElement.next_sibling("stage")) {
                pugi::xml_attribute nameAttr = stgElement.attribute("name");
                const char *stgName          = nameAttr ? nameAttr.value() : "unknownStage";

                pugi::xml_attribute folderAttr = stgElement.attribute("folder");
                const char *stgFolder          = folderAttr ? folderAttr.value() : "unknownStageFolder";

                pugi::xml_attribute idAttr = stgElement.attribute("id");
                const char *stgID          = idAttr ? idAttr.value() : "unknownStageID";

                listData.emplace(listData.begin() + list->sceneOffsetEnd);
                SceneListEntry *scene = &listData[list->sceneOffsetEnd];

                sprintf_s(scene->name, sizeof(scene->name), "%s", stgName);
                GEN_HASH_MD5(scene->name, scene->hash);
                sprintf_s(scene->folder, sizeof(scene->folder), "%s", stgFolder);
                sprintf_s(scene->id, sizeof(scene->id), "%s", stgID);

                scene->filter = 0xFF;

                list->sceneCount++;
                list->sceneOffsetEnd++;
                for (int32 c = l + 1; c < sceneInfo.categoryCount; ++c) sceneInfo.listCategory[c].sceneOffsetStart++;
            }
        }
    }
    sceneInfo.listData = listData.data();
}

#endif
