implementing keptech;

public namespace kt {
public struct Sphere {
  public float3 center;
  public float radius;

  public Sphere apply(float4x4 transform) {
    let newCenter = mul(transform, float4(center, 1.f)).xyz;
    float maxScale = max(max(length(transform[0].xyz), length(transform[1].xyz)), length(transform[2].xyz));
    Sphere newSphere;
    newSphere.center = newCenter;
    newSphere.radius = radius * maxScale;
    return newSphere;
  }
}
}
