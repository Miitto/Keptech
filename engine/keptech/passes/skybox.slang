import keptech;

struct v2f {
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
  float3 direction : TEXCOORD1;
};

[shader("vertex")]
[cull(kt::CullMode::None)]
v2f vert(uint vid: SV_VertexID) {
  float2 coord = float2((vid << 1) & 2, vid & 2);
  v2f output = { float4(float2(-1.0, 1.0) + coord * float2(2.0, -2.0), 0.0, 1.0), coord, {} };
  output.direction = mul(camera.clipToCamera(output.position).xyz, float3x3(camera.inverseView));
  return output;
}

ConstantBuffer<kt::Camera> camera;

Texture2D<float4> srcTex;
Texture2D<float> depthTex;

[shader("pixel")]
float4 frag(v2f input) : SV_Target {
  let sampler = kt::samplers::linearClamp;

  float4 srcColor = srcTex.Sample(sampler, input.uv);

  if (!kt::valid(camera.envMap)) {
    return srcColor;
  }

  float depth = depthTex.Sample(sampler, input.uv);

  float3 direction = normalize(input.direction);

  if (depth < 1.0) {
    return srcColor;
  } else {
    return linearToSrgb(camera.envMap.Sample(sampler, direction));
  }
}
