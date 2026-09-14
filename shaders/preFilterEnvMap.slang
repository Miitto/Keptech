import keptech;
import cubemapUtil;

[shader("vertex")]
[cull(kt::CullMode::None)]
CubeV2G vert(uint vid: SV_VertexID) { return cubeVertex(vid); }

[shader("geometry")]
[maxvertexcount(3)]
[instance(6)]
void geom(triangle CubeV2G input[3], uint layer: SV_GSInstanceID, inout TriangleStream<CubeG2F> outputStream) {
  cubeGeom(input, layer, outputStream);
}

uniform TextureCube<float4>.Handle src;
uniform float roughness;

[shader("pixel")]
float4 frag(CubeG2F input) : SV_Target {
  let sampler = kt::samplers::linearMipmapClamp;
  let normal = normalize(input.direction);

  var R = normal;
  var V = R;

  static const let SAMPLE_COUNT = 1024u;
  var totalWeight = 0.0;
  var prefilteredColor = float3(0.0);

  for (var i = 0; i < SAMPLE_COUNT; ++i) {
    let Xi = hammersley(i, SAMPLE_COUNT);
    let H = kt::importanceSampleGGX(Xi, normal, roughness);
    var L = normalize(2.0 * dot(V, H) * H - V);

    let NdotL = max(dot(normal, L), 0.0);
    if (NdotL > 0.0) {
      prefilteredColor += src.Sample(sampler, -L).rgb * NdotL;
      totalWeight += NdotL;
    }
  }

  if (totalWeight > 0.0) {
    prefilteredColor = prefilteredColor / totalWeight;
  }
  return float4(prefilteredColor, 1.0);
}
