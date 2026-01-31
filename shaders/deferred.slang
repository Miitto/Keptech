import keptech;

struct Vertex {
  float3 position;
  float uvX;
  float3 normal;
  float uvY;
  float4 color;
  float4 tangent;
};

struct Tex {
  float2 uvScale;
  float2 uvOffset;
  float uvRotation;
  uint textureIndex;

  property bool valid {
    get { return textureIndex != ~0u; }
  }

  float2 transformedUV(float2 uv) {
      float cosRotation = cos(uvRotation);
      float sinRotation = sin(uvRotation);
      float2x2 rotMatrix = float2x2(
        cosRotation, -sinRotation,
        sinRotation, cosRotation
      );
      return mul(uv, rotMatrix) * uvScale + uvOffset;
  }

  float4 Sample(float2 uv) {
    let texture = keptech::textures.getTexture(textureIndex);
    return texture.Sample(transformedUV(uv));
  }
};

struct Instance {
  float4x4 modelMatrix;
  Tex albedo;
  Tex normal;
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
    Tex albedoTex : TEXCOORD2;
    Tex normalTex : TEXCOORD3;
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

  output.albedoTex = inst.albedo;
  output.normalTex = inst.normal;

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

  float2 uv = float2(input.uvX, input.uvY);

  output.albedo = input.color;
  output.normal = float4(input.normal, 1.f);

  if (input.albedoTex.valid) {
    output.albedo *= input.albedoTex.Sample(uv);
  }

  if (input.normalTex.valid) {
    float3 normalSample = input.normalTex.Sample(uv).xyz * 2.0 - 1.0;

    float3x3 TBN = float3x3(normalize(input.tangent), normalize(input.bitangent), normalize(input.normal));
    float3 worldNormal = normalize(mul(TBN, normalSample));

    output.normal = float4(worldNormal, 1.f);
  }

  if (output.albedo.a < 0.01f) {
    discard;
  }


  return output;
}
