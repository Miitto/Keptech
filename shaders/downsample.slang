import keptech;

struct PushConstants {
  float2 texelSize;
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

  float x = pc.texelSize.x;
  float y = pc.texelSize.y;
  float x2 = x * 2.0;
  float y2 = y * 2.0;

  //
  // Take 13 samples around current texel:
  // a - b - c
  // - j - k -
  // d - e - f
  // - l - m -
  // g - h - i
  // === ('e' is the current texel) ===
  let a = sourceTex.Sample(uv + float2(-x2, y2)).rgb;
  let b = sourceTex.Sample(uv + float2(0, y2)).rgb;
  let c = sourceTex.Sample(uv + float2(x2, y2)).rgb;
  let d = sourceTex.Sample(uv + float2(-x2, 0)).rgb;
  let e = sourceTex.Sample(uv).rgb;
  let f = sourceTex.Sample(uv + float2(x2, 0)).rgb;
  let g = sourceTex.Sample(uv + float2(-x2, -y2)).rgb;
  let h = sourceTex.Sample(uv + float2(0, -y2)).rgb;
  let i = sourceTex.Sample(uv + float2(x2, -y2)).rgb;
  let j = sourceTex.Sample(uv + float2(-x, y)).rgb;
  let k = sourceTex.Sample(uv + float2(x, y)).rgb;
  let l = sourceTex.Sample(uv + float2(-x, -y)).rgb;
  let m = sourceTex.Sample(uv + float2(x, -y)).rgb;

  var downsample = e * 0.125;
  downsample += (a + c + g + i) * 0.03125;
  downsample += (b + d + f + h) * 0.0625;
  downsample += (j + k + l + m) * 0.125;
  return float4(downsample, 1.0);
}
