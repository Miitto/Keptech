implementing keptech;

public namespace kt {
static const float3 FULLSCREEN_TRIANGLE_VERTS[3] = {
  { -1.f, -1.f, 0.f },
  { 3.f, -1.f, 0.f },
  { -1.f, 3.f, 0.f },
};

public float3 getFullscreenTriangleVertex(uint vertexID) {
  return FULLSCREEN_TRIANGLE_VERTS[vertexID];
}
}
