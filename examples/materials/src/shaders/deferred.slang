import keptech;

#include "keptech/cameraUniform.slang"

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
};

struct PushConstants {
  Vertex* vBuffer;
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
};

[shader("vertex")]
VertexOutput vert(uint vid: SV_VertexID, uint iid: SV_InstanceID) {
  VertexOutput output;

  Vertex v = PC.vBuffer[vid];
  Instance inst = PC.iBuffer[iid + PC.instanceOffset];

  output.position = camera.worldToClip(mul(inst.modelMatrix, float4(v.position, 1.0)));
  output.color = v.color;
  output.uvX = v.uvX;
  output.uvY = v.uvY;

  float3x3 modelRotMatrix = (float3x3)inst.modelMatrix;

  output.normal = normalize(mul(modelRotMatrix, v.normal));
  output.tangent = normalize(mul(modelRotMatrix, v.tangent.xyz));
  output.bitangent = normalize(cross(output.normal, output.tangent) * v.tangent.w);

  return output;
}

struct FragmentOutput
{
    float4 albedo   : SV_Target0;
    float4 normal   : SV_Target1;
};

static const float SRGB_TO_LINEAR_FACTOR = 2.2;
static const float LINEAR_TO_SRGB_FACTOR = 1.0 / SRGB_TO_LINEAR_FACTOR;

float4 srgbToLinear(float4 color) {
  return float4(pow(color.rgb, float3(SRGB_TO_LINEAR_FACTOR)), color.a);
}

float4 linearToSrgb(float4 color) {
  return float4(pow(color.rgb, float3(LINEAR_TO_SRGB_FACTOR)), color.a);
}

[shader("pixel")]
FragmentOutput frag(VertexOutput input) {
  FragmentOutput output;

  output.albedo = input.color;
  output.normal = float4(input.normal, 1.f);

  return output;
}
