module keptech;

__include camera;

public namespace keptech {
  public struct DeferredOutput {
      public float4 albedo   : SV_Target0;
      public float4 normal   : SV_Target1;
  };
}
