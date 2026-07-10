import keptech;

struct PushConstants {
  float filterRadius;
  uint sourceIndex;
};

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
float4 frag(v2f input, uniform PushConstants pc) : SV_Target {
  float2 uv = input.uv;

  let sourceTex = kt::textures2D.getTexture(pc.sourceIndex);

  float x = pc.filterRadius;
  float y = pc.filterRadius;

  // Take 9 samples around current texel:
  // a - b - c
  // d - e - f
  // g - h - i
  // === ('e' is the current texel) ===
  let a = sourceTex.Sample(uv + float2(-x, y)).rgb;
  let b = sourceTex.Sample(uv + float2(0, y)).rgb;
  let c = sourceTex.Sample(uv + float2(x, y)).rgb;
  let d = sourceTex.Sample(uv + float2(-x, 0)).rgb;
  let e = sourceTex.Sample(uv).rgb;
  let f = sourceTex.Sample(uv + float2(x, 0)).rgb;
  let g = sourceTex.Sample(uv + float2(-x, -y)).rgb;
  let h = sourceTex.Sample(uv + float2(0, -y)).rgb;
  let i = sourceTex.Sample(uv + float2(x, -y)).rgb;

  var upsample = e * 4.f;
  upsample += (b + d + f + h) * 2.f;
  upsample += a + c + g + i;
  upsample *= 1.f / 16.f;

  return float4(upsample, 1.0);
}
