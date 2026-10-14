@group(0) @binding(0) var<uniform> projection: mat4x4f;
@group(0) @binding(1) var atlasSampler: sampler;
@group(0) @binding(2) var atlas: texture_2d<f32>;
struct UiVertex {
    @builtin(position) position: vec4f,
    @location(0) texLocal: vec4f,
    @location(1) color: vec4f,
    @location(2) shape: vec4f,
    @location(3) corners: vec4f,
    @location(4) borderColor: vec4f,
};
@vertex fn vs_main(@location(0) position: vec4f, @location(1) texLocal: vec4f,
    @location(2) color: vec4f, @location(3) shape: vec4f,
    @location(4) corners: vec4f, @location(5) borderColor: vec4f) -> UiVertex {
    var output: UiVertex;
    output.position = projection * position;
    output.texLocal = texLocal; output.color = color; output.shape = shape;
    output.corners = corners; output.borderColor = borderColor;
    return output;
}
@fragment fn fs_texture(input: UiVertex) -> @location(0) vec4f {
    return textureSample(atlas, atlasSampler, input.texLocal.xy) * input.color;
}
@fragment fn fs_rounded(input: UiVertex) -> @location(0) vec4f {
    let p = input.texLocal.zw;
    let top = select(input.corners.x, input.corners.y, p.x > 0.0);
    let bottom = select(input.corners.z, input.corners.w, p.x > 0.0);
    let enabled = select(top, bottom, p.y > 0.0);
    let radius = min(input.shape.z, min(input.shape.x, input.shape.y)) * enabled;
    let q = abs(p) - input.shape.xy + vec2f(radius);
    let distance = length(max(q, vec2f(0.0))) + min(max(q.x, q.y), 0.0) - radius;
    let aa = max(fwidth(distance), 0.5);
    let coverage = 1.0 - smoothstep(-aa, aa, distance);
    let inner = select(1.0 - smoothstep(-aa, aa, distance + input.shape.w), 1.0, input.shape.w <= 0.0);
    let color = mix(input.borderColor, input.color, inner);
    return vec4f(color.rgb, color.a * coverage);
}
