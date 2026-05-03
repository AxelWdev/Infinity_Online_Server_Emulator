# Infinity C++ Server Emulator

![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)
![CMake](https://img.shields.io/badge/build-CMake-064F8C?logo=cmake&logoColor=white)
![Windows](https://img.shields.io/badge/platform-Windows-0078D4?logo=windows&logoColor=white)
![Status](https://img.shields.io/badge/status-server%20emulator%20research-orange)
![License: MIT](https://img.shields.io/badge/license-MIT-green)

Research-oriented C++20 server emulator for Infinity client protocol work.

This project is meant to emulate enough client-facing behavior for local protocol testing, login/lobby/shop flows, room setup, and ongoing gameplay research. It is not a complete private server, not a production MMORPG backend, and it does not contain official server code.

Affiliation disclaimer: this project is not affiliated with, endorsed by, or sponsored by Windysoft, GameTribe, Game Media Networks, Digital Bros, `game & game`, or any current or former Infinity Online rights holder.

Code and documentation authored for this repository are released under the [MIT License](LICENSE). The license does not grant rights to third-party trademarks, original Infinity Online game assets, client binaries, runtime DLLs, or external installer archives.

## At A Glance

| This repo is | This repo is not |
| --- | --- |
| A C++ server emulator for client protocol research | A complete private server |
| A runnable local test bed with starter data | A production MMORPG backend |
| A place to document confirmed packet behavior | A source of official server code |
| A sanitized public export | A dump of captures, logs, private accounts, or client binaries |

## Game Background

Infinity Online, also styled as INFINITY Online or INFINITY, was a Korean Windows PC free-to-play action MMOG/MMORPG developed by Windysoft. Public historical listings describe it as a lobby and room-based online hack-and-slash game with PvP arenas, cooperative PvE missions, combo-focused combat, guard companions, character unlocks, and the in-game currency Luna.

The game was associated with several publishers and operators across regions. Public catalogs list Windysoft as the developer and publisher for the original PC release, with `game & game` and `Gametribe` also appearing in publisher listings. European operation was tied to GameTribe / Game Media Networks, a Digital Bros online-games portal. JeuxOnLine records that the European service closed with GameTribe on December 31, 2009, after which the game remained available in South Korea on Windysoft servers before also being discontinued.

This emulator exists for preservation, interoperability, and protocol research around the original Infinity Online client. It focuses on reconstructing client/server behavior from observed packets and client-side behavior, while keeping proprietary client binaries, private captures, and official server code out of the public repository.

Historical references:

- [UVL: Infinity Online](https://www.uvlist.net/game-166908-Infinity%2BOnline)
- [JeuxOnLine: Infinity Online](https://www.jeuxonline.info/jeu/Infinity_Online)
- [GameFAQs: Infinity Online Release Details](https://gamefaqs.gamespot.com/pc/936377-infinity-online/data)
- [GameSource: Gametribe.com closure notice](https://www.gamesource.it/notizie/gametribe-com-chiude-definitivamente/)

## Current Status

| Area | Status | Notes |
| --- | --- | --- |
| Build and startup | Implemented | CMake project with `build.bat`, `run.bat`, starter JSON, and CSV data. |
| TCP transport | Implemented | LZSS stream handling, logical packet framing, logging, and opcode dispatch. |
| Login and lobby | Implemented locally | Covers known login challenge, lobby connect, channels, account, character, guard, item, skill, and quickslot flows. |
| Shop and inventory | Partially implemented locally | Supports item/skill purchase, currency checks, package expansion, equipment, removal, and supported quickslot assignment. |
| Mission room setup | Partially implemented | Covers TCP mission room list/create/info/state/start/leave flows used by the current emulator path. PvP room creation is not supported yet. |
| UDP gameplay | Experimental | Native-order sync work exists behind `--experimental-game-udp-sync`; mission gameplay is not complete. |
| Full MMORPG backend | Not implemented | No complete authoritative combat, rewards, matchmaking, account security, persistence layer, or production operations. |

## Roadmap Snapshot

| Done | Next | Later |
| --- | --- | --- |
| Runnable C++20 TCP emulator | Add tests for packet codecs, inventory, account persistence, and room flows | Production-grade account storage if public testing ever needs it |
| Starter account/config/data files | Stabilize multi-client TCP room behavior | Complete channel, matchmaking, and long-running operations tooling |
| Local HTTP endpoint helper | Verify UDP mission entry against captures | Full authoritative mission loop, combat, rewards, and progression |
| Injector source and build docs | Complete reviewed mission catalog coverage | Broader client-version compatibility documentation |

See [ROADMAP.md](ROADMAP.md) for the full implemented feature list, remaining work, and suggested milestones.

## Quick Start

```powershell
cmake -S . -B build
cmake --build build --config Debug
build\Debug\tcp_lzss_server_cpp.exe
```

On Windows, `build.bat Debug` or `build.bat Release` runs the same CMake flow.

Useful flags:

```powershell
build\Debug\tcp_lzss_server_cpp.exe --host 127.0.0.1 --port 8080 --game-udp-port 8081
```

On Windows, `run.bat Debug` builds and runs the server in one step. `run.bat` defaults to Release.

## Project Layout

| Path | Purpose |
| --- | --- |
| `src/` and `include/` | C++ emulator source, packet definitions, TCP server code, UDP research code, and tools. |
| `data/setting/` | Starter item, skill, character, and catalog CSV files. |
| `data/missions/` | Included starter mission layout data. |
| `http_server/` | Node.js `/net_gsp.php` endpoint helper for local client startup. |
| `injector/` | x86 client hook/injector source and build instructions. |
| `tools/` | Public helper scripts plus Infinity `.bin` pack unpacker/repacker source. |
| `client_runtime/` | Notes for external client runtime files that are not tracked in this repo. |
| `ROADMAP.md` | Detailed implementation status and future work. |

## Client Setup

| Source | Purpose |
| --- | --- |
| https://archive.org/details/infinity-online-client | Main client installer for normal setup. |
| https://archive.org/details/cbt-infinity-20101011-manual | Use only to extract `speedtreert.dll` if the main client install is missing it. |

Install or unpack the client outside this repository. The client folder should contain `xclient.exe`.

`speedtreert.dll` must be in the same folder as `xclient.exe`. The DLL itself is not tracked in this repo; if it is missing, copy only that DLL from the CBT client archive into the first client's folder. See `client_runtime/README.md` for the reference hash observed during repo preparation.

Start the client from a terminal so the language flag is applied:

```powershell
.\xclient.exe -english
```

Optional helpers are included for local client testing:

- `http_server/`: Node.js `/net_gsp.php` endpoint helper. Its README includes the Windows hosts-file entry for `www.arngamez.com`.
- `injector/`: x86 client hook/injector source. Its README includes Visual Studio build steps and runtime usage.

## Demo Accounts

The tracked `account_database.json` is a runnable demo seed using the same fields as the working server schema. Demo accounts start with `luna` and `cash` set to `999999`, plus `shared_item_stacks` entries for item IDs `1` through `20` with `owned_count: 99`. They still start with no owned/equipped character equipment and no owned skills, so shop purchases can populate those parts of the inventory during testing.

- `player` / `player`: named demo account with currency and shared stack items
- `newuser` / `newuser`: unnamed starter profile with currency and shared stack items

The server may update `account_database.json` while you test inventory, nickname, or shop flows. To reset the demo seed:

```powershell
git restore account_database.json
```

For private accounts, use a local file such as `account_database.local.json` and start the server with:

```powershell
build\Debug\tcp_lzss_server_cpp.exe --account-db-file account_database.local.json
```

`*.local.json` runtime files are ignored by Git.

## Local Data

The repo includes starter CSV data from:

- `data/setting/item_skill_v2.csv`: preferred skill/shop catalog
- `data/setting/item_skill.csv`: legacy skill/shop fallback
- `data/setting/game_itemlist.csv`: starter item id list
- `data/setting/item.csv`: item metadata, prices, equipment categories
- `data/setting/character.csv`: character names and asset keys
- `data/package_contents.csv`: package/bundle expansion table
- `data/missions/mission_0_6.csv`: included mission layout data

The repo does not currently include a full `data/setting/mission.csv` mission catalog. TCP room flows use `tcp_lzss_server_options.json`; experimental UDP mission sync may need a reviewed mission catalog for broader mission support.

Generated helper catalogs are still ignored:

- `data/item_id_catalog.json`
- `data/item_type_catalog.json`

## Repository Hygiene

Before pushing, check:

```powershell
git status --short
git ls-files
```

The tracked file list should not contain runtime logs, packet captures, decoded reports, generated catalogs, or `*.local.json` files.
