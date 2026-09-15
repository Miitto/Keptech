implementing keptech;

public static const float PI = 3.14159265359;

public static const float SRGB_TO_LINEAR_FACTOR = 2.2;
public static const float LINEAR_TO_SRGB_FACTOR = 1.0 / SRGB_TO_LINEAR_FACTOR;

public static const float3 UP = float3(0, -1, 0);
public static const float3 RIGHT = float3(1, 0, 0);
public static const float3 FORWARD = float3(0, 0, 1);

public float4 srgbToLinear(float4 color) { return float4(pow(color.rgb, float3(SRGB_TO_LINEAR_FACTOR)), color.a); }

public float4 linearToSrgb(float4 color) { return float4(pow(color.rgb, float3(LINEAR_TO_SRGB_FACTOR)), color.a); }
public float3 srgbToLinear(float3 color) { return pow(color, float3(SRGB_TO_LINEAR_FACTOR)); }
public float3 linearToSrgb(float3 color) { return pow(color, float3(LINEAR_TO_SRGB_FACTOR)); }

static const float2 INVERSE_ATAN = float2(0.1591, 0.3183);

public float2 equirectangularFromSpherical(float3 direction) {
  float2 uv = float2(atan2(direction.z, direction.x), asin(direction.y));
  uv *= INVERSE_ATAN;
  uv += 0.5;
  return uv;
}

public float radicalInverseVdC(uint bits) {
  bits = (bits << 16u) | (bits >> 16u);
  bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
  bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
  bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
  bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
  return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

public float2 hammersley(uint i, uint N) { return float2(float(i) / float(N), radicalInverseVdC(i)); }

public float luminance(float3 color) { return dot(color, float3(0.299, 0.587, 0.114)); }
