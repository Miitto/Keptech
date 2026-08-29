import keptech;

struct VsOutput {
  float4 position : SV_Position;
  float4 uv : TEXCOORD0;
}

struct PushConstants {
  float2 offset;
  float2 extent;
  float z;
  float level;
  Texture2DArray<float4>.Handle src;
}

uniform PushConstants PC;

[shader("vertex")]
[cull(kt::CullMode::None)]
[depthWrite(false)]
VsOutput vert(uint vid: SV_VertexID) {
  float2 coord = float2((vid << 1) & 2, vid & 2);
  VsOutput output = { float4(float2(-1.0, 1.0) + coord * float2(2.0, -2.0), 0.0, 1.0), float4(PC.offset + coord, PC.z, PC.level) };
  return output;
}

[shader("pixel")]
float4 frag(VsOutput input) : SV_Target {
  // TODO: Make sampler configurable.
  return PC.src.SampleLevel(kt::samplers::linearClamp, input.uv.xyz, input.uv.w);
}
