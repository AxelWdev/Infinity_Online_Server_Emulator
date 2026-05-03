# Infinity C++ Server Emulator

Research-oriented C++ server emulator for Infinity client protocol work.

This is not a complete private server and it does not contain official server code. The current goal is to emulate enough client-facing behavior for local protocol testing, login/lobby/shop flows, room setup, and ongoing gameplay research.

This public export contains the C++ server, starter runtime configuration, and CSV data for the current item, skill, package, and included mission-layout loaders. It does not include packet captures, runtime logs, decoded traffic reports, generated helper catalogs, or private local account data.

## Status

The emulator currently builds and runs with starter data, supports a set of known TCP client flows, includes local HTTP/injector helpers, and has experimental UDP gameplay-sync work behind an explicit flag. It should be treated as a protocol emulator and research base, not a production MMORPG backend.

See `ROADMAP.md` for the implemented feature list and remaining work.

## Build

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

On Windows, `build.bat Debug` or `build.bat Release` runs the same CMake flow.

## Run

The repo includes default starter files, so after building you can run:

```powershell
build\Debug\tcp_lzss_server_cpp.exe
```

Useful flags:

```powershell
build\Debug\tcp_lzss_server_cpp.exe --host 127.0.0.1 --port 8080 --game-udp-port 8081
```

On Windows, `run.bat Debug` builds and runs the server in one step. `run.bat` defaults to Release.

## Client Helpers

Optional helpers are included for local client testing:

- `http_server/`: Node.js `/net_gsp.php` endpoint helper. Its README includes the Windows hosts-file entry for `www.arngamez.com`.
- `injector/`: x86 client hook/injector source. Its README includes Visual Studio build steps and runtime usage.

## Client Setup

Use this client installer for normal setup:

- https://archive.org/details/infinity-online-client

Use this second archive only as a source for `speedtreert.dll` if the first client install is missing it:

- https://archive.org/details/cbt-infinity-20101011-manual

Install or unpack the client outside this repository. The client folder should contain `xclient.exe`.

`speedtreert.dll` must be in the same folder as `xclient.exe`. The DLL itself is not tracked in this repo; if it is missing, copy only that DLL from the CBT client archive into the first client's folder. See `client_runtime/README.md` for the reference hash observed during repo preparation.

Start the client from a terminal so the language flag is applied:

```powershell
.\xclient.exe -english
```

## Demo Accounts

The tracked `account_database.json` is a runnable demo seed using the same fields as the working server schema. Demo accounts start with no real owned/equipped shop items so item and skill purchases can populate inventory during testing. `shared_item_stacks` keeps a `{ "item_id": 0, "owned_count": 0 }` placeholder to show the expected object shape; `item_id: 0` is ignored by the server.

- `player` / `player`: named demo account with currency and no real owned items
- `newuser` / `newuser`: unnamed starter profile with currency and no real owned items

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
