implementing keptech;

public namespace kt {
public struct VertexBuffers {
  public StructuredBuffer<float3> positions;
  public StructuredBuffer<VertexAttribs> attributes;
};

public struct MeshletBuffers {
  public StructuredBuffer<Meshlet> meshlets;
  public StructuredBuffer<uint> vertices;
  public StructuredBuffer<uint> triangles;
};

public struct EngineBuffers {
  public VertexBuffers vertices;
  public StructuredBuffer<uint> indices;
  public MeshletBuffers meshlets;
  public StructuredBuffer<Mesh> meshes;
  public StructuredBuffer<Material> materials;
};
}
