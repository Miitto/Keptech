implementing keptech;

public namespace kt {

public struct Buffers {
  public StructuredBuffer<float3> positions;
  public StructuredBuffer<VertexAttribs> vertexAttribs;
  public StructuredBuffer<uint> indices;
  public StructuredBuffer<Meshlet> meshlets;
  public StructuredBuffer<uint> meshletVertices;
  public StructuredBuffer<uint> meshletTriangles;
};

[[vk::binding(2, 0)]]
public Buffers buffers : register(b2);
}
