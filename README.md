# CS2 Fake RCON

An extension for Counter-Strike 2 that restores RCON functionality by adding custom RCON commands.

## Features

FakeRCON adds the `fake_rcon_password` and `fake_rcon` commands to Counter-Strike 2, compensating for Valve's omission or breaking of the native RCON functionality.

## Installation

1. Download the package for your operating system:
   - [Linux](https://github.com/mrc4tt/cs2-fake-rcon/releases/latest/download/linux.tar.gz)
   - [Windows](https://github.com/mrc4tt/cs2-fake-rcon/releases/latest/download/windows.zip)
2. Extract the downloaded archive (`.tar.gz` or `.zip`).
3. Upload the **fake_rcon** folder to your server path at `game/csgo/addons`.

## Configuration

For setup instructions and configuration details, refer to the [AlliedModders Guide](https://forums.alliedmods.net/showpost.php?p=2811082&postcount=15).

> **Note:** The password must be at least **4 characters long**. The password is read from a configuration file rather than a ConVar, so it is not exposed in `server.cfg`, launch options or `cvarlist`.

## Usage

Set your RCON password:
```text
fake_rcon_password YOURPWD
```

Execute commands:

```text
fake_rcon say hello
```

## Support

Need help, found a bug, or have a feature idea? Join the Discord support server:

[![Discord](https://img.shields.io/badge/Discord-Join%20the%20server-5865F2?logo=discord&logoColor=white)](https://discord.gg/zuQN3U2ZEh)

- **Questions and setup help:** ask in the support channels on Discord.
- **Bug reports:** open a [GitHub issue](https://github.com/mrc4tt/cs2-fake-rcon/issues) with your server log, plugin version and steps to reproduce. Discord is fine for a quick check first.
- **Feature requests:** post them on Discord or open an [issue](https://github.com/mrc4tt/cs2-fake-rcon/issues).

Please do not send support requests by direct message. Asking in the public channels means others can help and find the answer later.

## Credits

* **Original Creator:** [Kriax](https://github.com/Salvatore-Als/cs2-fake-rcon)
* **Maintainer:** [mrc4tt](https://github.com/mrc4tt)
