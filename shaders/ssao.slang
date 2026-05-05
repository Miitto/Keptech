import keptech;

[[vk::binding(0, 1)]]
public ConstantBuffer<float3[64]> ssaoKernel;
[vk::binding(1, 1)]
uniform RWTexture2D<float> ssaoOutput : register(u0);

[shader("compute")]
[numthreads(32, 32, 1)]
void ssao(uint3 dispatchThreadID: SV_DispatchThreadID) {

  uint2 pixelCoord = dispatchThreadID.xy;

  int2 imageSize = int2(kt::camera.viewportSize);

  if (pixelCoord.x >= imageSize.x || pixelCoord.y >= imageSize.y) {
    return;
  }
  float2 uv = (float2(pixelCoord) + 0.5) / kt::camera.viewportSize;

  let noiseScale = float2(kt::camera.viewportSize / kt::constants::SSAO_NOISE_SIZE);

  let normalTex = kt::textures2D.getTexture(kt::TextureIndices::Normal);
  let depthTex = kt::textures2D.getTexture(kt::TextureIndices::Depth);
  let ssaoNoiseTex = kt::textures2D.getTexture(kt::TextureIndices::SsaoNoise);

  let normalSample = normalTex.Sample(uv).xyz;
  float depth = depthTex.Sample(uv).r;
  let noiseSample = ssaoNoiseTex.Sample(uv * noiseScale).xyz;

  float3 ndc = { uv * 2.0 - 1.0, depth };
  float4 invClip = kt::camera.clipToCamera(float4(ndc, 1.0));
  float3 view = invClip.xyz / invClip.w;

  let normal = normalize(normalSample * 2.0 - 1.0);

  let tangent = normalize(noiseSample - normal * dot(noiseSample, normal));
  let bitangent = cross(normal, tangent);
  float3x3 tbn = float3x3(tangent, bitangent, normal);

  float occlusion = 0.f;
  struct Sample {
    float3 samplePos;
    float2 position;
    float depth;
  };
  Sample samples[kt::constants::SSAO_KERNEL_SIZE];
  [ForceUnroll]
  for (uint i = 0; i < kt::constants::SSAO_KERNEL_SIZE; i++) {
    let sampleVec = mul(ssaoKernel[i], tbn);
    samples[i].samplePos = view + sampleVec * kt::constants::SSAO_RADIUS;

    float4 offsetNdc = kt::camera.cameraToClip(float4(samples[i].samplePos, 1.0));
    samples[i].position = (offsetNdc.xy / offsetNdc.w) * 0.5 + 0.5;

    samples[i].depth = depthTex.Sample(samples[i].position).r;
  }

  [ForceUnroll]
  for (uint i = 0; i < kt::constants::SSAO_KERNEL_SIZE; i++) {
    let samplePos = samples[i].samplePos;
    let offsetScreen = samples[i].position;
    let sampleDepth = samples[i].depth;
    float4 sampleNdc = float4(offsetScreen * 2.0 - 1.0, sampleDepth, 1.0);
    let invClip = kt::camera.clipToCamera(sampleNdc);
    let sampleView = invClip.xyz / invClip.w;

    float rangeCheck = smoothstep(0.f, 1.f, kt::constants::SSAO_RADIUS / abs(view.z - sampleView.z));
    static const float bias = 0.025;
    occlusion += (sampleView.z + bias <= samplePos.z ? 1.f : 0.f) * rangeCheck;
  }

  float result = pow(1.f - (occlusion / kt::constants::SSAO_KERNEL_SIZE), 2.f);

  ssaoOutput[pixelCoord] = result;
}
