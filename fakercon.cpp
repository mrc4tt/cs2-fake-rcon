#include <stdio.h>
#include "fakercon.h"
#include <string>
#include "filemanager.h"
#include "abstract.h"
#include "tier0/icommandline.h"
#include "filesystem/ifilesystem.h"
#include <engine2/isource2server.h>
#include <engine2/ivengineserver2.h>

void Debug(const char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[1024] = {};
	V_vsnprintf(buf, sizeof(buf) - 1, msg, args);

	META_CONPRINTF("%s\n", buf);

	va_end(args);
}

FakeRcon g_FakeRcon;
ISource2GameClients *gameclients = NULL;
IVEngineServer2 *engine = NULL;
ICvar *icvar = NULL;
CFileManager *g_fileManager = NULL;
IFileSystem *g_fileSystem = NULL;

PlayerData g_playerData[MAXPLAYERS + 1];

// The password lives in a std::string: strdup()/free() pairs went through two different
// allocators here (libc and the engine's) and crashed the server when it shut down.
static std::string g_rconPasswordStorage;
const char *g_szRconPassword;

static void SetRconPassword(const char *password)
{
	g_rconPasswordStorage = password ? password : "";
	g_szRconPassword = password ? g_rconPasswordStorage.c_str() : nullptr;
}

// How often the ClientFullyConnect hook ran: reported by fake_rcon_selftest.
static int g_nFullyConnectCalls = 0;

// Console answer to the player who typed a command. A bot has no console (and no network
// channel to send one to), so it gets none: fake_rcon_selftest plays a player with a bot.
static void Reply(CPlayerSlot slot, const char *message)
{
	const char *networkId = engine->GetPlayerNetworkIDString(slot);
	if (networkId && !V_stricmp(networkId, "BOT"))
	{
		return;
	}

	g_SMAPI->ClientConPrintf(slot, "%s", message);
}

static bool IsRconPassword(const char *password)
{
	return g_szRconPassword != nullptr && password != nullptr && strcmp(password, g_szRconPassword) == 0;
}

FakeRcon::FakeRcon() :
	m_ClientFullyConnect(&ISource2GameClients::ClientFullyConnect, this, &FakeRcon::Hook_ClientFullyConnect, nullptr)
{
}

PLUGIN_EXPOSE(FakeRcon, g_FakeRcon);
bool FakeRcon::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_ANY(GetServerFactory, gameclients, ISource2GameClients, SOURCE2GAMECLIENTS_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, icvar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetFileSystemFactory, g_fileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);

	m_ClientFullyConnect.Add(gameclients);

	g_fileManager = new CFileManager();

	if (CommandLine()->HasParm("-fakercon"))
	{
		// ParmValue returns NULL when -fakercon has no value (e.g. it is the last argument).
		const char *rconPasswordConst = CommandLine()->ParmValue("-fakercon");
		SetRconPassword(rconPasswordConst);

		Debug("[FAKE RCON] Fetching RCON from -fakercon command line");
	}
	else
	{
		SetRconPassword(g_fileManager->GetRconPassword());

		Debug("[FAKE RCON] Fetching RCON from %s", CONFIG_FILE);
	}

	// A missing, short or default password must disable fake_rcon, not just warn: the commands only check for
	// nullptr, and "changeme" is what CFileManager writes on first run, i.e. a publicly known password.
	if (!g_szRconPassword || strlen(g_szRconPassword) < 4 || !V_stricmp(g_szRconPassword, "changeme"))
	{
		Debug("[FAKE RCON] Password is missing, too short (min 4 chars) or still the default \"changeme\" - fake_rcon is DISABLED.");
		Debug("[FAKE RCON] Set it in game/csgo/%s (Valve KeyValues format) or via the -fakercon launch parameter.", CONFIG_FILE);

		SetRconPassword(nullptr);
	}
	else
	{
		// Never print the password itself: console output ends up in server logs and panel consoles.
		Debug("[FAKE RCON] Password set (%zu chars)", strlen(g_szRconPassword));
	}

	g_pCVar = icvar;
	ConVar_Register(FCVAR_RELEASE | FCVAR_CLIENT_CAN_EXECUTE | FCVAR_GAMEDLL);

	return true;
}

bool FakeRcon::Unload(char *error, size_t maxlen)
{
	m_ClientFullyConnect.Remove(gameclients);

	// Take our commands out of the engine's list: it still walks that list after this
	// library is unloaded, and crashed the server on shutdown.
	ConVar_Unregister();

	SetRconPassword(nullptr);

	delete g_fileManager;
	g_fileManager = nullptr;

	return true;
}

void FakeRcon::AllPluginsLoaded()
{
}

