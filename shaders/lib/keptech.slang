module keptech;

__include constants;
__include camera;
__include fullscreen;
__include lighting;
__include textures;
__include maths;

public namespace kt {
public struct Vertex {
  public float3 position;
  public float uvX;
  public float3 normal;
  public float uvY;
  public float4 tangent;
};

[[vk::binding(0, 0)]]
public ConstantBuffer<Camera> camera : register(b0);

[[vk::binding(1, 0)]]
public ConstantBuffer<TexData<Sampler2D>> textures2D;
[[vk::binding(1, 0)]]
public ConstantBuffer<TexData<SamplerCube>> texturesCube;

[[vk::binding(2, 0)]]
public ConstantBuffer<float3[64]> ssaoKernel;

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
