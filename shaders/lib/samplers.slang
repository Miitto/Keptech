implementing keptech;

namespace kt::samplers {
public property SamplerState linearClamp {
  get { return SamplerState.Handle(uint2(0, 0)); }
}

public property SamplerState linearRepeat {
  get { return SamplerState.Handle(uint2(1, 0)); }
}

public property SamplerState linearMirror {
  get { return SamplerState.Handle(uint2(2, 0)); }
}

public property SamplerState linearMipmapClamp {
  get { return SamplerState.Handle(uint2(3, 0)); }
}

public property SamplerState linearMipmapRepeat {
  get { return SamplerState.Handle(uint2(4, 0)); }
}

public property SamplerState linearMipmapMirror {
  get { return SamplerState.Handle(uint2(5, 0)); }
}

public property SamplerState nearestClamp {
  get { return SamplerState.Handle(uint2(6, 0)); }
}

public property SamplerState nearestRepeat {
  get { return SamplerState.Handle(uint2(7, 0)); }
}

public property SamplerState nearestMirror {
  get { return SamplerState.Handle(uint2(8, 0)); }
}
}
