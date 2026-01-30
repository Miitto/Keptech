import keptech;

struct Vertex {
  float3 position;
  float uvX;
  float3 normal;
  float uvY;
  float4 color;
  float4 tangent;
};

struct Instance {
  float4x4 modelMatrix;
  uint albedoTextureIndex;
  uint normalTextureIndex;
};

struct PushConstants {
  Instance* iBuffer;
  uint instanceOffset;
};

[vk::push_constant]
uniform PushConstants PC;

ConstantBuffer<float4x4> modelMatrix;

struct VertexOutput
{
    float4 position : SV_Position;
    float4 color : COLOR0;
    float3 normal : NORMAL0;
    float uvX : TEXCOORD0;
    float3 tangent : TANGENT0;
    float uvY : TEXCOORD1;
    float3 bitangent : BITANGENT0;
    uint albedoTextureIndex : TEXCOORD2;
    uint normalTextureIndex : TEXCOORD3;
};

[shader("vertex")]
VertexOutput vert(Vertex v, uint vid: SV_VertexID, uint iid: SV_InstanceID) {
  VertexOutput output;

  Instance inst = PC.iBuffer[iid + PC.instanceOffset];

  float4 worldPosition = mul(float4(v.position, 1.0), inst.modelMatrix);

  output.position = keptech::camera.worldToClip(worldPosition);
  output.color = v.color;
  output.uvX = v.uvX;
  output.uvY = v.uvY;

  float3x3 modelRotMatrix = (float3x3)inst.modelMatrix;

  output.normal = normalize(mul(modelRotMatrix, v.normal));
  output.tangent = normalize(mul(modelRotMatrix, v.tangent.xyz));
  output.bitangent = normalize(cross(output.normal, output.tangent) * v.tangent.w);

  output.albedoTextureIndex = inst.albedoTextureIndex;
  output.normalTextureIndex = inst.normalTextureIndex;

  return output;
}

static const float SRGB_TO_LINEAR_FACTOR = 2.2;
static const float LINEAR_TO_SRGB_FACTOR = 1.0 / SRGB_TO_LINEAR_FACTOR;

float4 srgbToLinear(float4 color) {
  return float4(pow(color.rgb, float3(SRGB_TO_LINEAR_FACTOR)), color.a);
}

float4 linearToSrgb(float4 color) {
  return float4(pow(color.rgb, float3(LINEAR_TO_SRGB_FACTOR)), color.a);
}

[shader("pixel")]
keptech::DeferredOutput frag(VertexOutput input) {
  keptech::DeferredOutput output;

  if (input.albedoTextureIndex != uint(-1)) {
    let albedoTexture = keptech::textures.getTexture(input.albedoTextureIndex);
    input.color = albedoTexture.Sample(float2(input.uvX, input.uvY));
  }

  output.albedo = input.color;
  output.normal = float4(input.normal, 1.f);

  return output;
}
