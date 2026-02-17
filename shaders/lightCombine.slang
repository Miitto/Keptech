import keptech;

struct v2f {
  float4 position : SV_Position;
  float2 uv;
};

[shader("vertex")]
v2f vert(uint vid: SV_VertexID) {
  v2f output;

  float3 vertPos = keptech::getFullscreenTriangleVertex(vid);

  output.position = float4(vertPos, 1.0);
  output.uv = vertPos.xy * 0.5 + 0.5;

  return output;
}

struct GBufferIndices {
  uint albedoIndex;
  uint normalIndex;
  uint depthIndex;
};

struct LightBufferIndices {
  uint diffuseIndex;
  uint specularIndex;
};

[shader("pixel")]
float4 frag(v2f input, uniform GBufferIndices gBufferIndices,
            uniform LightBufferIndices lightBufferIndices) {
  float2 uv = input.uv;

  let albedoTex = keptech::textures.getTexture(gBufferIndices.albedoIndex);
  let diffuseTex =
      keptech::textures.getTexture(lightBufferIndices.diffuseIndex);
  let specularTex =
      keptech::textures.getTexture(lightBufferIndices.specularIndex);

  float3 albedo = albedoTex.Sample(uv).rgb;
  float3 diffuseLight = diffuseTex.Sample(uv).rgb;
  float3 specularLight = specularTex.Sample(uv).rgb;

  static const float3 ambientLight = float3(0.1, 0.1, 0.1);

  float3 color = albedo * ambientLight;
  color += albedo * diffuseLight;
  color += specularLight;

  return float4(color, 1.0);
}
