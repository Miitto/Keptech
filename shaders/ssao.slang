import keptech;

struct v2f {
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
};

[shader("vertex")]
v2f vert(uint vid: SV_VertexID) {
  v2f output;

  float3 vertPos = kt::getFullscreenTriangleVertex(vid);

  output.position = float4(vertPos, 1.0);
  output.uv = vertPos.xy * 0.5 + 0.5;

  return output;
}

[shader("pixel")]
float frag(v2f v) : SV_Target {
  let noiseScale = float2(kt::camera.viewportSize / kt::constants::SSAO_NOISE_SIZE);

  let normalTex = kt::textures2D.getTexture(kt::TextureIndices::Normal);
  let depthTex = kt::textures2D.getTexture(kt::TextureIndices::Depth);
  let ssaoNoiseTex = kt::textures2D.getTexture(kt::TextureIndices::SsaoNoise);

  let normalSample = normalTex.Sample(v.uv).xyz;
  float depth = depthTex.Sample(v.uv).r;
  let noiseSample = ssaoNoiseTex.Sample(v.uv * noiseScale).xyz;

  float3 ndc = { v.uv * 2.0 - 1.0, depth };
  float4 invClip = kt::camera.clipToCamera(float4(ndc, 1.0));
  float3 view = invClip.xyz / invClip.w;

  let normal = normalize(normalSample * 2.0 - 1.0);

  let tangent = normalize(noiseSample - normal * dot(noiseSample, normal));
  let bitangent = cross(normal, tangent);
  float3x3 tbn = float3x3(tangent, bitangent, normal);

  float occlusion = 0.f;
  for (uint i = 0; i < kt::constants::SSAO_KERNEL_SIZE; i++) {
    let sampleVec = mul(kt::ssaoKernel[i], tbn);
    let samplePos = view + sampleVec * kt::constants::SSAO_RADIUS;

    float4 offsetNdc = kt::camera.cameraToClip(float4(samplePos, 1.0));
    float2 offsetScreen = (offsetNdc.xy / offsetNdc.w) * 0.5 + 0.5;

    float sampleDepth = depthTex.Sample(offsetScreen).r;
    float4 sampleNdc = float4(offsetScreen * 2.0 - 1.0, sampleDepth, 1.0);
    let invClip = kt::camera.clipToCamera(sampleNdc);
    let sampleView = invClip.xyz / invClip.w;

    float rangeCheck = smoothstep(0.f, 1.f, kt::constants::SSAO_RADIUS / abs(view.z - sampleView.z));
    static const float bias = 0.025;
    occlusion += (sampleView.z + bias <= samplePos.z ? 1.f : 0.f) * rangeCheck;
  }

  return pow(1.f - (occlusion / kt::constants::SSAO_KERNEL_SIZE), 2.f);
}
