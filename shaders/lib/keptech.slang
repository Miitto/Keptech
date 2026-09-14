module keptech;

__include attribs;
__include buffers;
__include camera;
__include frustum;
__include lighting;
__include material;
__include maths;
__include mesh;
__include samplers;
__include sphere;
__include struct;
__include vertex;

public namespace kt {

public struct DeferredOutput {
  public float4 albedo : SV_Target0;
  public float4 normal : SV_Target1;
  public float4 metallicRoughness : SV_Target2;
  public float4 emissiveAo : SV_Target3;
};

public bool valid<T>(DescriptorHandle<T> handle)
    where T : IOpaqueDescriptor {
  __target_switch {
  case spirv: {
    uint64_t handleValue = reinterpret<uint64_t>(handle);
    return handleValue != ~0u;
  }
  case hlsl: {
    uint64_t handleValue = reinterpret<uint64_t>(handle);
    return handleValue != ~0u;
  }
  default: {
    static_assert(false, "DescriptorHandle is only supported on SPIR-V targets.");
    return false;
  }
  }
}

public struct PosUv {
  public float4 position : SV_Position;
  public float2 uv : TEXCOORD0;
};

public PosUv fullscreenTri(uint vid) {
  float2 coord = float2((vid << 1) & 2, vid & 2);
  PosUv output = { float4(float2(-1.0, 1.0) + coord * float2(2.0, -2.0), 0.0, 1.0), coord };
  return output;
}
}

public export T getDescriptorFromHandle<T>(DescriptorHandle<T> handle)
    where T : IOpaqueDescriptor { return defaultGetDescriptorFromHandle(handle, BindlessDescriptorOptions::None); }
