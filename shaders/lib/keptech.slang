module keptech;

__include camera;
__include fullscreen;
__include lighting;
__include textures;
__include maths;

public namespace keptech {
[[vk::binding(0, 0)]]
public ConstantBuffer<keptech::Camera> camera : register(b0);

[[vk::binding(1, 0)]]
public ConstantBuffer<TexData> textures;

[[vk::binding(2, 0)]]
public ConstantBuffer<CubeMapData> cubeMaps;

public struct DeferredOutput {
  public float4 albedo : SV_Target0;
  public float4 normal : SV_Target1;
  public float4 emissiveAo : SV_Target2;
  public float4 metallicRoughness : SV_Target3;
};

public struct DeferredLightingOutput {
  public float4 diffuse : SV_Target0;
  public float4 specular : SV_Target1;
};
}
