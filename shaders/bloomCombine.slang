import keptech;

static const float bloomIntensity = 0.03;
;

struct v2f {
  float4 position : SV_Position;
  float2 uv;
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
float4 frag(v2f input) : SV_Target {
  float2 uv = input.uv;

  let sceneTex = kt::textures2D.getTexture(kt::TextureIndices::Combine);
  let bloomTex = kt::textures2D.getTexture(kt::TextureIndices::Bloom);

  float4 sceneColor = sceneTex.Sample(uv);
  float4 bloomColor = bloomTex.Sample(uv);

  let bloom = lerp(sceneColor, bloomColor, bloomIntensity);

  return bloom;
}
