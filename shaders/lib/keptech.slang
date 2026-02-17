module keptech;

__include camera;
__include fullscreen;

public namespace keptech {
[[vk::binding(0, 0)]]
public ConstantBuffer<keptech::Camera> camera : register(b0);

public struct TexData {
  Sampler2D textures[];

  public Sampler2D getTexture(int index) { return textures[index]; }
};

[[vk::binding(1, 0)]]
public ConstantBuffer<TexData> textures;

public struct DeferredOutput {
  public float4 albedo : SV_Target0;
  public float4 normal : SV_Target1;
};

public struct DeferredLightingOutput {
  public float4 diffuse : SV_Target0;
  public float4 specular : SV_Target1;
};
}
