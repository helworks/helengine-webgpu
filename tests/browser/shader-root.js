const fs = require('node:fs');
const path = require('node:path');

/** Selects the requested GPU fixtures without accepting stale or incomplete explicit roots. */
function resolveShaderRoot(configured) {
  if (configured === undefined) return 'C:/dev/helworks/builds/helengine-webgpu/shared-shaders/shader-tests';
  if (!configured || !path.isAbsolute(configured)) throw new Error('HELENGINE_WEBGPU_TEST_SHADER_ROOT must be an absolute path.');
  const root = path.resolve(configured);
  for (const name of ['index.html', 'shared.js', 'shared.wasm', 'shared.data', 'shadows.html', 'shadows.js', 'shadows.wasm', 'shadows.data']) {
    const file = path.join(root, name);
    if (!fs.existsSync(file) || !fs.statSync(file).isFile()) throw new Error(`Configured shader fixtures are missing ${file}`);
    if (!fs.statSync(file).size) throw new Error(`Configured shader fixture is empty: ${file}`);
  }
  return root;
}
module.exports = { resolveShaderRoot };
