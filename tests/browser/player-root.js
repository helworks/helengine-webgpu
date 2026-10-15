const fs = require('node:fs');
const path = require('node:path');

/** Chooses one explicit export; configured invalid paths must fail before the server starts. */
function resolvePlayerRoot(configured) {
  if (configured === undefined) return 'C:/dev/helworks/builds/helengine-webgpu/cube-test/player';
  if (!configured || !path.isAbsolute(configured)) throw new Error('HELENGINE_WEBGPU_TEST_PLAYER_ROOT must be an absolute path.');
  const root = path.resolve(configured);
  for (const name of ['index.html', 'helengine_webgpu.js', 'helengine_webgpu.wasm', 'helengine_webgpu.data']) {
    const file = path.join(root, name);
    if (!fs.existsSync(file) || !fs.statSync(file).isFile()) throw new Error(`Configured player package is missing ${file}`);
    if (!fs.statSync(file).size) throw new Error(`Configured player package has an empty artifact: ${file}`);
  }
  return root;
}
module.exports = { resolvePlayerRoot };
