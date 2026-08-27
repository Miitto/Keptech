import keptech;

struct VsOutput {
  float4 position : SV_Position;
  float4 uv : TEXCOORD0;
}

[shader("vertex")]
VsOutput vert(uint vid: SV_VertexID, uniform float2 offset, uniform float2 extent, uniform float z, uniform float level) {
  float2 coord = float2((vid << 1) & 2, vid & 2);
  VsOutput output = { float4(float2(-1.0, 1.0) + coord * float2(2.0, -2.0), 0.0, 1.0), float4(offset + coord * extent, z, level) };
  return output;
}

[shader("pixel")]
float4 frag(VsOutput input, uniform Texture2D<float4>.Handle src) : SV_Target {
  // TODO: Make sampler configurable.
  return src.SampleLevel(kt::samplers::linearClamp, input.uv.xy, input.uv.z);
}
