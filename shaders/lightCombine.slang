import keptech;

struct v2f {
  float4 position : SV_Position;
  float2 uv;
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
float4 frag(v2f input) {
  float2 uv = input.uv;

  let albedoTex = kt::getTarget(kt::Target::Albedo);
  let emissiveAoTex = kt::getTarget(kt::Target::EmissiveAo);
  let diffuseTex = kt::getTarget(kt::Target::Diffuse);
  let specularTex = kt::getTarget(kt::Target::Specular);
  let ssaoTex = kt::getTarget(kt::Target::SsaoBlur);

  float3 albedo = albedoTex.Sample(uv).rgb;
  float4 emissiveAo = emissiveAoTex.Sample(uv);
  float3 diffuseLight = diffuseTex.Sample(uv).rgb;
  float3 specularLight = specularTex.Sample(uv).rgb;
  float occlusion = ssaoTex.Sample(uv).r;

  static const float3 ambientLight = float3(0.1, 0.1, 0.1);

  float combinedAo = occlusion * emissiveAo.a;

  float3 ambient = albedo * ambientLight;

  float3 color = diffuseLight + specularLight;

  color = ambient + color;

  color *= combinedAo;

  color += emissiveAo.rgb;

  return float4(color, 1.0);
}
