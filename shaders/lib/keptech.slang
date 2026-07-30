module keptech;

__include camera;
__include fullscreen;
__include lighting;
__include maths;
__include vertex;
__include buffers;
__include material;
__include sphere;
__include frustum;
__include struct;

public namespace kt {

public struct DeferredOutput {
  public float4 albedo : SV_Target0;
  public float4 normal : SV_Target1;
  public float4 emissiveAo : SV_Target2;
  public float4 metallicRoughness : SV_Target3;
};

public struct DeferredLightingOutput {
  public float4 diffuse : SV_Target0;
  public float4 specular : SV_Target1;
};

public bool valid<T>(DescriptorHandle<T> handle)
    where T : IOpaqueDescriptor {
  __target_switch {
  case spirv: {
    uint64_t handleValue = reinterpret<uint64_t>(handle);
    return handleValue != ~0u;
  }
  default: {
    static_assert(false, "DescriptorHandle is only supported on SPIR-V targets.");
    return false;
  }
  }
}
}

public export T getDescriptorFromHandle<T>(DescriptorHandle<T> handle)
    where T : IOpaqueDescriptor { return defaultGetDescriptorFromHandle(handle, BindlessDescriptorOptions::None); }
