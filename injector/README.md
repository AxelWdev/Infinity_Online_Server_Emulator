# Client Hook Injector

Source for the local research injector used with the 32-bit `xclient.exe`.

It builds two local binaries:

- `hook.dll`: patches the known client stream cipher instructions and logs UDP `sendto` / `recvfrom` traffic.
- `injector.exe`: finds `xclient.exe` and loads `hook.dll` into that process.

Generated binaries and UDP logs are ignored by Git.

## Requirements

- Windows
- Visual Studio C++ Build Tools with x86 tools installed
- The target client must be the 32-bit `xclient.exe` build that matches the patch offsets in `hook.cpp`

## Build

From this folder:

```bat
build.bat
```

The script auto-detects Visual Studio with `vswhere` when available. If `cl.exe` is not found, open an **x86 Native Tools Command Prompt for VS** and run `build.bat` again.

Expected local outputs:

```text
hook.dll
injector.exe
```

## Run

Start `xclient.exe` first, then run:

```bat
injector.exe
```

By default, `injector.exe` loads `hook.dll` from the same folder. To load a different hook DLL:

```bat
injector.exe path\to\hook.dll
```

The hook writes UDP traffic to:

```text
udp_hook_traffic.log
```

next to `xclient.exe`.

## Notes

Use this only with your local research client. The hook is version-sensitive: if `xclient.exe` changes, the hardcoded patch offsets in `hook.cpp` must be reviewed before use.
