import keptech;

[shader("vertex")]
[cull(kt::CullMode::None)]
kt::PosUv vert(uint vid: SV_VertexID) { return kt::fullscreenTri(vid); }

ConstantBuffer<kt::Camera> camera;

Texture2D<float4> albedoTex;
Texture2D<float4> normalTex;
Texture2D<float4> emissiveTex;
Texture2D<float4> metallicRoughnessTex;
Texture2D<float> depthTex;
Texture2D<float4> lightingTex;

uniform float2 invViewportSize;

[shader("pixel")]
float4 frag(kt::PosUv input) : SV_Target {
  let fragCoord = input.position;

  float2 uv = fragCoord.xy * invViewportSize;

  let albedoSample = albedoTex.Sample(kt::samplers::linearClamp, uv);
  let metallicRoughnessSample = metallicRoughnessTex.Sample(kt::samplers::linearClamp, uv);
  let normalSample = normalTex.Sample(kt::samplers::linearClamp, uv);
  let depth = depthTex.Sample(kt::samplers::linearClamp, uv).r;
  let lightingSample = lightingTex.Sample(kt::samplers::linearClamp, uv);

  if (depth >= 1.0) {
    return 0;
  }

  float3 ndc = { uv * 2.0 - 1.0, depth };
  float4 invClip = camera.clipToWorld(float4(ndc, 1.0));
  float3 world = invClip.xyz / invClip.w;

  let normal = normalize(normalSample.rgb * 2.0 - 1.0);

  float3 viewDir = normalize(camera.position - world);
  float3 reflectDir = reflect(-viewDir, normal);

  float3 F0 = float3(0.04);
  F0 = lerp(F0, albedoSample.rgb, metallicRoughnessSample.r);

  let F = kt::fresnelSchlickRoughness(max(dot(normal, viewDir), 0.0), F0, metallicRoughnessSample.g);

  float3 kS = F;
  float3 kD = float3(1.0) - kS;
  kD *= 1.0 - metallicRoughnessSample.r;

  float3 normalSampleDir = normal;

  float3 irradiance = camera.envMapIrradiance.Sample(kt::samplers::linearClamp, normal).rgb;
  float3 diffuse = irradiance * albedoSample.rgb;

  static const float MAX_REFLECTION_LOD = 4.0;
  float3 prefilteredColor =
      camera.envMapPreFilter.SampleLevel(kt::samplers::linearMipmapClamp, reflectDir, metallicRoughnessSample.g * MAX_REFLECTION_LOD).rgb;
  float2 brdf =
      camera.integratedBrdf.Sample(kt::samplers::linearClamp, float2(max(dot(normal, viewDir), 0.0), metallicRoughnessSample.g)).rg;

  float3 specular = prefilteredColor * (F * brdf.x + brdf.y);

  float3 ambient = (kD * diffuse + specular) /* * ao */;

  float3 lit = ambient + lightingSample.rgb;
  return float4(lit, 1.0);
}
