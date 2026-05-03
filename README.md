# Infinity C++ Server

Standalone C++ server implementation for the Infinity client protocol work.

This public export intentionally contains only source code, build files, helper scripts, and sanitized examples. It does not include private account databases, packet captures, generated catalogs, logs, decoded traffic reports, or extracted game data.

## Build

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

## Run

Create local runtime files from the examples or pass explicit paths:

```powershell
build\Debug\tcp_lzss_server_cpp.exe --account-db-file account_database.json --options-file tcp_lzss_server_options.json
```

The server can also run without those files, but no accounts or configured packet lists will be available.

## Local Data

The code can read optional local data from:

- `data/setting/item_skill_v2.csv`
- `data/setting/item_skill.csv`
- `data/setting/game_itemlist.csv`
- `data/setting/item.csv`
- `data/setting/character.csv`
- `data/package_contents.csv`
- `data/missions/*.csv`

Those files are intentionally ignored because they may contain extracted or generated project data. Keep them local unless you have confirmed they are safe to publish.

## Repository Hygiene

Before pushing, check:

```powershell
git status --short
git ls-files
```

The tracked file list should not contain runtime logs, account databases, packet captures, generated catalogs, or extracted game data.
