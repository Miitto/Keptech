import keptech;

struct PushConstants {
  uint objectIndex;
};

struct VertexOutput {
  float4 position : SV_Position;
  float4 color : COLOR0;
};

const static uint TASK_SHADER_DISPATCH_SIZE = 64;
const static uint MESH_SHADER_DISPATCH_SIZE = 32;

struct Object {
  float4x4 modelMatrix;
  uint meshIndex;
  uint materialIndex;
};
struct Buffers {
  ConstantBuffer<kt::Camera> camera;
  StructuredBuffer<Object> objects;
}

ParameterBlock<Buffers> buffers;
ParameterBlock<kt::EngineBuffers> engineBuffers;

uniform PushConstants PC;

struct Payload {
  uint32_t groupMeshletOffset : MESHLET_OFFSET;
  uint meshletIndices[TASK_SHADER_DISPATCH_SIZE] : MESHLET_INDICES;
}
groupshared Payload payload;
groupshared Atomic<uint> visibleMeshlets;

[shader("task")]
[numthreads(TASK_SHADER_DISPATCH_SIZE, 1, 1)]
void task(uint3 gid: SV_GroupID, uint3 gtid: SV_GroupThreadID) {
  let object = buffers.objects[PC.objectIndex];
  kt::Mesh mesh = engineBuffers.meshes[object.meshIndex];

  uint groupMeshletOffset = gid.x * TASK_SHADER_DISPATCH_SIZE;
  uint totalMeshlets = mesh.meshletCount;

  if (gtid.x == 0) {
    visibleMeshlets.store(0);
    payload.groupMeshletOffset = groupMeshletOffset;
  }

  GroupMemoryBarrierWithGroupSync();

  uint meshletCount = min(TASK_SHADER_DISPATCH_SIZE, totalMeshlets - groupMeshletOffset);
  if (gtid.x < meshletCount) {
    uint idx = groupMeshletOffset + gtid.x;
    kt::Meshlet meshlet = engineBuffers.meshlets.meshlets[mesh.meshletOffset + idx];

    bool inFrustum = buffers.camera.canSee(meshlet.bounds.apply(object.modelMatrix));
    uint count = inFrustum; // Quick boolean to int conversion for WaveActiveSum.

    // So it lives outside the if statement to be read after.
    uint idxOffset = 0;
    // Get the total number of visible meshlets in this wave.
    uint visibleCount = WaveActiveSum(count);

    // WaveIsFirstLane() 'probably' checks if its lane 0, but it might not actually?
    // Being explicit so when we fetch the offset from lane 0 its right.
    if (WaveGetLaneIndex() == 0) {
      // Atomically add the number of visible meshlets in this wave to the global count.
      // Only runs on one lane since atomics are expensive.
      idxOffset = visibleMeshlets.add(visibleCount);
    }

    if (inFrustum) {
      // Pull out the first offset from the wave so all lanes can use it to calculate their own index.
      uint offset = WaveReadLaneAt(idxOffset, 0);
      // Add the number of visible meshlets before this lane in the wave to the offset to get the final index for this lane.
      uint myIndex = WavePrefixCountBits(inFrustum) + offset;

      payload.meshletIndices[myIndex] = gtid.x;
    }
  }

  GroupMemoryBarrierWithGroupSync();

  uint totalVisibleMeshlets = visibleMeshlets.load();
  DispatchMesh(totalVisibleMeshlets, 1, 1, payload);
}

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(MESH_SHADER_DISPATCH_SIZE, 1, 1)]
[cull(kt::CullMode::Back)]
void mesh(in payload Payload payload, OutputIndices<uint3, kt::MAX_TRIANGLES_PER_MESHLET> triangles,
          OutputVertices<VertexOutput, kt::MAX_VERTICES_PER_MESHLET> vertices, uint3 dispatchId: SV_DispatchThreadID,
          uint3 groupId: SV_GroupID, uint3 gtid: SV_GroupThreadID) {
  let object = buffers.objects[PC.objectIndex];
  let mesh = engineBuffers.meshes[object.meshIndex];

  uint meshletId = payload.groupMeshletOffset + payload.meshletIndices[groupId.x];

  uint vCount = 0;
  uint tCount = 0;

  kt::Meshlet meshlet;
  float4x4 mvp;

  if (meshletId < mesh.meshletCount) {
    meshlet = engineBuffers.meshlets.meshlets[mesh.meshletOffset + meshletId];

    mvp = buffers.camera.makeMvp(object.modelMatrix);
    vCount = meshlet.vertexCount;
    tCount = meshlet.triangleCount;
  }

  SetMeshOutputCounts(vCount, tCount);

  if (meshletId >= mesh.meshletCount) {
    return;
  }

  // Hash-based per-meshlet coloring for visualization
  uint hash = meshletId * 747796405u + 2891336453u;
  float3 color = float3((hash & 0xFF) / 255.0, ((hash >> 8) & 0xFF) / 255.0, ((hash >> 16) & 0xFF) / 255.0);

  float4 offset = float4(0., 0., float(dispatchId.x), 0.0);

  uint instanceId = gtid.x;
  for (uint i = instanceId; i < meshlet.vertexCount; i += MESH_SHADER_DISPATCH_SIZE) {
    uint vIdx = engineBuffers.meshlets.vertices[mesh.meshletVertexOffset + meshlet.vertexOffset + i];
    float3 position = engineBuffers.vertices.positions[vIdx];
    vertices[i].position = mul(float4(position, 1.0), mvp);
    vertices[i].color = float4(color, 1.f);
  }

  for (uint i = instanceId; i < meshlet.triangleCount; i += MESH_SHADER_DISPATCH_SIZE) {
    uint tOffset = mesh.meshletTriangleOffset + meshlet.triangleOffset + i * 3;
    uint idx0 = engineBuffers.meshlets.triangles[tOffset + 0];
    uint idx1 = engineBuffers.meshlets.triangles[tOffset + 1];
    uint idx2 = engineBuffers.meshlets.triangles[tOffset + 2];
    triangles[i] = uint3(idx0, idx1, idx2);
  }
}

[shader("pixel")]
float4 frag(VertexOutput input) : SV_Target { return input.color; }
