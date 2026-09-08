#define MAXPLAYERS 64

#include <ISmmPlugin.h>
#include <igameevents.h>

class ISource2GameClients;

class FakeRcon : public ISmmPlugin, public IMetamodListener
{
public:
	FakeRcon();

	bool Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late);
	bool Unload(char *error, size_t maxlen);
	bool Pause(char *error, size_t maxlen);
	bool Unpause(char *error, size_t maxlen);
	void AllPluginsLoaded();

	KHook::Return<void> Hook_ClientFullyConnect( ISource2GameClients *, CPlayerSlot nSlot );

protected:
	KHook::Virtual<ISource2GameClients, void, CPlayerSlot> m_ClientFullyConnect;

public:
	const char *GetAuthor();
	const char *GetName();
	const char *GetDescription();
	const char *GetURL();
	const char *GetLicense();
	const char *GetVersion();
	const char *GetDate();
	const char *GetLogTag();
};

struct PlayerData
{
	bool logged;
};

extern FakeRcon g_FakeRcon;

PLUGIN_GLOBALVARS();