# Client Runtime Files

The public repository does not track proprietary client binaries or third-party runtime DLLs.

Install or unpack the main client from:

```text
https://archive.org/details/infinity-online-client
```

Only use the CBT archive below to extract `speedtreert.dll` if the main client folder is missing it:

```text
https://archive.org/details/cbt-infinity-20101011-manual
```

Place `speedtreert.dll` in the same folder as `xclient.exe`. Do not replace the main client with the CBT client for this setup; copy only the DLL from the CBT archive when needed.

Local reference copy observed during repo preparation:

```text
filename: speedtreert.dll
sha256: 9FDA2FD3D8F91669F83AFCF8776C93171CF5B5C1FAD09D1576D01A2AF8755EB7
size: 221184 bytes
```

Run the client from that folder:

```powershell
.\xclient.exe -english
```
