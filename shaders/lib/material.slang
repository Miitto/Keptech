implementing keptech;

public namespace kt {
public struct Material {
  public DescriptorHandle<Sampler2D> albedo;
  public DescriptorHandle<Sampler2D> bump;
  public DescriptorHandle<Sampler2D> emissive;
  public DescriptorHandle<Sampler2D> metRough;
  public DescriptorHandle<Sampler2D> ao;
  public float4 albedoFactor;
  public float3 emissiveFactor;
  public float metFactor;
  public float roughFactor;
  public float specFactor;
  public float alphaCutoff;
};
}
