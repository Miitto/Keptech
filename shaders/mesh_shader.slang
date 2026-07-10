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

[shader("task")]
[numthreads(1, 1, 1)]
void task() {
  Payload payload;
  DispatchMesh(3, 1, 1, payload);
}

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(1, 1, 1)]
void mesh(out indices uint3 triangles[1], out vertices VertexOutput vertices[3], uint3 dispatchId: SV_DispatchThreadID,
          uint3 groupId: SV_GroupID, uint3 gtid: SV_GroupThreadID, uniform PushConstants PC) {

  let mvp = kt::camera.makeMvp(PC.modelMatrix);

  uint meshletId = groupId.x;

  // Hash-based per-meshlet coloring for visualization
  uint hash = meshletId * 747796405u + 2891336453u;
  float3 color = float3((hash & 0xFF) / 255.0, ((hash >> 8) & 0xFF) / 255.0, ((hash >> 16) & 0xFF) / 255.0);

  float4 offset = float4(0., 0., float(dispatchId.x), 0.0);

  SetMeshOutputCounts(3, 1);

  for (uint i = 0; i < 3; i++) {
    vertices[i].position = mul(positions[i] + offset, mvp);
    vertices[i].color = float4(color, 1.f);
  }

  triangles[0] = uint3(0, 1, 2);
}

[shader("pixel")]
kt::DeferredOutput frag(VertexOutput input, uniform PushConstants PC) {
  kt::DeferredOutput output;

  output.albedo = input.color;

  return output;
}
