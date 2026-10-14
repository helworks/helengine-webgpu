struct Constants {
    mvp: mat4x4f,
    normalMatrix: mat4x4f,
    color: vec4f,
    lightDirection: vec4f,
    lightColor: vec4f,
};
@group(0) @binding(0) var<uniform> constants: Constants;
struct VertexOutput { @builtin(position) position: vec4f, @location(0) normal: vec3f };
@vertex fn vs_main(@location(0) position: vec3f, @location(1) normal: vec3f) -> VertexOutput {
    var output: VertexOutput;
    output.position = constants.mvp * vec4f(position, 1.0);
    output.normal = (constants.normalMatrix * vec4f(normal, 0.0)).xyz;
    return output;
}
@fragment fn fs_main(input: VertexOutput) -> @location(0) vec4f {
    let diffuse = max(dot(normalize(input.normal), -constants.lightDirection.xyz), 0.0);
    let lighting = vec3f(0.15) + constants.lightColor.rgb * diffuse;
    let litColor = select(constants.color.rgb, constants.color.rgb * lighting, constants.lightDirection.w > 0.5);
    return vec4f(litColor, constants.color.a);
}
