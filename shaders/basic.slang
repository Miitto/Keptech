struct Vertex {
  float3 position;
  float4 color;
};

struct PushConstants {
  float4x4 viewProjMatrix;
  float4x4 modelMatrix;
};

struct VertexOutput {
  float4 position : SV_Position;
  float4 color : COLOR0;
};

[shader("vertex")]
VertexOutput vert(Vertex v) {
  VertexOutput output;

  output.position = float4(v.position, 1.0);
  output.color = v.color;

  return output;
}

[shader("pixel")]
float4 frag(VertexOutput v) : SV_Target0 { return v.color; }
