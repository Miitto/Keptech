import keptech;

static const float3 CUBE_VERTS[8] = {
  { -1.f, -1.f, -1.f }, { 1.f, -1.f, -1.f }, { 1.f, 1.f, -1.f }, { -1.f, 1.f, -1.f },
  { -1.f, -1.f, 1.f },  { 1.f, -1.f, 1.f },  { 1.f, 1.f, 1.f },  { -1.f, 1.f, 1.f },
};
static const uint CUBE_INDICES[36] = {
  0, 1, 2, 0, 2, 3, 1, 5, 6, 1, 6, 2, 5, 4, 7, 5, 7, 6, 4, 0, 3, 4, 3, 7, 3, 2, 6, 3, 6, 7, 4, 5, 1, 4, 1, 0,
};

struct v2f {
  float4 position : SV_Position;
  nointerpolation kt::PointLight light : LIGHT;
};

ConstantBuffer<kt::Camera> camera;
StructuredBuffer<kt::PointLight> lightBuffer;

Texture2D<float4> albedoTex;
Texture2D<float4> normalTex;
Texture2D<float4> metallicRoughnessTex;
Texture2D<float4> emissiveTex;
Texture2D<float> depthTex;

[shader("vertex")]
v2f vert(uint vid: SV_VertexID, uint iid: SV_InstanceID, uint bid: SV_StartInstanceLocation) {
  v2f output;

  kt::PointLight light = lightBuffer[iid + bid];

  float3 vertPos = CUBE_VERTS[CUBE_INDICES[vid]];
  float3 scaledPos = vertPos * light.radius;

  float3 lightPos = light.position;

  float4 worldPos = float4(scaledPos + lightPos, 1.0);

  output.position = (*kt::camera).worldToClip(worldPos);

  output.light = light;

  return output;
}

[shader("pixel")]
kt::DeferredLightingOutput frag(v2f input) {
  kt::DeferredLightingOutput output;

  let camera = (*kt::camera);

  let fragCoord = input.position;

  kt::PointLight light = input.light;

  float2 uv = fragCoord.xy / camera.viewportSize;

  let albedoSample = albedoTex.Sample(kt::samplers::linearClamp, uv);
  let metallicRoughnessSample = metallicRoughnessTex.Sample(kt::samplers::linearClamp, uv);
  let normalSample = normalTex.Sample(kt::samplers::linearClamp, uv);
  float depth = depthTex.Sample(kt::samplers::linearClamp, uv).r;

  float3 ndc = { uv * 2.0 - 1.0, depth };
  float4 invClip = camera.clipToWorld(float4(ndc, 1.0));
  float3 world = invClip.xyz / invClip.w;

  let toLight = light.position - world;

  float shadowMapSample = 1.f;

  float dist = length(toLight);

  float attenuation = 1.0 - clamp(dist / light.radius, 0.0, 1.0);

  if (attenuation == 0.0) {
    discard;
  }

  float3 radiance = light.color * attenuation;

  let shadow = kt::calculatePointLightOcclusion(toLight, dist, light);

  radiance *= shadow;

  let normal = normalize(normalSample.xyz * 2.0 - 1.0);

  let incident = normalize(toLight);
  let viewDir = normalize(camera.position - world);
  let halfDir = normalize(incident + viewDir);

  float metallic = metallicRoughnessSample.r;
  float roughness = metallicRoughnessSample.g;

  float3 F0 = float3(0.04);
  F0 = lerp(F0, albedoSample.rgb, metallic);

  float NDF = kt::DistributionGGX(normal, halfDir, roughness);
  float G = kt::GeometrySmith(normal, viewDir, incident, roughness);
  float3 F = kt::fresnelSchlick(max(dot(halfDir, viewDir), 0.0), F0);

  float3 kS = F;
  float3 kD = float3(1.0) - kS;
  kD *= 1.0 - metallic;

  float3 numerator = NDF * G * F;

  float denominator = 4.0 * max(dot(normal, viewDir), 0.0) * max(dot(normal, incident), 0.0) + 0.001;
  float3 specular = numerator / denominator;

  float NdotL = clamp(dot(normal, incident), 0.0, 1.0);

  output.specular = float4(specular * radiance * NdotL, 1.0);

  float3 adjusted = kD * (albedoSample.rgb / PI + specular) * radiance * NdotL;

  output.diffuse = float4(adjusted, 1.0);

  return output;
}
