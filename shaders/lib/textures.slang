implementing keptech;

namespace kt {
public struct TexData {
  Sampler2D textures[];

  public static bool isTextureValid(uint index) { return index != ~0u; }
  public Sampler2D getTexture(uint index) { return textures[index]; }
};

public struct CubeMapData {
  SamplerCube cubeMaps[];

  public static bool isCubeMapValid(uint index) { return index != ~0u; }
  public SamplerCube getTexture(uint index) { return cubeMaps[index]; }
};
}
