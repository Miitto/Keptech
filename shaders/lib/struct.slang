implementing keptech;

public struct IndirectDrawCommand {
  public uint vertexCount;
  public uint instanceCount;
  public uint firstVertex;
  public uint firstInstance;
}

public struct IndirectDrawIndexedCommand {
  public uint indexCount;
  public uint instanceCount;
  public uint firstIndex;
  public int vertexOffset;
  public uint firstInstance;
}

public typedef uint3 IndirectComputeDispatchCommand;
public typedef uint3 IndirectDrawMeshTasksCommand;
