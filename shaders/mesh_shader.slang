import keptech;

struct Object {
  float4x4 modelMatrix;
  uint materialIndex;
  float[3] padding;
};

struct PushConstants {
  float4x4 modelMatrix;
  uint materialIndex;
  uint meshletCount;
};

struct VertexOutput {
  float4 position : SV_Position;
  float4 color : COLOR0;
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

static const float4 positions[3] = { float4(0.0, -1.0, 0.0, 1.0), float4(-1.0, 1.0, 0.0, 1.0), float4(1.0, 1.0, 0.0, 1.0) };

struct Payload {
  uint dummy;
}

// [shader("task")]
// [numthreads(1, 1, 1)]
// void task() {
//   Payload payload;
//   DispatchMesh(1, 1, 1, payload);
// }

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(MESH_SHADER_DISPATCH_SIZE, 1, 1)]
void mesh(out indices uint3 triangles[MAX_TRIANGLES_PER_MESHLET], out vertices VertexOutput vertices[MAX_VERTICES_PER_MESHLET],
          uint3 dispatchId: SV_DispatchThreadID, uint3 groupId: SV_GroupID, uint3 gtid: SV_GroupThreadID, uniform PushConstants PC) {
  uint meshletId = groupId.x;

  if (meshletId >= PC.meshletCount) {
    SetMeshOutputCounts(0, 0);
    return;
  }

  kt::Meshlet meshlet = kt::buffers.meshlets[meshletId];

  let mvp = kt::camera.makeMvp(PC.modelMatrix);

  // Hash-based per-meshlet coloring for visualization
  uint hash = meshletId * 747796405u + 2891336453u;
  float3 color = float3((hash & 0xFF) / 255.0, ((hash >> 8) & 0xFF) / 255.0, ((hash >> 16) & 0xFF) / 255.0);

  float4 offset = float4(0., 0., float(dispatchId.x), 0.0);

  SetMeshOutputCounts(meshlet.vertexCount, meshlet.triangleCount);

  uint instanceId = gtid.x;
  for (uint i = instanceId; i < meshlet.vertexCount; i += MESH_SHADER_DISPATCH_SIZE) {
    uint vIdx = kt::buffers.meshletVertices[meshlet.vertexOffset + i];
    float3 position = kt::buffers.positions[vIdx];
    vertices[i].position = mul(float4(position, 1.0), mvp);
    vertices[i].color = float4(color, 1.f);
  }

  for (uint i = instanceId; i < meshlet.triangleCount; i += MESH_SHADER_DISPATCH_SIZE) {
    uint tOffset = meshlet.triangleOffset + i * 3;
    uint idx0 = kt::buffers.meshletTriangles[tOffset + 0];
    uint idx1 = kt::buffers.meshletTriangles[tOffset + 1];
    uint idx2 = kt::buffers.meshletTriangles[tOffset + 2];
    triangles[i] = uint3(idx0, idx1, idx2);
  }
}

[shader("pixel")]
kt::DeferredOutput frag(VertexOutput input, uniform PushConstants PC) {
  kt::DeferredOutput output;

  output.albedo = input.color;

  return output;
}
