implementing keptech;

namespace kt {

namespace constants {
public static const uint albedoIndex = 0u;
public static const uint normalIndex = 1u;
public static const uint emissiveAoIndex = 2u;
public static const uint metallicRoughnessIndex = 3u;
public static const uint depthIndex = 4u;
public static const uint diffuseIndex = 5u;
public static const uint specularIndex = 6u;
public static const uint combineIndex = 7u;
public static const uint bloomIndex = 8u;
}

public struct TexData<Sampler> {
  Sampler textures[];

  public static bool isTextureValid(uint index) { return index != ~0u; }
  public Sampler getTexture(uint index) { return textures[index]; }
};
}
