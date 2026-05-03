# Infinity Pack Tools

Small C/zlib tools for inspecting and rebuilding Infinity client `raw1` `.bin` pack archives.

These tools are included as source code only. The public repository does not include original client pack archives, repacked archives, unpacked proprietary client files, or generated patch outputs.

## Files

| File | Purpose |
| --- | --- |
| `unpack.c` | Lists or extracts files from an Infinity `.bin` pack archive. |
| `repack.c` | Rebuilds a `.bin` pack from a template archive and an extracted working directory. |
| `compile.bat` | Windows/MSYS2 MinGW64 build helper for both tools. |

## Requirements

On Windows, the included build script expects MSYS2 MinGW64 with GCC and zlib:

```powershell
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-zlib
```

The default script path is:

```text
C:\msys64\mingw64\bin\gcc.exe
```

If MSYS2 is installed somewhere else, edit `compile.bat` and change the `MINGW` variable.

## Build

From this directory:

```powershell
.\compile.bat
```

Expected outputs:

```text
unpack.exe
repack.exe
```

Manual equivalent:

```powershell
C:\msys64\mingw64\bin\gcc.exe -O2 -std=c11 -Wall -Wextra -o unpack.exe unpack.c -lz
C:\msys64\mingw64\bin\gcc.exe -O2 -std=c11 -Wall -Wextra -o repack.exe repack.c -lz
```

## Unpack Usage

List archive contents:

```powershell
.\unpack.exe path\to\infinity.0.00.02.bin --list
```

Extract archive contents:

```powershell
.\unpack.exe path\to\infinity.0.00.02.bin extracted
```

Verbose extraction:

```powershell
.\unpack.exe path\to\infinity.0.00.02.bin -v extracted
```

## Repack Usage

Rebuild a full archive using a template pack and an extracted working directory:

```powershell
.\repack.exe path\to\template.bin extracted output\infinity.0.00.02.bin
```

Build a minimal overlay archive containing explicit relative paths:

```powershell
.\repack.exe --overlay path\to\template.bin extracted output\overlay.bin setting\item.csv
```

Append explicit files to the template archive:

```powershell
.\repack.exe --append path\to\template.bin extracted output\patched.bin setting\item.csv text\scriptmsg_kor.csv
```

Useful options:

```text
-v                 verbose logging
--hash-name <name> override SECONDARY header hash basename
--level <0-9>      zlib compression level for rebuilt streams
```

## Encoding Note

Some client CSV files are UTF-16LE with BOM. For example, `setting/item.csv` in the known client pack uses UTF-16LE. Preserve the original encoding when editing extracted files before repacking; saving those files as UTF-8 can make the client misread shop or item rows.

## Repository Hygiene

Local outputs from these tools are ignored by Git:

- `unpack.exe`
- `repack.exe`
- `*.bin`
- `unpacked/`
- `extracted/`
- `repacked/`
- `output/`

Keep original client archives and extracted proprietary files outside commits.
