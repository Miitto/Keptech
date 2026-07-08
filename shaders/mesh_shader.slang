import keptech;

struct Material {
  uint albedo;
  uint bump;
  uint emissive;
  uint metRough;
  float4 albedoFactor;
  float3 emissiveFactor;
  uint ao;
  float metFactor;
  float roughFactor;
  float specFactor;
  float alphaCutoff;
};

struct Object {
  float4x4 modelMatrix;
  uint materialIndex;
  float[3] padding;
};

struct PushConstants {
  Object *objectBuffer;
  Material *materialBuffer;
  uint vertexCount;
  uint meshletCount;
  uint meshletVertexCount;
  uint meshletTriangleCount;
};

struct VertexOutput {
  float4 position : SV_Position;
  float3 normal : NORMAL0;
  float uvX : TEXCOORD0;
  float3 tangent : TANGENT0;
  float uvY : TEXCOORD1;
  float3 bitangent : BITANGENT0;
  nointerpolation uint materialIndex : MATERIAL_INDEX;
};

const static uint MESH_SHADER_DISPATCH_SIZE = 32;
const static uint MAX_VERTICES_PER_MESHLET = 64;
const static uint MAX_TRIANGLES_PER_MESHLET = 64;

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(MESH_SHADER_DISPATCH_SIZE, 1, 1)]
void mesh(out indices uint3 triangles[MAX_TRIANGLES_PER_MESHLET], out vertices VertexOutput vertices[MAX_VERTICES_PER_MESHLET],
          uint3 groupId: SV_GroupID, uint3 gtid: SV_GroupThreadID, uniform PushConstants PC) {
  Object obj = *PC.objectBuffer;

  uint meshletId = groupId.x;

  if (meshletId >= PC.meshletCount) {
    return;
  }

  kt::Meshlet meshlet = kt::buffers.meshlets[meshletId];

  uint vertexOffset = meshlet.vertexOffset;
  uint triangleOffset = meshlet.triangleOffset;

  SetMeshOutputCounts(meshlet.vertexCount, meshlet.triangleCount);

  uint instanceIndex = gtid.x;

  for (uint i = instanceIndex; i < meshlet.vertexCount; i += MESH_SHADER_DISPATCH_SIZE) {
    uint localIndex = kt::buffers.meshletVertices[meshlet.vertexOffset + i];

    float3 pos = kt::buffers.positions[localIndex];
    kt::VertexAttribs v = kt::buffers.vertexAttribs[localIndex];

    VertexOutput output;
    float4 worldPosition = mul(float4(pos, 1.0), obj.modelMatrix);

    output.position = kt::camera.worldToClip(worldPosition);
    let uv = v.uv;
    output.uvX = uv.x;
    output.uvY = uv.y;

    float3x3 modelRotMatrix = (float3x3)obj.modelMatrix;

    output.normal = normalize(mul(v.normal, modelRotMatrix));
    output.tangent = normalize(mul(v.tangent.xyz, modelRotMatrix));
    output.bitangent = normalize(cross(output.normal, output.tangent) * v.tangent.w);

    output.materialIndex = obj.materialIndex;

    vertices[i] = output;
  }

  for (uint i = instanceIndex; i < meshlet.triangleCount; i += MESH_SHADER_DISPATCH_SIZE) {
    uint triangleIndex = triangleOffset + i * 3;

    uint3 tri = uint3(kt::buffers.meshletVertices[triangleIndex], kt::buffers.meshletVertices[triangleIndex + 1],
                      kt::buffers.meshletVertices[triangleIndex + 2]);

    triangles[i] = tri;
  }
}

[shader("pixel")]
kt::DeferredOutput frag(VertexOutput input, uniform PushConstants PC) {
  kt::DeferredOutput output;

  if (input.materialIndex == ~0u) {
    discard;
  }

  Material mat = PC.materialBuffer[input.materialIndex];

  float2 uv = float2(input.uvX, input.uvY);

  output.albedo = mat.albedoFactor;
  output.normal = float4(input.normal, 1.f);

  output.metallicRoughness = float4(mat.metFactor, mat.roughFactor, 0.f, 1.f);
  output.emissiveAo = float4(0.0, 0.0, 0.0, 1.f);

  if (mat.albedo != ~0u) {
    output.albedo *= kt::textures2D.getTexture(mat.albedo).Sample(uv);
  }

  if (output.albedo.a < mat.alphaCutoff) {
    discard;
  }

  if (mat.bump != ~0u) {
    float3 normalSample = kt::textures2D.getTexture(mat.bump).Sample(uv).xyz * 2.0 - 1.0;

    float3x3 TBN = float3x3(normalize(input.tangent), normalize(input.bitangent), normalize(input.normal));
    float3 worldNormal = normalize(mul(normalSample, TBN));

    output.normal = float4(worldNormal, 1.f);
  }

  if (mat.emissive != ~0u) {
    output.emissiveAo.rgb = kt::textures2D.getTexture(mat.emissive).Sample(uv).rgb * mat.emissiveFactor;
  }

  if (mat.metRough != ~0u) {
    float4 metallicRoughnessSample = kt::textures2D.getTexture(mat.metRough).Sample(uv);
    output.metallicRoughness.r *= metallicRoughnessSample.b;
    output.metallicRoughness.g *= metallicRoughnessSample.g;
  }

  output.normal.xyz = output.normal.xyz * 0.5 + 0.5;

  return output;
}
