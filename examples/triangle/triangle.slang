import keptech;

static const float3 POSITIONS[3] = { { 0.0f, 0.5f, 0.0f }, { 0.5f, -0.5f, 0.0f }, { -0.5f, -0.5f, 0.0f } };

static const float3 COLORS[3] = { { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } };

struct VOut {
  float4 position : SV_Position;
  float3 color : COLOR;
}

[shader("vertex")]
[topology(kt::PrimitiveTopology::TriangleList)]
[cull(kt::CullMode::None)]
VOut vert(uint vid: SV_VertexID) {
  VOut output;

  output.position = float4(POSITIONS[vid], 1.0);
  output.color = COLORS[vid];

  return output;
}

[shader("pixel")]
float4 frag(VOut input) : SV_Target { return float4(input.color, 1.0); }