KHook::Return<void> FakeRcon::Hook_ClientFullyConnect(ISource2GameClients *, CPlayerSlot slot)
{
	g_nFullyConnectCalls++;

	PlayerData *pData = &g_playerData[slot.Get()];
	pData->logged = false; // Unlog for the security, the cache will be checked after the first fake_rcon cmd typed by the client

	return { KHook::Action::Ignore };
}

CON_COMMAND_EXTERN(fake_rcon_cache_clean, Command_FakeRconCacheClean, "Clean the fake rcon cache");
void Command_FakeRconCacheClean(const CCommandContext &context, const CCommand &args)
{
	CPlayerSlot slot = context.GetPlayerSlot();

	if (g_szRconPassword == nullptr || !slot.IsValid())
	{
		return;
	}

	PlayerData *pData = &g_playerData[slot.Get()];

	if (!pData->logged)
	{
		const char *networkId = engine->GetPlayerNetworkIDString(slot);
		if (!networkId || !g_fileManager->IsSteamIdCached(networkId))
		{
			Reply(slot, "First, enter the password with the fake_rcon_password command\n");
			return;
		}

		pData->logged = true;
	}

	g_fileManager->CleanCache();
	Reply(slot, "Cache has been cleaned\n");
}

CON_COMMAND_EXTERN(fake_rcon, Command_FakeRcon, "fake_rcon {CMD}");
void Command_FakeRcon(const CCommandContext &context, const CCommand &args)
{
	CPlayerSlot slot = context.GetPlayerSlot();

	if (g_szRconPassword == nullptr || !slot.IsValid())
	{
		return;
	}

	if (args.ArgC() < 2)
	{
		Reply(slot, "Usage: fake_rcon <command>\n");
		return;
	}

	PlayerData *pData = &g_playerData[slot.Get()];

	if (!pData->logged)
	{
		const char *networkId = engine->GetPlayerNetworkIDString(slot);
		if (!networkId || !g_fileManager->IsSteamIdCached(networkId))
		{
			Reply(slot, "First, enter the password with the fake_rcon_password command\n");
			return;
		}

		pData->logged = true;
	}

	const char *command = args.ArgS();
	if (!command || !*command)
	{
		return;
	}

	engine->ServerCommand(command);
}

CON_COMMAND_EXTERN(fake_rcon_password, Command_FakeRconPassword, "fake_rcon_password {CMD}");
void Command_FakeRconPassword(const CCommandContext &context, const CCommand &args)
{
	CPlayerSlot slot = context.GetPlayerSlot();

	if (g_szRconPassword == nullptr || !slot.IsValid())
	{
		return;
	}

	if (args.ArgC() < 2)
	{
		Reply(slot, "Usage: fake_rcon_password <password>\n");
		return;
	}

	PlayerData *pData = &g_playerData[slot.Get()];

	if (pData->logged)
	{
		Reply(slot, "You are already using the fake_rcon command\n");
		return;
	}

	const char *password = args[1];
	if (!IsRconPassword(password))
	{
		Reply(slot, "Bad password !\n");
		return;
	}

	Reply(slot, "You can now use the fake_rcon command\n");
	pData->logged = true;

	const char *networkId = engine->GetPlayerNetworkIDString(slot);
	if (networkId)
	{
		g_fileManager->AddSteamIdToCache(networkId);
	}
}

void Command_FakeRcon(const CCommandContext &context, const CCommand &args);
void Command_FakeRconPassword(const CCommandContext &context, const CCommand &args);

// What the self test has fake_rcon run as a player: the value it sets is how the result is read back.
#define SELFTEST_CVAR "mp_teamname_1"
#define SELFTEST_VALUE "FAKE_RCON_SELFTEST_OK"

// Runs a console line through the real command handler, as the player in `slot`.
static void RunAsPlayer(void (*handler)(const CCommandContext &, const CCommand &), CPlayerSlot slot, const char *line)
{
	CCommand args;
	args.Tokenize(line);
	handler(CCommandContext(CT_FIRST_SPLITSCREEN_CLIENT, slot), args);
}

