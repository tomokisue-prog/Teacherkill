#include "SceneManager.h"

int main()
{
    SceneManager::RunConfig cfg{};
#ifdef NDEBUG
    cfg.enableDebugUI = false;
    cfg.windowed = false;
#else
    cfg.enableDebugUI = true;
#endif
    SM().SetRunConfig(cfg);
    SM().Init();
    SM().Run();
    SM().Shutdown();

    return 0;
}