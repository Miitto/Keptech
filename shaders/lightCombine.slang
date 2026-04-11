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

  let albedoTex = kt::textures.getTexture(kt::constants::albedoIndex);
  let emissiveAoTex = kt::textures.getTexture(kt::constants::emissiveAoIndex);
  let diffuseTex = kt::textures.getTexture(kt::constants::diffuseIndex);
  let specularTex = kt::textures.getTexture(kt::constants::specularIndex);

  float3 albedo = albedoTex.Sample(uv).rgb;
  float4 emissiveAo = emissiveAoTex.Sample(uv);
  float3 diffuseLight = diffuseTex.Sample(uv).rgb;
  float3 specularLight = specularTex.Sample(uv).rgb;

  static const float3 ambientLight = float3(0.1, 0.1, 0.1);

  float3 color = diffuseLight + specularLight;

  color *= emissiveAo.a;

  color += albedo * ambientLight;

  color += emissiveAo.rgb;

  // Tone map
  // TODO: Move to post process
  color = color / (color + float3(1.0));

  return float4(color, 1.0);
}
