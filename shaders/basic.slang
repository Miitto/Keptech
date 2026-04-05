struct Vertex {
  float3 position;
  float uvX;
  float3 normal;
  float uvY;
  float4 color;
  float4 tangent;
};

struct PushConstants {
  float4x4 viewProjMatrix;
  float4x4 modelMatrix;
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
VertexOutput vert(Vertex v, uniform PushConstants PC) {
  VertexOutput output;

  float4 worldPosition = mul(float4(v.position, 1.0), PC.modelMatrix);

  output.position = mul(worldPosition, PC.viewProjMatrix);
  output.color = v.color;
  output.uvX = v.uvX;
  output.uvY = v.uvY;

  float3x3 modelRotMatrix = (float3x3)PC.modelMatrix;

  output.normal = normalize(mul(v.normal, modelRotMatrix));
  output.tangent = normalize(mul(v.tangent.xyz, modelRotMatrix));
  output.bitangent = normalize(cross(output.normal, output.tangent) * v.tangent.w);

  return output;
}

[shader("pixel")]
float4 frag(VertexOutput v) : SV_Target0 { return v.color; }
