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
Texture2D<float4> diffuseTex;
Texture2D<float4> specularTex;

[shader("pixel")]
float4 frag(v2f input) : SV_Target {
  float2 uv = input.uv;

  let sampler = kt::samplers::linearClamp;

  float3 albedo = albedoTex.Sample(sampler, uv).rgb;
  float4 emissive = emissiveTex.Sample(sampler, uv);
  float3 diffuseLight = diffuseTex.Sample(sampler, uv).rgb;
  float3 specularLight = specularTex.Sample(sampler, uv).rgb;

  static const float3 ambientLight = float3(0.1, 0.1, 0.1);

  float combinedAo = 1.f; // TODO: This is where we would add in SSAO.

  float3 ambient = albedo * ambientLight;

  float3 color = diffuseLight + specularLight;

  color = ambient + color;

  color *= combinedAo;

  color += emissive.rgb;

  return float4(color, 1.0);
}
