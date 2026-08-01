implementing keptech;

public namespace kt {
public struct Mesh {
  public uint32_t indexOffset;
  public uint32_t indexCount;
  public int32_t vertexOffset;
  public uint32_t vertexCount;
  public uint32_t meshletOffset;
  public uint32_t meshletCount;
  public uint32_t meshletVertexOffset;
  public uint32_t meshletVertexCount;
  public uint32_t meshletTriangleOffset;
  public uint32_t meshletTriangleCount;
  public Sphere boundingSphere;
};
}
