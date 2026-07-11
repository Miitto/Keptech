implementing keptech;

namespace kt {

public enum Target : uint {
  Albedo,
  Normal,
  EmissiveAo,
  MetallicRoughness,
  Depth,
  Diffuse,
  Specular,
  SsaoResult,
  SsaoNoise,
  SsaoBlur,
  Combine,
  MAX_TARGETS
};

[[vk::binding(2, 1)]]
Sampler2D targets[Target::MAX_TARGETS];

public Sampler2D getTarget(Target target) { return targets[(uint)target]; }
}
