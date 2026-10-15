const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');
const { resolvePlayerRoot } = require('./player-root');
const playerRoot = resolvePlayerRoot(process.env.HELENGINE_WEBGPU_TEST_PLAYER_ROOT);
const root = 'C:/dev/helworks/builds/helengine-webgpu/device-tests';
const shaderRoot = 'C:/dev/helworks/builds/helengine-webgpu/shared-shaders/shader-tests';
http.createServer((request, response) => {
  let relative = decodeURIComponent(new URL(request.url, 'http://localhost').pathname);
  const selectedRoot = relative.startsWith('/game/') ? playerRoot : relative.startsWith('/shaders/') ? shaderRoot : root;
  if (relative.startsWith('/game/')) relative = relative.slice(5);
  if (relative.startsWith('/shaders/')) relative = relative.slice(8);
  const file = path.resolve(selectedRoot, '.' + (relative === '/' ? '/index.html' : relative));
  if (!file.startsWith(path.resolve(selectedRoot) + path.sep)) { response.writeHead(403).end(); return; }
  fs.readFile(file, (error, data) => {
    if (error) { response.writeHead(404, { 'Content-Type': 'text/html' }).end('<p>Player artifact not built yet.</p>'); return; }
    response.setHeader('Content-Type', file.endsWith('.wasm') ? 'application/wasm' : file.endsWith('.js') ? 'application/javascript' : 'text/html');
    response.end(data);
  });
}).listen(8197, '127.0.0.1');
