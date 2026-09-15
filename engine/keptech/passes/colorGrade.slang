#language slang 2026

import keptech;

[shader("vertex")]
[cull(kt::CullMode::None)]
kt::PosUv vert(uint vid: SV_VertexID) { return kt::fullscreenTri(vid); }

ConstantBuffer<kt::Camera> camera;

Texture2D<float4> srcTex;

[shader("pixel")]
float4 frag(kt::PosUv input) : SV_Target {
  let sampler = kt::samplers::linearClamp;

  let color = srcTex.SampleLevel(sampler, input.uv, 0);

  let tonemapped = color.rgb / (color.rgb + float3(1.0));

  let gammaCorrected = linearToSrgb(tonemapped);

  let luma = luminance(gammaCorrected);

  return float4(gammaCorrected, luma);
}
