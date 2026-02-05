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
  float3 worldPos;
};

[shader("vertex")]
v2f vert(uint vid: SV_VertexID, uniform PointLightPC light) {
  v2f output;

  float4 vertPos = float4(CUBE_VERTS[CUBE_INDICES[vid]], 1.0);

  float4 worldPos = (vertPos * light.radius) + float4(light.position, 1.0);

  output.position = keptech::camera.worldToClip(worldPos);
  output.worldPos = worldPos.xyz;

  return output;
}

[shader("pixel")]
keptech::DeferredLightingOutput frag(v2f input, uniform PointLightPC light,
                                     uniform GBufferIndices gBufferIndices) {
  keptech::DeferredLightingOutput output;

  let ndcPos = input.position / input.position.w;
  let uv = (ndcPos.xy + 1.0) * 0.5;

  let normalTex = keptech::textures.getTexture(gBufferIndices.normalIndex);
  let depthTex = keptech::textures.getTexture(gBufferIndices.depthIndex);
  let normalSample = normalTex.Sample(uv);
  let depthSample = depthTex.Sample(uv).r;

  let color = light.color * light.intensity;

  let toLight = light.position - input.worldPos;
  let toLightDistSq = dot(toLight, toLight);

  let radiusSq = light.radius * light.radius;

  let falloff = max(1.0 - (toLightDistSq / radiusSq), 0.f);

  let normal = normalize(normalSample.xyz * 2.0 - 1.0);

  let inDir = max(dot(normal, normalize(toLight)), 0.0);

  let diffuse = color * inDir * falloff;

  output.diffuse = float4(diffuse, 1.0);
  output.specular = float4(0.0, 0.0, 0.0, 1.0);

  return output;
}
