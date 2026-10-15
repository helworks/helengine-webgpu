const { createHash } = require('node:crypto');

/** Changes a valid WGSP vertex program while maintaining its byte envelope and stage hash. */
function invalidateShader(bytes) {
  const start = bytes.indexOf(Buffer.from('WGSP'));
  if (start < 0) throw new Error('The selected package contains no cooked WGSP shader.');
  const metadataLength = bytes.readUInt32LE(start + 16);
  const vertexLength = bytes.readUInt32LE(start + 20);
  const metadataStart = start + 28;
  const metadata = JSON.parse(bytes.subarray(metadataStart, metadataStart + metadataLength));
  const vertexStart = metadataStart + metadataLength;
  bytes[vertexStart] = '!'.charCodeAt(0);
  const hash = createHash('sha256').update(bytes.subarray(vertexStart, vertexStart + vertexLength)).digest('hex');
  const oldHash = Buffer.from(metadata.vertexWgslSha256);
  const offset = bytes.subarray(metadataStart, metadataStart + metadataLength).indexOf(oldHash);
  if (offset < 0) throw new Error('WGSP stage fingerprint is absent.');
  Buffer.from(hash).copy(bytes, metadataStart + offset);
  return bytes;
}
module.exports = { invalidateShader };
