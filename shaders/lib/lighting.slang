implementing keptech;

namespace kt {
public struct PointLight {
  public float3 position;
  public float radius;
  public float3 color;
  public float intensity;
};

public float calculatePointLightOcclusion(float3 worldToLight, float distToLight, PointLight light) {
  return 1.f;
  // TODO: Shadow mapping
  // if (!valid(light.shadowMap)) {
  //   return 1.f;
  // }
  // let shadowMapSample = light.shadowMap.Sample(worldToLight).r;

  // float closestDepth = shadowMapSample * light.radius;
  // float currentDepth = distToLight;

  // float bias = 0.05;
  // var shadow = currentDepth - bias > closestDepth ? 0.0 : 1.0;

  // return shadow;
}

public float DistributionGGX(float3 N, float3 H, float roughness) {
  float a = roughness * roughness;
  float a2 = a * a;
  float NdotH = max(dot(N, H), 0.0);
  float NdotH2 = NdotH * NdotH;

  float num = a2;
  float denom = (NdotH2 * (a2 - 1.0) + 1.0);
  denom = PI * denom * denom;

  return num / denom;
}

public float3 fresnelSchlick(float cosTheta, float3 F0) { return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0); }

public float GeometrySchlickGGX(float NdotV, float roughness) {
  float r = (roughness + 1.0);
  float k = (r * r) / 8.0;

  float num = NdotV;
  float denom = NdotV * (1.0 - k) + k;

  return num / denom;
}

public float GeometrySmith(float3 N, float3 V, float3 L, float roughness) {
  float NdotV = max(dot(N, V), 0.0);
  float NdotL = max(dot(N, L), 0.0);
  float ggx2 = GeometrySchlickGGX(NdotV, roughness);
  float ggx1 = GeometrySchlickGGX(NdotL, roughness);

  return ggx1 * ggx2;
}
}
