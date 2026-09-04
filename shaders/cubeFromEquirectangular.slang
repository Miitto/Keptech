import keptech;
import cubemapUtil;

uniform Texture2D<float4>.Handle src;

struct v2g {
  float4 position : SV_POSITION;
  float3 viewPosition : TEXCOORD0;
};

[shader("vertex")]
[cull(kt::CullMode::None)]
CubeV2G vert(uint vid: SV_VertexID) { return cubeVertex(vid); }

[shader("geometry")]
[maxvertexcount(3)]
[instance(6)]
void geom(triangle CubeV2G input[3], uint layer: SV_GSInstanceID, inout TriangleStream<CubeG2F> outputStream) {
  cubeGeom(input, layer, outputStream);
}

[shader("pixel")]
float4 frag(CubeG2F input) : SV_Target {
  let sampler = kt::samplers::linearClamp;
  float3 direction = normalize(input.direction);
  float2 uv = equirectangularFromSpherical(direction);
  return src.Sample(sampler, uv);
}
