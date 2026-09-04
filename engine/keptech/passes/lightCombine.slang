import keptech;

struct v2f {
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
};

[shader("vertex")]
[cull(kt::CullMode::None)]
v2f vert(uint vid: SV_VertexID) {
  float2 coord = float2((vid << 1) & 2, vid & 2);
  v2f output = { float4(float2(-1.0, 1.0) + coord * float2(2.0, -2.0), 0.0, 1.0), coord };
  return output;
}

Texture2D<float4> albedoTex;
Texture2D<float4> emissiveTex;
Texture2D<float4> lightingTex;

[shader("pixel")]
float4 frag(v2f input) : SV_Target {
  float2 uv = input.uv;

  let sampler = kt::samplers::linearClamp;

  float3 albedo = albedoTex.Sample(sampler, uv).rgb;
  float3 emissive = emissiveTex.Sample(sampler, uv).rgb;
  float3 lighting = lightingTex.Sample(sampler, uv).rgb;

  return float4((albedo * lighting) + emissive, 1.0);
}
