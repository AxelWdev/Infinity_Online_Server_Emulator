# Infinity HTTP Endpoint Server

Small Node.js helper server for the legacy client bootstrap endpoint.

The 2005 client contains this endpoint:

```text
http://www.arngamez.com/net_gsp.php
```

The helper listens on port `80` and answers `/net_gsp.php` with local TCP/UDP endpoints for the public C++ server.

## Hosts File

Edit the Windows hosts file as Administrator:

```text
C:\Windows\System32\drivers\etc\hosts
```

Add this line:

```text
127.0.0.1 www.arngamez.com
```

Then flush DNS:

```powershell
ipconfig /flushdns
```

## Run

Install Node.js, then start the HTTP server from this folder:

```powershell
node server.js
```

Port `80` usually requires an elevated terminal on Windows. If another service is using port `80`, stop it before starting this helper.

## Verify

```powershell
curl http://www.arngamez.com/net_gsp.php
```

Expected response:

```text
127.0.0.1 8080
127.0.0.1 8081
127.0.0.1 8082
```

Run the C++ server with matching ports:

```powershell
..\build\Debug\tcp_lzss_server_cpp.exe --host 127.0.0.1 --port 8080 --game-udp-port 8081
```
