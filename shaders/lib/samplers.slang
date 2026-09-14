implementing keptech;

namespace kt::samplers {
public property SamplerState.Handle linearClamp {
  get { return SamplerState.Handle(uint2(0, 0)); }
}

public property SamplerState.Handle linearRepeat {
  get { return SamplerState.Handle(uint2(1, 0)); }
}

public property SamplerState.Handle linearMirror {
  get { return SamplerState.Handle(uint2(2, 0)); }
}

public property SamplerState.Handle linearMipmapClamp {
  get { return SamplerState.Handle(uint2(3, 0)); }
}

public property SamplerState.Handle linearMipmapRepeat {
  get { return SamplerState.Handle(uint2(4, 0)); }
}

public property SamplerState.Handle linearMipmapMirror {
  get { return SamplerState.Handle(uint2(5, 0)); }
}

public property SamplerState.Handle nearestClamp {
  get { return SamplerState.Handle(uint2(6, 0)); }
}

public property SamplerState.Handle nearestRepeat {
  get { return SamplerState.Handle(uint2(7, 0)); }
}

public property SamplerState.Handle nearestMirror {
  get { return SamplerState.Handle(uint2(8, 0)); }
}
}
