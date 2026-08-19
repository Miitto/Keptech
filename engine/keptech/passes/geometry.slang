import keptech;

struct VertexOutput {
  float4 position : SV_Position;
  float3 normal : NORMAL0;
  float uvX : TEXCOORD0;
  float3 tangent : TANGENT0;
  float uvY : TEXCOORD1;
  float3 bitangent : BITANGENT0;
  nointerpolation uint materialIndex : MATERIAL_INDEX;
};

struct Object {
  float4x4 modelMatrix;
  uint meshIndex;
  uint materialIndex;
};

ConstantBuffer<kt::Camera> camera;
StructuredBuffer<Object> objects;

[shader("vertex")]
[topology(kt::PrimitiveTopology::TriangleList)]
[cull(kt::CullMode::Back)]
VertexOutput vert(float3 position: POSITION, kt::VertexAttribs v, uint baseInstance: SV_StartInstanceLocation) {
  VertexOutput output;

  Object obj = objects[baseInstance];

  let worldPosition = mul(float4(position, 1.0), obj.modelMatrix);

  output.position = camera.worldToClip(worldPosition);
  output.uvX = v.uv.x;
  output.uvY = v.uv.y;

  float3x3 modelRotMatrix = (float3x3)obj.modelMatrix;

  output.normal = normalize(mul(v.normal, modelRotMatrix));
  output.tangent = normalize(mul(v.tangent.xyz, modelRotMatrix));
  output.bitangent = normalize(cross(output.normal, output.tangent) * v.tangent.w);

  output.materialIndex = obj.materialIndex;

  return output;
}

[shader("pixel")]
kt::DeferredOutput frag(VertexOutput input) {
  kt::DeferredOutput output;

  output.albedo = float4(1.f, 1.f, 1.f, 1.f);
  return output;

  if (input.materialIndex == ~0u) {
    discard;
  }

  kt::Material mat = (*kt::materials)[input.materialIndex];

  float2 uv = float2(input.uvX, input.uvY);

  output.albedo = mat.albedoFactor;
  output.normal = float4(input.normal, 1.f);

  output.metallicRoughness = float4(mat.metFactor, mat.roughFactor, 0.f, 1.f);
  output.emissiveAo = float4(0.0, 0.0, 0.0, 1.f);

  if (kt::valid(mat.albedo)) {
    output.albedo *= mat.albedo.Sample(kt::samplers::linearRepeat, uv);
  }

  if (output.albedo.a < mat.alphaCutoff) {
    discard;
  }

  if (kt::valid(mat.bump)) {
    float3 normalSample = mat.bump.Sample(kt::samplers::linearRepeat, uv).xyz * 2.0 - 1.0;

    float3x3 TBN = float3x3(normalize(input.tangent), normalize(input.bitangent), normalize(input.normal));
    float3 worldNormal = normalize(mul(normalSample, TBN));

    output.normal = float4(worldNormal, 1.f);
  }

  if (kt::valid(mat.emissive)) {
    output.emissiveAo.rgb = mat.emissive.Sample(kt::samplers::linearRepeat, uv).rgb * mat.emissiveFactor;
  }

  if (kt::valid(mat.metRough)) {
    float4 metallicRoughnessSample = mat.metRough.Sample(kt::samplers::linearRepeat, uv);
    output.metallicRoughness.r *= metallicRoughnessSample.b;
    output.metallicRoughness.g *= metallicRoughnessSample.g;
  }

  output.normal.xyz = output.normal.xyz * 0.5 + 0.5;

  return output;
}
