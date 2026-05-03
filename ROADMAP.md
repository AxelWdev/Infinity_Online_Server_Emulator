# Roadmap

This project is a server emulator for Infinity client protocol research. It is not a complete private server, not a production MMORPG backend, and not a replacement for the original official server.

The roadmap below describes the public repository status. Items marked as implemented are present in the source tree, but some protocol fields are still named conservatively when their purpose is not fully confirmed.

## Implemented

- C++20/CMake build for the main TCP server and UDP analysis tools.
- TCP LZSS transport, logical packet framing, packet logging, and opcode dispatch.
- Starter configuration files that allow the server to run after cloning and building.
- JSON account database loading/saving with demo accounts and a local-file override option.
- Starter item, skill, character, package, and included mission-layout CSV data.
- Game data catalog loading for item/shop/skill/package behavior.
- Basic account and lobby flow support, including login challenge, lobby connect, channel enumeration, account info, character list, guard list, item list, skill list, quickslot list, and keepalive/no-response handling for observed packets.
- Basic inventory operations, including item purchase, skill purchase, currency checks, package expansion, item removal, equipment assignment, and quickslot assignment for supported item kinds.
- TCP room setup flows, including room list, room create completion, room info, room state, character select state, room start handling, and room leave handling.
- Local room registry/runtime room state for emulator-side TCP room behavior.
- Experimental UDP game server path with native-order sync work guarded by `--experimental-game-udp-sync`.
- UDP traffic logging and hook-log decode tools for protocol investigation.
- Local HTTP helper for the client's `/net_gsp.php` endpoint.
- Injector source and build instructions for local client testing.
- Public-repo hygiene rules that exclude private account data, logs, captures, generated catalogs, client binaries, and third-party DLLs.

## Remaining Work

- Complete the packet map and document all confirmed client/server packet fields.
- Replace remaining unknown packet fields with confirmed names only after protocol evidence is strong enough.
- Add broader automated tests for packet encoding/decoding, account persistence, inventory rules, room flows, and data loading.
- Add CI builds for supported Windows toolchains.
- Implement production-grade account storage if the emulator ever needs long-running public testing: password hashing, migrations, backups, admin tooling, and safer account-reset flows.
- Expand channel, lobby, matchmaking, room ownership, and multi-client synchronization beyond the current local emulator behavior.
- Complete mission catalog coverage. The repo does not currently include a full reviewed `data/setting/mission.csv`.
- Complete authoritative gameplay simulation: movement validation, combat rules, damage, deaths, objectives, mission completion, rewards, drops, and progression.
- Finish and verify UDP gameplay synchronization for real mission play. Current UDP work is experimental and should be validated with captures before being treated as stable.
- Implement complete multiplayer state replication across TCP and UDP, including late join, reconnect, disconnect, ready state, loading state, and host migration behavior if the client expects it.
- Fill out shop/catalog edge cases: expiration, limited items, cash/event-cash rules, character restrictions, equipment categories, and unsupported quickslot item kinds.
- Improve observability for emulator users: structured logs, per-session traces, packet summaries, and safer debug toggles.
- Document known client versions and compatibility differences.
- Keep proprietary client files and third-party DLLs out of the repository; document external installer sources instead.

## Suggested Milestones

- M1: Make the TCP lobby/shop/inventory path repeatable with tests.
- M2: Stabilize TCP room creation, entry, start, leave, and multi-client room state.
- M3: Confirm the UDP mission-entry sequence and document packet fields from capture evidence.
- M4: Implement a minimal authoritative mission loop for one training mission.
- M5: Add persistent progression, rewards, and broader data coverage.
- M6: Harden tooling, logs, setup docs, and CI so new contributors can reproduce the emulator state without private files.
