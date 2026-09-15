#language slang 2026

import keptech;

[shader("vertex")]
[cull(kt::CullMode::None)]
kt::PosUv vert(uint vid: SV_VertexID) { return kt::fullscreenTri(vid); }

ConstantBuffer<kt::Camera> camera;

Texture2D<float4> srcTex;
Options options;

struct Options {
  float fixedLumaThreshold;
  float relativeLumaThreshold;
  float blendFactor;
  bool lumaInAlpha;
};

struct LumaNeighborhood {
  float m, n, e, s, w, ne, nw, se, sw;
  float h, l, r;

  __init(float m, float n, float e, float s, float w, float ne, float nw, float se, float sw) {
    this.m = m;
    this.n = n;
    this.e = e;
    this.s = s;
    this.w = w;
    this.ne = ne;
    this.nw = nw;
    this.se = se;
    this.sw = sw;

    this.h = max(max(max(max(m, n), e), s), w);
    this.l = min(min(min(min(m, n), e), s), w);

    this.r = h - l;
  }
}

struct Edge {
  bool horizontal;
  float pixelStep;

  __init(LumaNeighborhood n, float2 texelSize) {
    float horizontal = 2.0 * abs(n.n + n.s - 2.0 * n.m) + abs(n.ne + n.se - 2.0 * n.e) + abs(n.nw + n.sw - 2.0 * n.w);
    float vertical = 2.0 * abs(n.e + n.w - 2.0 * n.m) + abs(n.ne + n.nw - 2.0 * n.n) + abs(n.se + n.sw - 2.0 * n.s);
    this.horizontal = horizontal > vertical;
    float lp, ln;
    if (this.horizontal) {
      this.pixelStep = texelSize.y;
      lp = n.n;
      ln = n.s;
    } else {
      this.pixelStep = texelSize.x;
      lp = n.e;
      ln = n.w;
    }

    float gp = abs(lp - n.m);
    float gn = abs(ln - n.m);

    if (gp > gn) {
      this.pixelStep = -this.pixelStep;
    }
  }
};

bool canSkip(LumaNeighborhood n) { return n.r < max(options.fixedLumaThreshold, options.relativeLumaThreshold * n.h); }

[shader("pixel")]
float4 frag(kt::PosUv input) : SV_Target {
  let sampler = kt::samplers::linearClamp;

  uint width, height;
  srcTex.GetDimensions(width, height);

  let invWidth = 1.0 / float(width);
  let invHeight = 1.0 / float(height);

  let texelSize = float2(invWidth, invHeight);

  let lumaNeighborhood =
      options.lumaInAlpha ? getAlphaLumaNeighborhood(sampler, input.uv, texelSize) : getGreenLumaNeighborhood(sampler, input.uv, texelSize);

  let r = lumaNeighborhood.r;

  if (canSkip(lumaNeighborhood)) {
    return srcTex.SampleLevel(sampler, input.uv, 0);
  }

  let filter = subpixelBlendFactor(lumaNeighborhood);
  let edge = Edge(lumaNeighborhood, texelSize);

  var bUv = input.uv;
  bUv += filter * (edge.horizontal ? float2(0.0, edge.pixelStep) : float2(edge.pixelStep, 0.0));

  return srcTex.SampleLevel(sampler, bUv, 0);
}

float subpixelBlendFactor(LumaNeighborhood n) {
  float filter = 2. * (n.n + n.e + n.s + n.w);
  filter += n.ne + n.nw + n.se + n.sw;
  filter *= 1.f / 12.f;
  filter = abs(filter - n.m);
  filter = saturate(filter / n.r);
  filter = smoothstep(0.0, 1.0, filter);
  return filter * filter * options.blendFactor;
}

LumaNeighborhood getAlphaLumaNeighborhood(SamplerState sampler, float2 uv, float2 texelSize) {
  let m = srcTex.SampleLevel(sampler, uv, 0).a;
  let n = srcTex.SampleLevel(sampler, uv + float2(0.0, -texelSize.y), 0).a;
  let e = srcTex.SampleLevel(sampler, uv + float2(texelSize.x, 0.0), 0).a;
  let s = srcTex.SampleLevel(sampler, uv + float2(0.0, texelSize.y), 0).a;
  let w = srcTex.SampleLevel(sampler, uv + float2(-texelSize.x, 0.0), 0).a;
  let ne = srcTex.SampleLevel(sampler, uv + float2(texelSize.x, -texelSize.y), 0).a;
  let nw = srcTex.SampleLevel(sampler, uv + float2(-texelSize.x, -texelSize.y), 0).a;
  let se = srcTex.SampleLevel(sampler, uv + float2(texelSize.x, texelSize.y), 0).a;
  let sw = srcTex.SampleLevel(sampler, uv + float2(-texelSize.x, texelSize.y), 0).a;

  return { m, n, e, s, w, ne, nw, se, sw };
}

LumaNeighborhood getGreenLumaNeighborhood(SamplerState sampler, float2 uv, float2 texelSize) {
  let m = srcTex.SampleLevel(sampler, uv, 0).g;
  let n = srcTex.SampleLevel(sampler, uv + float2(0.0, -texelSize.y), 0).g;
  let e = srcTex.SampleLevel(sampler, uv + float2(texelSize.x, 0.0), 0).g;
  let s = srcTex.SampleLevel(sampler, uv + float2(0.0, texelSize.y), 0).g;
  let w = srcTex.SampleLevel(sampler, uv + float2(-texelSize.x, 0.0), 0).g;
  let ne = srcTex.SampleLevel(sampler, uv + float2(texelSize.x, -texelSize.y), 0).g;
  let nw = srcTex.SampleLevel(sampler, uv + float2(-texelSize.x, -texelSize.y), 0).g;
  let se = srcTex.SampleLevel(sampler, uv + float2(texelSize.x, texelSize.y), 0).g;
  let sw = srcTex.SampleLevel(sampler, uv + float2(-texelSize.x, texelSize.y), 0).g;

  return { m, n, e, s, w, ne, nw, se, sw };
}
