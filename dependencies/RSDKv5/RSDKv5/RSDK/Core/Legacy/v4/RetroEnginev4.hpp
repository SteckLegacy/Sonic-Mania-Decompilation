
namespace Legacy
{
namespace v4
{
enum RetroStates {
    ENGINE_WAIT      = 3,
    ENGINE_INITPAUSE = 5,
    ENGINE_EXITPAUSE = 6,
    ENGINE_ENDGAME   = 7,
    ENGINE_RESETGAME = 8,
};

bool32 LoadGameConfig(const char *filepath);
void ProcessEngine();

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

} // namespace v4
} // namespace Legacy