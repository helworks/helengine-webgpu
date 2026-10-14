const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');
const root = 'C:/dev/helworks/builds/helengine-webgpu/device-tests';
http.createServer((request, response) => {
  const relative = decodeURIComponent(new URL(request.url, 'http://localhost').pathname);
  const file = path.resolve(root, '.' + (relative === '/' ? '/index.html' : relative));
  if (!file.startsWith(path.resolve(root) + path.sep)) { response.writeHead(403).end(); return; }
  fs.readFile(file, (error, data) => {
    if (error) { response.writeHead(404).end(); return; }
    response.setHeader('Content-Type', file.endsWith('.wasm') ? 'application/wasm' : file.endsWith('.js') ? 'application/javascript' : 'text/html');
    response.end(data);
  });
}).listen(8197, '127.0.0.1');
