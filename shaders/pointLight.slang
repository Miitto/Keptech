import keptech;

static const float3 CUBE_VERTS[8] = {
  { -1.f, -1.f, -1.f }, { 1.f, -1.f, -1.f }, { 1.f, 1.f, -1.f },
  { -1.f, 1.f, -1.f },  { -1.f, -1.f, 1.f }, { 1.f, -1.f, 1.f },
  { 1.f, 1.f, 1.f },    { -1.f, 1.f, 1.f },
};
static const uint CUBE_INDICES[36] = {
  0, 1, 2, 0, 2, 3, 1, 5, 6, 1, 6, 2, 5, 4, 7, 5, 7, 6,
  4, 0, 3, 4, 3, 7, 3, 2, 6, 3, 6, 7, 4, 5, 1, 4, 1, 0,
};

struct GBufferIndices {
  uint albedoIndex;
  uint normalIndex;
  uint emissiveAoIndex;
  uint metallicRoughnessIndex;
  uint depthIndex;
};

struct v2f {
  float4 position : SV_Position;
};

[shader("vertex")]
v2f vert(uint vid: SV_VertexID, uniform keptech::PointLight light) {
  v2f output;

  float3 vertPos = CUBE_VERTS[CUBE_INDICES[vid]];
  float3 scaledPos = vertPos * light.radius;

  float3 lightPos = light.position;

  float4 worldPos = float4(scaledPos + lightPos, 1.0);

  output.position = keptech::camera.worldToClip(worldPos);

  return output;
}

[shader("pixel")]
keptech::DeferredLightingOutput frag(v2f input,
                                     uniform keptech::PointLight light,
                                     uniform GBufferIndices gBufferIndices,
                                     float4 fragCoord: SV_Position) {
  keptech::DeferredLightingOutput output;

  float2 uv = fragCoord.xy / keptech::camera.viewportSize;

  let albedoTex = keptech::textures.getTexture(gBufferIndices.albedoIndex);
  let metallicRoughnessTex =
      keptech::textures.getTexture(gBufferIndices.metallicRoughnessIndex);
  let normalTex = keptech::textures.getTexture(gBufferIndices.normalIndex);
  let depthTex = keptech::textures.getTexture(gBufferIndices.depthIndex);

  let albedoSample = albedoTex.Sample(uv);
  let metallicRoughnessSample = metallicRoughnessTex.Sample(uv);
  let normalSample = normalTex.Sample(uv);
  float depth = depthTex.Sample(uv).r;

  float3 ndc = { uv * 2.0 - 1.0, depth };
  float4 invClip = keptech::camera.clipToWorld(float4(ndc, 1.0));
  float3 world = invClip.xyz / invClip.w;

  let toLight = light.position - world;

  float dist = length(toLight);

  float attenuation = 1.0 - clamp(dist / light.radius, 0.0, 1.0);

  if (attenuation == 0.0) {
    discard;
  }

  float3 radiance = light.color * light.intensity * attenuation;

  let normal = normalize(normalSample.xyz * 2.0 - 1.0);

  let incident = normalize(toLight);
  let viewDir = normalize(keptech::camera.position - world);
  let halfDir = normalize(incident + viewDir);

  float metallic = metallicRoughnessSample.r;
  float roughness = metallicRoughnessSample.g;

  float3 F0 = float3(0.04);
  F0 = lerp(F0, albedoSample.rgb, metallic);

  float NDF = keptech::DistributionGGX(normal, halfDir, roughness);
  float G = keptech::GeometrySmith(normal, viewDir, incident, roughness);
  float3 F = keptech::fresnelSchlick(max(dot(halfDir, viewDir), 0.0), F0);

  float3 kS = F;
  float3 kD = float3(1.0) - kS;
  kD *= 1.0 - metallic;

  float3 numerator = NDF * G * F;

  float denominator =
      4.0 * max(dot(normal, viewDir), 0.0) * max(dot(normal, incident), 0.0) +
      0.001;
  float3 specular = numerator / denominator;

  float NdotL = clamp(dot(normal, incident), 0.0, 1.0);

  output.specular = float4(specular * radiance * NdotL, 1.0);

  float3 adjusted = kD * (albedoSample.rgb / PI + specular) * radiance * NdotL;

  output.diffuse = float4(adjusted, 1.0);

  return output;
}


