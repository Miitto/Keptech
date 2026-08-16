implementing keptech;

public namespace kt {
public enum AlphaMode : uint32_t { Opaque = 0, Mask = 1, Blend = 2 };

public struct Material {
  public DescriptorHandle<Texture2D> albedo;
  public DescriptorHandle<Texture2D> bump;
  public DescriptorHandle<Texture2D> emissive;
  public DescriptorHandle<Texture2D> metRough;
  public DescriptorHandle<Texture2D> ao;
  public float4 albedoFactor;
  public float3 emissiveFactor;
  public float metFactor;
  public float roughFactor;
  public float specFactor;
  public float alphaCutoff;
  public AlphaMode alphaMode;
};
}