// Server console / RCON only: checks the parts of the plugin a CS2 update can break. The
// automated smoke test runs it after every build. A player who types it gets no answer.
//
// On a server started with -fake_rcon_selftest it also plays a player: a bot's slot goes
// through the real fake_rcon_password and fake_rcon commands (wrong password, right
// password, then two commands). That logs the bot in and writes it to the cache, so it
// is only done on a test server started for it.
CON_COMMAND_EXTERN(fake_rcon_selftest, Command_FakeRconSelfTest, "Check that fake_rcon works (server console only)");
void Command_FakeRconSelfTest(const CCommandContext &context, const CCommand &args)
{
	if (context.GetPlayerSlot().IsValid())
	{
		return;
	}

	int failed = 0;
	auto check = [&failed](bool ok, const char *what)
	{
		Debug("[FAKE RCON] SELFTEST %s: %s", ok ? "ok" : "FAILED", what);
		if (!ok)
		{
			failed++;
		}
	};

	check(engine != nullptr && gameclients != nullptr && icvar != nullptr && g_fileSystem != nullptr, "engine interfaces");
	check(g_szRconPassword != nullptr, "a password is set (-fakercon or " CONFIG_FILE ")");

	// Config and cache go through the game's filesystem and KeyValues.
	check(g_fileManager != nullptr && g_fileSystem != nullptr && g_fileSystem->FileExists(CONFIG_FILE, "MOD"), "config file is readable");
	check(g_fileManager != nullptr && !g_fileManager->IsSteamIdCached("fake_rcon_selftest_no_such_id"), "cache lookup of an unknown SteamID");

	if (g_szRconPassword != nullptr)
	{
		std::string wrong = std::string(g_szRconPassword) + "x";
		check(IsRconPassword(g_szRconPassword), "the right password is accepted");
		check(!IsRconPassword(wrong.c_str()) && !IsRconPassword("") && !IsRconPassword(nullptr), "a wrong password is refused");
	}

	if (failed == 0 && CommandLine()->HasParm("-fake_rcon_selftest"))
	{
		// A bot is a player with a slot of its own: the engine names it "BOT".
		int bot = -1;
		for (int i = 0; i < MAXPLAYERS && bot < 0; i++)
		{
			const char *networkId = engine->GetPlayerNetworkIDString(CPlayerSlot(i));
			if (networkId && !V_stricmp(networkId, "BOT"))
			{
				bot = i;
			}
		}

		check(bot >= 0, "a bot is on the server to play the player (start with +bot_quota 1)");
		if (bot >= 0)
		{
			CPlayerSlot slot(bot);
			PlayerData *pData = &g_playerData[bot];
			std::string right = std::string("fake_rcon_password \"") + g_szRconPassword + "\"";
			std::string wrong = std::string("fake_rcon_password \"") + g_szRconPassword + "x\"";

			g_fileManager->CleanCache();
			pData->logged = false;

			RunAsPlayer(Command_FakeRcon, slot, "fake_rcon " SELFTEST_CVAR " FAKE_RCON_SELFTEST_NOT_LOGGED_IN");
			check(!pData->logged, "player: fake_rcon before the password is refused");

			RunAsPlayer(Command_FakeRconPassword, slot, wrong.c_str());
			check(!pData->logged, "player: fake_rcon_password with a wrong password is refused");

			RunAsPlayer(Command_FakeRconPassword, slot, right.c_str());
			check(pData->logged, "player: fake_rcon_password with the right password logs in");
			check(g_fileManager->IsSteamIdCached("BOT"), "player: the login is remembered in the cache");

			// The engine runs these on its next frames; whoever runs the self test reads
			// SELFTEST_CVAR back and looks for the chat line.
			RunAsPlayer(Command_FakeRcon, slot, "fake_rcon " SELFTEST_CVAR " " SELFTEST_VALUE);
			RunAsPlayer(Command_FakeRcon, slot, "fake_rcon say hi from fake_rcon_selftest");
			Debug("[FAKE RCON] SELFTEST info: sent \"fake_rcon %s %s\" and \"fake_rcon say hi from fake_rcon_selftest\" as the bot in slot %d", SELFTEST_CVAR, SELFTEST_VALUE, bot);

			pData->logged = false;
			g_fileManager->CleanCache();
		}
	}
	else if (failed == 0)
	{
		Debug("[FAKE RCON] SELFTEST info: player commands not played (only on a server started with -fake_rcon_selftest)");
	}

	Debug("[FAKE RCON] SELFTEST info: ClientFullyConnect hook ran %d time(s)", g_nFullyConnectCalls);
	Debug("[FAKE RCON] SELFTEST %s (version %s)", failed == 0 ? "PASS" : "FAIL", g_FakeRcon.GetVersion());
}

bool FakeRcon::Pause(char *error, size_t maxlen)
{
	return true;
}

bool FakeRcon::Unpause(char *error, size_t maxlen)
{
	return true;
}

const char *FakeRcon::GetLicense()
{
	return "Public Domain";
}

const char *FakeRcon::GetVersion()
{
	return "1.3.5";
}

const char *FakeRcon::GetDate()
{
	return __DATE__;
}

const char *FakeRcon::GetLogTag()
{
	return "STUB";
}

const char *FakeRcon::GetAuthor()
{
	return "Kriax (forked by Miksen)";
}

const char *FakeRcon::GetDescription()
{
	return "Like the real RCON, but it's not the real RCON, thank you Valve";
}

const char *FakeRcon::GetName()
{
	return "Fake RCON";
}

const char *FakeRcon::GetURL()
{
	return "https://kriax.ovh";
}
