implementing keptech;

public namespace kt {
public enum class PrimitiveTopology : uint8_t {
  TriangleList,
  TriangleStrip,
};

public enum class BlendFactor : uint8_t {
  Zero,
  One,
  SrcColor,
  OneMinusSrcColor,
  DstColor,
  OneMinusDstColor,
  SrcAlpha,
  OneMinusSrcAlpha,
  DstAlpha,
  OneMinusDstAlpha,
};

public enum class CullMode : uint8_t {
  None,
  Front,
  Back,
};
}

[__AttributeUsage(_AttributeTargets.Function)]
struct topologyAttribute {
  kt::PrimitiveTopology value;
};

[__AttributeUsage(_AttributeTargets.Function)]
struct blendAttribute {
  kt::BlendFactor src;
  kt::BlendFactor dst;
};

[__AttributeUsage(_AttributeTargets.Function)]
struct cullAttribute {
  kt::CullMode value;
};

[__AttributeUsage(_AttributeTargets.Function)]
struct depthWriteAttribute {
  bool value;
};

[__AttributeUsage(_AttributeTargets.Param)]
struct vertexBufferAttribute {
  uint value;
};

[__AttributeUsage(_AttributeTargets.Var)]
struct pushAttribute {};
