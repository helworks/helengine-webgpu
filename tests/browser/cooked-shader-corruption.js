const { createHash } = require('node:crypto');

/** Corrupts the required startup UI program while preserving its envelope and stage hash. */
function invalidateShader(bytes) {
  let start = bytes.indexOf(Buffer.from('WGSP'));
  while(start>=0) {
  const metadataLength = bytes.readUInt32LE(start + 16);
  const vertexLength = bytes.readUInt32LE(start + 20);
  const fragmentLength = bytes.readUInt32LE(start + 24);
  const metadataStart = start + 28;
  const metadata = JSON.parse(bytes.subarray(metadataStart, metadataStart + metadataLength));
  if(metadata.shaderAssetId!=='SpriteBatchShader' || metadata.variant!=='Textured') {
    start=bytes.indexOf(Buffer.from('WGSP'),metadataStart+metadataLength+vertexLength+fragmentLength);
    continue;
  }
  const vertexStart = metadataStart + metadataLength;
  bytes[vertexStart] = '!'.charCodeAt(0);
  const hash = createHash('sha256').update(bytes.subarray(vertexStart, vertexStart + vertexLength)).digest('hex');
  const oldHash = Buffer.from(metadata.vertexWgslSha256);
  const offset = bytes.subarray(metadataStart, metadataStart + metadataLength).indexOf(oldHash);
  if (offset < 0) throw new Error('WGSP stage fingerprint is absent.');
  Buffer.from(hash).copy(bytes, metadataStart + offset);
  return bytes;
  }
  throw new Error('The selected package contains no required startup UI shader.');
}
module.exports = { invalidateShader };
