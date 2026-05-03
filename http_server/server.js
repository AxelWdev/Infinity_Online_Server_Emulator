const http = require('http');

const socketIp = '127.0.0.1';
const socketPort = 8080;

const server = http.createServer((req, res) => {
	if (req.url === '/net_gsp.php') {
		console.log('[HTTP] GET /net_gsp.php from', req.socket.remoteAddress);

		const responseText =
			`${socketIp} ${socketPort}\n` +
			`127.0.0.1 8081\n` +
			`127.0.0.1 8082\n`;

		console.log('[HTTP] Response:\n' + responseText);

		res.writeHead(200, {
			'Content-Type': 'text/plain; charset=utf-8',
			'Content-Length': Buffer.byteLength(responseText),
			'Connection': 'close'
		});
		res.end(responseText);
	} else {
		console.log('[HTTP] Unknown request:', req.url);
		res.writeHead(404, { 'Content-Type': 'text/plain' });
		res.end('Not Found\n');
	}
});

server.listen(80, () => {
	console.log('[HTTP] Server running on port 80');
});
