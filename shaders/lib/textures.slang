implementing keptech;

namespace kt {

public enum TextureIndices : uint {
  Albedo,
  Normal,
  EmissiveAo,
  MetallicRoughness,
  Depth,
  Diffuse,
  Specular,
  SsaoResult,
  SsaoNoise,
  SsaoBlur,
  Combine,
  Bloom,
};

public struct TexData<Sampler> {
  Sampler textures[];

  public static bool isTextureValid(uint index) { return index != ~0u; }
  public Sampler getTexture(uint index) { return textures[index]; }
};
}
