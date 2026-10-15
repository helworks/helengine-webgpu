const {test}=require('node:test');
const assert=require('node:assert/strict');
const {createHash}=require('node:crypto');
const {invalidateShader}=require('./cooked-shader-corruption');

function artifact(shaderAssetId,variant) {
  const vertex=Buffer.from('valid vertex source');
  const metadata=Buffer.from(JSON.stringify({shaderAssetId,variant,vertexWgslSha256:createHash('sha256').update(vertex).digest('hex')}));
  const header=Buffer.alloc(28);header.write('WGSP');header.writeUInt32LE(metadata.length,16);header.writeUInt32LE(vertex.length,20);
  return Buffer.concat([header,metadata,vertex]);
}

test('corruption targets the required startup UI shader even when other scenes have earlier artifacts',()=>{
  const unused=artifact('UnusedSceneShader','default');
  const required=artifact('SpriteBatchShader','Textured');
  const bytes=Buffer.concat([unused,required]);
  invalidateShader(bytes);
  assert.deepEqual(bytes.subarray(0,unused.length),unused);
  assert.notDeepEqual(bytes.subarray(unused.length),required);
  const metadataLength=bytes.readUInt32LE(unused.length+16);
  const vertexStart=unused.length+28+metadataLength;
  const metadata=JSON.parse(bytes.subarray(unused.length+28,vertexStart));
  assert.equal(metadata.vertexWgslSha256,createHash('sha256').update(bytes.subarray(vertexStart)).digest('hex'));
});

test('corruption rejects a package without the required UI selection',()=>{
  assert.throws(()=>invalidateShader(artifact('UnusedSceneShader','default')),/required.*UI/i);
});
