# Client Runtime Files

The public repository does not track proprietary client binaries or third-party runtime DLLs.

Install or unpack the client from:

```text
https://archive.org/details/infinity-online-client
https://archive.org/details/cbt-infinity-20101011-manual
```

Place `speedtreert.dll` in the same folder as `xclient.exe`. The CBT client archive is a known source for this DLL.

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
