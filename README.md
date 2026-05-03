# Infinity C++ Server

Standalone C++ server implementation for the Infinity client protocol work.

This public export contains the C++ server, starter runtime configuration, and the CSV data needed by the current item/skill/mission loaders. It does not include packet captures, runtime logs, decoded traffic reports, generated catalogs, or private local account data.

## Build

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

## Run

The repo includes default starter files, so after building you can run:

```powershell
build\Debug\tcp_lzss_server_cpp.exe
```

Useful flags:

```powershell
build\Debug\tcp_lzss_server_cpp.exe --host 127.0.0.1 --port 8080 --game-udp-port 8081
```

## Demo Accounts

The tracked `account_database.json` is a runnable demo seed using the same fields as the working server schema. Demo accounts start with no owned/equipped shop items so item and skill purchases can populate inventory during testing. `shared_item_stacks` keeps a `{ "item_id": 0, "owned_count": 0 }` placeholder to show the expected object shape; `item_id: 0` is ignored by the server.

- `player` / `player`: named demo account with currency and empty inventory
- `newuser` / `newuser`: unnamed starter profile with currency and empty inventory

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

- `data/setting/item_skill_v2.csv`
- `data/setting/item_skill.csv`
- `data/setting/game_itemlist.csv`
- `data/setting/item.csv`
- `data/setting/character.csv`
- `data/missions/*.csv`

Generated catalogs and private exports are still ignored:

- `data/item_id_catalog.json`
- `data/item_type_catalog.json`
- `data/package_contents.csv`

## Repository Hygiene

Before pushing, check:

```powershell
git status --short
git ls-files
```

The tracked file list should not contain runtime logs, packet captures, decoded reports, generated catalogs, or `*.local.json` files.
