module keptech;

__include constants;
__include camera;
__include fullscreen;
__include lighting;
__include textures;
__include maths;

public namespace kt {
public struct VertexAttribs {
  public float4 tangent;
  public float3 normal;
  half2 packedUv;

  public property float2 uv {
    get { return packedUv; }
  }
};

public struct Meshlet {
  public uint vertexOffset;
  public uint vertexCount;
  public uint triangleOffset;
  public uint triangleCount;
  public float3 center;
  public float radius;
};

[[vk::binding(0, 0)]]
public ConstantBuffer<Camera> camera : register(b0);

[[vk::binding(1, 0)]]
public ConstantBuffer<TexData<Sampler2D>> textures2D;
[[vk::binding(1, 0)]]
public ConstantBuffer<TexData<SamplerCube>> texturesCube;

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
