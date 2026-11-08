
namespace Legacy
{
namespace v3
{
enum RetroStates {
    ENGINE_EXITGAME        = 3,
    ENGINE_ENTER_HIRESMODE = 5,
    ENGINE_EXIT_HIRESMODE  = 6,
    ENGINE_PAUSE           = 7,
    ENGINE_WAIT            = 8,
    ENGINE_VIDEOWAIT       = 9,
};

extern int32 engineMessage;

bool32 LoadGameConfig(const char *filepath);
void ProcessEngine();

void RetroEngineCallback(int32 callbackID);

#if RETRO_USE_MOD_LOADER
void LoadGameXML(bool pal = false);
void LoadXMLWindowText(const pugi::xml_node &gameElement);
void LoadXMLVariables(const pugi::xml_node &gameElement);
void LoadXMLPalettes(const pugi::xml_node &gameElement);
void LoadXMLObjects(const pugi::xml_node &gameElement);
void LoadXMLSoundFX(const pugi::xml_node &gameElement);
void LoadXMLPlayers(const pugi::xml_node &gameElement);
void LoadXMLStages(const pugi::xml_node &gameElement);
#endif

} // namespace v3
} // namespace Legacy