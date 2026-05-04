import keptech;

struct Material {
  uint albedo;
  uint bump;
  uint emissive;
  uint metRough;
  float4 albedoFactor;
  float3 emissiveFactor;
  uint ao;
  float metFactor;
  float roughFactor;
  float specFactor;
  float alphaCutoff;
};

struct Object {
  float4x4 modelMatrix;
  uint materialIndex;
  float[3] padding;
};

struct PushConstants {
  Object *objectBuffer;
  Material *materialBuffer;
};

struct VertexOutput {
  float4 position : SV_Position;
  float3 normal : NORMAL0;
  float uvX : TEXCOORD0;
  float3 tangent : TANGENT0;
  float uvY : TEXCOORD1;
  float3 bitangent : BITANGENT0;
  nointerpolation uint materialIndex : MATERIAL_INDEX;
};

[shader("vertex")]
VertexOutput vert(kt::Vertex v, uint vid: SV_VertexID, uint iid: SV_InstanceID, uint bid: SV_StartInstanceLocation,
                  uniform PushConstants PC) {
  VertexOutput output;

  Object obj = PC.objectBuffer[iid + bid];

  float4 worldPosition = mul(float4(v.position, 1.0), obj.modelMatrix);

  output.position = kt::camera.worldToClip(worldPosition);
  output.uvX = v.uvX;
  output.uvY = v.uvY;

  float3x3 modelRotMatrix = (float3x3)obj.modelMatrix;

  output.normal = normalize(mul(v.normal, modelRotMatrix));
  output.tangent = normalize(mul(v.tangent.xyz, modelRotMatrix));
  output.bitangent = normalize(cross(output.normal, output.tangent) * v.tangent.w);

  output.materialIndex = obj.materialIndex;

  return output;
}

[shader("pixel")]
kt::DeferredOutput frag(VertexOutput input, uniform PushConstants PC) {
  kt::DeferredOutput output;

  if (input.materialIndex == ~0u) {
    discard;
  }

  Material mat = PC.materialBuffer[input.materialIndex];

  float2 uv = float2(input.uvX, input.uvY);

  output.albedo = mat.albedoFactor;
  output.normal = float4(input.normal, 1.f);

  output.metallicRoughness = float4(mat.metFactor, mat.roughFactor, 0.f, 1.f);
  output.emissiveAo = float4(0.0, 0.0, 0.0, 1.f);

  if (mat.albedo != ~0u) {
    output.albedo *= kt::textures2D.getTexture(mat.albedo).Sample(uv);
  }

  if (output.albedo.a < mat.alphaCutoff) {
    discard;
  }

  if (mat.bump != ~0u) {
    float3 normalSample = kt::textures2D.getTexture(mat.bump).Sample(uv).xyz * 2.0 - 1.0;

    float3x3 TBN = float3x3(normalize(input.tangent), normalize(input.bitangent), normalize(input.normal));
    float3 worldNormal = normalize(mul(normalSample, TBN));

    output.normal = float4(worldNormal, 1.f);
  }

  if (mat.emissive != ~0u) {
    output.emissiveAo.rgb = kt::textures2D.getTexture(mat.emissive).Sample(uv).rgb * mat.emissiveFactor;
  }

  if (mat.metRough != ~0u) {
    float4 metallicRoughnessSample = kt::textures2D.getTexture(mat.metRough).Sample(uv);
    output.metallicRoughness.r *= metallicRoughnessSample.b;
    output.metallicRoughness.g *= metallicRoughnessSample.g;
  }

  output.normal.xyz = output.normal.xyz * 0.5 + 0.5;

  return output;
}
