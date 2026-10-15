const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { randomUUID } = require('node:crypto');
const { resolvePlayerRoot } = require('./player-root');

test('configured player roots never silently fall back to an old package', () => {
  assert.throws(() => resolvePlayerRoot('relative/player'), /absolute/i);
  assert.throws(() => resolvePlayerRoot(''), /absolute/i);
  assert.throws(() => resolvePlayerRoot('C:/dev/helworks/builds/helengine-webgpu/nonexistent-' + randomUUID()), /missing/i);
});
test('configured roots require all four nonempty exported package artifacts', () => {
  const root = path.resolve('C:/dev/helworks/builds/helengine-webgpu/shared-shaders/server-tests/' + randomUUID());
  fs.mkdirSync(root, { recursive: true });
  for (const file of ['index.html', 'helengine_webgpu.js', 'helengine_webgpu.wasm']) fs.writeFileSync(path.join(root, file), 'fixture');
  assert.throws(() => resolvePlayerRoot(root), /missing/i);
  fs.writeFileSync(path.join(root, 'helengine_webgpu.data'), '');
  assert.throws(() => resolvePlayerRoot(root), /empty/i);
  fs.writeFileSync(path.join(root, 'helengine_webgpu.data'), 'fixture');
  assert.equal(resolvePlayerRoot(root), root);
});
