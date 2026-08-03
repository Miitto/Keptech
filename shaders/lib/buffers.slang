implementing keptech;

public namespace kt {

public property ConstantBuffer<Camera>.Handle camera {
  get { return ConstantBuffer<Camera>.Handle(uint2(0, 0)); }
}

public property StructuredBuffer<float3>.Handle positions {
  get { return StructuredBuffer<float3>.Handle(uint2(1, 0)); }
}

public property StructuredBuffer<VertexAttribs>.Handle vertexAttribs {
  get { return StructuredBuffer<VertexAttribs>.Handle(uint2(2, 0)); }
}

public property StructuredBuffer<uint>.Handle indices {
  get { return StructuredBuffer<uint>.Handle(uint2(3, 0)); }
}

public property StructuredBuffer<Meshlet>.Handle meshlets {
  get { return StructuredBuffer<Meshlet>.Handle(uint2(4, 0)); }
}

public property StructuredBuffer<uint>.Handle meshletVertices {
  get { return StructuredBuffer<uint>.Handle(uint2(5, 0)); }
}

public property StructuredBuffer<uint>.Handle meshletTriangles {
  get { return StructuredBuffer<uint>.Handle(uint2(6, 0)); }
}

public property StructuredBuffer<Material>.Handle materials {
  get { return StructuredBuffer<Material>.Handle(uint2(7, 0)); }
}

public property StructuredBuffer<Mesh>.Handle meshes {
  get { return StructuredBuffer<Mesh>.Handle(uint2(8, 0)); }
}
}
