import keptech;

struct PushConstant {
  float2 texelSize;
};

struct v2f {
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
};

[shader("vertex")]
v2f vert(uint vid: SV_VertexID) {
  v2f output;

  float3 vertPos = kt::getFullscreenTriangleVertex(vid);

  output.position = float4(vertPos, 1.0);
  output.uv = vertPos.xy * 0.5 + 0.5;

  return output;
}

[shader("pixel")]
float frag(v2f input, uniform PushConstant pc) : SV_Target {
  let ssaoTex = kt::textures2D.getTexture(kt::TextureIndices::SsaoResult);

  float result = 0.f;
  for (int x = -2; x <= 2; x++) {
    for (int y = -2; y <= 2; y++) {
      float2 offset = float2(x, y) * pc.texelSize;
      result += ssaoTex.Sample(input.uv + offset).r;
    }
  }

  return result / 16.f;
}
