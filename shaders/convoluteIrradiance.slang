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

[shader("pixel")]
float4 frag(CubeG2F input) : SV_Target {
  let sampler = kt::samplers::linearClamp;

  let normal = normalize(input.direction);

  var irradiance = float3(0);

  var right = normalize(cross(UP, normal));
  var up = normalize(cross(normal, right));

  static const let sampleDelta = 0.025;
  float nrSamples = 0.0;
  for (float phi = 0; phi < 2.0 * PI; phi += sampleDelta) {
    let sPhi = sin(phi);
    let cPhi = cos(phi);
    for (float theta = 0; theta < 0.5 * PI; theta += sampleDelta) {
      let sTheta = sin(theta);
      let cTheta = cos(theta);
      let tangentSample = float3(sTheta * cPhi, sTheta * sPhi, cTheta);
      var sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;
      sampleVec = -sampleVec; // Sample vec is backwards. // TODO: the maths to figure out why.

      let sample = src.Sample(sampler, sampleVec).rgb * cTheta * sTheta;
      irradiance += (sample / (sample + 1)); // Tonemap it, otherwise the sun blows it all out of proportion. // TODO: this is a hack.
      nrSamples++;
    }
  }

  return { PI * irradiance * (1.0 / nrSamples), 1 };
}
