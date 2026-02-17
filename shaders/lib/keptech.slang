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
  public float4 emissiveAo : SV_Target2;
  public float4 metallicRoughness : SV_Target3;
};

public struct DeferredLightingOutput {
  public float4 diffuse : SV_Target0;
  public float4 specular : SV_Target1;
};

static const float SRGB_TO_LINEAR_FACTOR = 2.2;
static const float LINEAR_TO_SRGB_FACTOR = 1.0 / SRGB_TO_LINEAR_FACTOR;

public float4 srgbToLinear(float4 color) {
  return float4(pow(color.rgb, float3(SRGB_TO_LINEAR_FACTOR)), color.a);
}

public float4 linearToSrgb(float4 color) {
  return float4(pow(color.rgb, float3(LINEAR_TO_SRGB_FACTOR)), color.a);
}
}
