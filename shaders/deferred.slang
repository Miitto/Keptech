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
    float2x2 rotMatrix =
        float2x2(cosRotation, -sinRotation, sinRotation, cosRotation);
    return mul(uv, rotMatrix) * uvScale + uvOffset;
  }

  float4 Sample(float2 uv) {
    let texture = keptech::textures.getTexture(textureIndex);
    return texture.Sample(transformedUV(uv));
  }
};

struct Material {
  float4 albedoColor;
  float3 emissiveColor;
  float metallic;
  Tex albedo;
  Tex bump;
  Tex emissive;
  Tex metallicRoughness;
  Tex ao;
  float roughness;
};

struct Instance {
  float4x4 modelMatrix;
};

struct PushConstants {
  Instance *iBuffer;
  uint instanceOffset;
  Material *mBuffer;
};

struct VertexOutput {
  float4 position : SV_Position;
  float4 color : COLOR0;
  float3 normal : NORMAL0;
  float uvX : TEXCOORD0;
  float3 tangent : TANGENT0;
  float uvY : TEXCOORD1;
  float3 bitangent : BITANGENT0;
};

[shader("vertex")]
VertexOutput vert(Vertex v, uint vid: SV_VertexID, uint iid: SV_InstanceID,
                  uniform PushConstants PC) {
  VertexOutput output;

  Instance inst = PC.iBuffer[iid + PC.instanceOffset];

  float4 worldPosition = mul(float4(v.position, 1.0), inst.modelMatrix);

  output.position = keptech::camera.worldToClip(worldPosition);
  output.color = v.color;
  output.uvX = v.uvX;
  output.uvY = v.uvY;

  float3x3 modelRotMatrix = (float3x3)inst.modelMatrix;

  output.normal = normalize(mul(v.normal, modelRotMatrix));
  output.tangent = normalize(mul(v.tangent.xyz, modelRotMatrix));
  output.bitangent =
      normalize(cross(output.normal, output.tangent) * v.tangent.w);

  return output;
}

[shader("pixel")]
keptech::DeferredOutput frag(VertexOutput input, uniform PushConstants PC) {
  keptech::DeferredOutput output;

  Material mat = PC.mBuffer[0];

  float2 uv = float2(input.uvX, input.uvY);

  output.albedo = input.color;
  output.normal = float4(input.normal, 1.f);

  output.metallicRoughness = float4(mat.metallic, mat.roughness, 0.f, 1.f);
  output.emissiveAo = float4(0.0, 0.0, 0.0, 1.f);

  if (mat.albedo.valid) {
    output.albedo *= mat.albedo.Sample(uv);
  }

  if (output.albedo.a < 0.01f) {
    discard;
  }

  if (mat.bump.valid) {
    float3 normalSample = mat.bump.Sample(uv).xyz * 2.0 - 1.0;

    float3x3 TBN =
        float3x3(normalize(input.tangent), normalize(input.bitangent),
                 normalize(input.normal));
    float3 worldNormal = normalize(mul(normalSample, TBN));

    output.normal = float4(worldNormal, 1.f);
  }

  if (mat.emissive.valid) {
    output.emissiveAo.rgb = mat.emissive.Sample(uv).rgb * mat.emissiveColor;
  }

  if (mat.metallicRoughness.valid) {
    float4 metallicRoughnessSample = mat.metallicRoughness.Sample(uv);
    output.metallicRoughness.r *= metallicRoughnessSample.b;
    output.metallicRoughness.g *= metallicRoughnessSample.g;
  }

  if (mat.ao.valid) {
    output.emissiveAo.a = mat.ao.Sample(uv).r;
  }

  output.normal = output.normal;

  return output;
}
