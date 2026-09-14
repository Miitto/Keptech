import keptech;

[shader("vertex")]
[cull(kt::CullMode::None)]
kt::PosUv vert(uint vid: SV_VertexID) { return kt::fullscreenTri(vid); }

float2 integrateBrdf(float NdotV, float roughness) {
  float3 V = { sqrt(1.0 - NdotV * NdotV), 0.0, NdotV };

  float A = 0;
  float B = 0;

  float3 N = { 0.0, 0.0, 1.0 };

  static const let SAMPLE_COUNT = 1024u;
  for (uint i = 0; i < SAMPLE_COUNT; ++i) {
    let Xi = hammersley(i, SAMPLE_COUNT);
    let H = kt::importanceSampleGGX(Xi, N, roughness);
    var L = normalize(2.0 * dot(V, H) * H - V);

    let NdotL = max(L.z, 0.0);
    let NdotH = max(H.z, 0.0);
    let VdotH = max(dot(V, H), 0.0);

    if (NdotL > 0.0) {
      let G = kt::geometrySmithIbl(N, V, L, roughness);
      let G_Vis = (G * VdotH) / (NdotH * NdotV);
      let Fc = pow(1.0 - VdotH, 5.0);

      A += (1.0 - Fc) * G_Vis;
      B += Fc * G_Vis;
    }
  }

  static const float2 invSampleCount = float2(1.0 / float(SAMPLE_COUNT));

  return float2(A, B) * invSampleCount;
}

[shader("pixel")]
float2 frag(kt::PosUv input) : SV_Target {
  float2 uv = input.uv;

  return integrateBrdf(uv.x, uv.y);
}
