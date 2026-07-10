implementing keptech;

public namespace kt {
public struct VertexAttribs {
  public float4 tangent;
  public float3 normal;
  half2 packedUv;

  public property float2 uv {
    get { return packedUv; }
  }
};

public struct Meshlet {
  public uint vertexOffset;
  public uint vertexCount;
  public uint triangleOffset;
  public uint triangleCount;
  public Sphere bounds;
};
}
