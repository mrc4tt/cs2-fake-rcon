#pragma once

#include <string>
#include "tier0/keyvalues.h"
#include <ctime>
#include "abstract.h"

#define CACHE_FILE "addons/configs/fake_rcon/cache.cfg"
#define CONFIG_FILE "addons/configs/fake_rcon/config.cfg"

extern IFileSystem *g_fileSystem;

class CFileManager
{
public:
    CFileManager()
    {
        Init();
    }


    void CleanCache();
    bool IsSteamIdCached(const char *steamid);
    void AddSteamIdToCache(const char *steamid);

    int GetCacheTime() { return g_cacheTime; }
    // nullptr when the config has no password.
    const char *GetRconPassword() { return m_hasRconPassword ? m_rconPassword.c_str() : nullptr; }

private:
    int g_cacheTime = 120;
    std::string m_rconPassword;
    bool m_hasRconPassword = false;
    void Init();
};

extern CFileManager *g_fileManager;
