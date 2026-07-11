implementing keptech;

public namespace kt {

// Can't figure out how to get an array of differently typed buffers to work, and seperate members use different bindings which I can't be
// bothered to maintain, so just using BDA.
public struct Buffers {
  public float3 *positions;
  public VertexAttribs *vertexAttribs;
  public uint *indices;
  public Meshlet *meshlets;
  public uint *meshletVertices;
  public uint *meshletTriangles;
  public Material *materials;
};

public property ConstantBuffer<Camera>.Handle camera {
  get { return ConstantBuffer<Camera>.Handle(uint2(0, 0)); }
}
public property ConstantBuffer<Buffers>.Handle buffers {
  get { return ConstantBuffer<Buffers>.Handle(uint2(1, 0)); }
}
}
