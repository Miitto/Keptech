implementing keptech;

public namespace kt {
public struct Material {
  public uint albedo;
  public uint bump;
  public uint emissive;
  public uint metRough;
  public float4 albedoFactor;
  public float3 emissiveFactor;
  public uint ao;
  public float metFactor;
  public float roughFactor;
  public float specFactor;
  public float alphaCutoff;
};
}
