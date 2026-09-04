implementing keptech;

public static const float PI = 3.14159265359;

public static const float SRGB_TO_LINEAR_FACTOR = 2.2;
public static const float LINEAR_TO_SRGB_FACTOR = 1.0 / SRGB_TO_LINEAR_FACTOR;

public static const float3 UP = float3(0, -1, 0);
public static const float3 RIGHT = float3(1, 0, 0);
public static const float3 FORWARD = float3(0, 0, 1);

public float4 srgbToLinear(float4 color) { return float4(pow(color.rgb, float3(SRGB_TO_LINEAR_FACTOR)), color.a); }

public float4 linearToSrgb(float4 color) { return float4(pow(color.rgb, float3(LINEAR_TO_SRGB_FACTOR)), color.a); }

static const float2 INVERSE_ATAN = float2(0.1591, 0.3183);

public float2 equirectangularFromSpherical(float3 direction) {
  float2 uv = float2(atan2(direction.z, direction.x), asin(direction.y));
  uv *= INVERSE_ATAN;
  uv += 0.5;
  return uv;
}
