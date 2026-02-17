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

struct PointLightPC {
  float3 position;
  float radius;
  float3 color;
  float intensity;
};

struct GBufferIndices {
  uint albedoIndex;
  uint normalIndex;
  uint depthIndex;
};

struct v2f {
  float4 position : SV_Position;
};

[shader("vertex")]
v2f vert(uint vid: SV_VertexID, uniform PointLightPC light) {
  v2f output;

  float3 vertPos = CUBE_VERTS[CUBE_INDICES[vid]];
  float3 scaledPos = vertPos * light.radius;

  float4 worldPos = float4(scaledPos + light.position, 1.0);

  output.position = keptech::camera.worldToClip(worldPos);

  return output;
}

[shader("pixel")]
keptech::DeferredLightingOutput frag(v2f input, uniform PointLightPC light,
                                     uniform GBufferIndices gBufferIndices,
                                     float4 fragCoord: SV_Position) {
  keptech::DeferredLightingOutput output;

  float2 uv = fragCoord.xy / keptech::camera.viewportSize;

  let normalTex = keptech::textures.getTexture(gBufferIndices.normalIndex);
  let depthTex = keptech::textures.getTexture(gBufferIndices.depthIndex);

  let normalSample = normalTex.Sample(uv);
  float depth = depthTex.Sample(uv).r;

  float3 ndc = { uv * 2.0 - 1.0, depth };
  float4 invClip = keptech::camera.clipToWorld(float4(ndc, 1.0));
  float3 world = invClip.xyz / invClip.w;

  let toLight = light.position - world;

  float dist = length(toLight);

  float attenuation = 1.0 - clamp(dist / light.radius, 0.0, 1.0);

  output.diffuse = float4(world, 1.0);
  output.specular = float4(depth, attenuation, dist, 1.0);

  if (attenuation == 0.0) {
    // discard;
  }

  let normal = normalize(normalSample.xyz * 2.0 - 1.0);

  let incident = normalize(world - light.position);
  let viewDir = normalize(keptech::camera.position - world);
  let halfDir = normalize(incident + viewDir);

  let radiance = light.color * light.intensity * attenuation;

  let lambert = clamp(dot(normal, incident), 0.0, 1.0);
  let rFactor = pow(clamp(dot(normal, halfDir), 0.0, 1.0), 16.0);
  let specFactor = pow(rFactor, 60.0);

  output.diffuse = float4(radiance * lambert, 1.0);
  output.specular = float4(radiance * specFactor * 0.33, 1.0);

  return output;
}
