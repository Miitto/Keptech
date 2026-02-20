implementing keptech;

public static const float PI = 3.14159265359;

public static const float SRGB_TO_LINEAR_FACTOR = 2.2;
public static const float LINEAR_TO_SRGB_FACTOR = 1.0 / SRGB_TO_LINEAR_FACTOR;

public float4 srgbToLinear(float4 color) {
  return float4(pow(color.rgb, float3(SRGB_TO_LINEAR_FACTOR)), color.a);
}

public float4 linearToSrgb(float4 color) {
  return float4(pow(color.rgb, float3(LINEAR_TO_SRGB_FACTOR)), color.a);
}
