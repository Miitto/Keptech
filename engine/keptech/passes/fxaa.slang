#language slang 2026

import keptech;

[shader("vertex")]
[cull(kt::CullMode::None)]
kt::PosUv vert(uint vid: SV_VertexID) { return kt::fullscreenTri(vid); }

ConstantBuffer<kt::Camera> camera;

Texture2D<float4> srcTex;
Options options;

#if defined(FXAA_QUALITY_LOW)
#define EXTRA_EDGE_STEPS 3
#define EDGE_STEP_SIZES 1.5, 2.0, 2.0
#define LAST_EDGE_STEP_GUESS 8.0
#elif defined(FXAA_QUALITY_MEDIUM)
#define EXTRA_EDGE_STEPS 8
#define EDGE_STEP_SIZES 1.5, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 4.0
#define LAST_EDGE_STEP_GUESS 8.0
#else
#define EXTRA_EDGE_STEPS 10
#define EDGE_STEP_SIZES 1.0, 1.0, 1.0, 1.0, 1.5, 2.0, 2.0, 2.0, 2.0, 4.0
#define LAST_EDGE_STEP_GUESS 8.0
#endif

static const float edgeStepSizes[EXTRA_EDGE_STEPS] = { EDGE_STEP_SIZES };

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
  float gradient, other;

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

    if (gp < gn) {
      this.pixelStep = -this.pixelStep;
      this.gradient = gn;
      this.other = ln;
    } else {
      this.gradient = gp;
      this.other = lp;
    }
  }

  float edgeBlendFactor(LumaNeighborhood n, float2 uv, float2 texelSize) {
    var edgeUv = uv;
    float2 uvStep = 0.0;
    if (horizontal) {
      edgeUv.y += 0.5 * pixelStep;
      uvStep.x = texelSize.x;
    } else {
      edgeUv.x += 0.5 * pixelStep;
      uvStep.y = texelSize.y;
    }

    float edgeLuma = 0.5 * (n.m + other);
    float gradThreshold = 0.25 * gradient;

    let sampler = kt::samplers::linearClamp;

    float2 uvP = edgeUv + uvStep;
    float deltaP = (options.lumaInAlpha ? srcTex.SampleLevel(sampler, uvP, 0).a : srcTex.SampleLevel(sampler, uvP, 0).g) - edgeLuma;
    bool atEdgeP = abs(deltaP) >= gradThreshold;

    [[ForceUnroll]]
    for (int i = 0; i < EXTRA_EDGE_STEPS && !atEdgeP; i++) {
      uvP += uvStep * edgeStepSizes[i];
      deltaP = (options.lumaInAlpha ? srcTex.SampleLevel(sampler, uvP, 0).a : srcTex.SampleLevel(sampler, uvP, 0).g) - edgeLuma;
      atEdgeP = abs(deltaP) >= gradThreshold;
    }
    if (!atEdgeP) {
      uvP += uvStep * LAST_EDGE_STEP_GUESS;
    }

    float2 uvN = edgeUv - uvStep;
    float deltaN = (options.lumaInAlpha ? srcTex.SampleLevel(sampler, uvN, 0).a : srcTex.SampleLevel(sampler, uvN, 0).g) - edgeLuma;
    bool atEdgeN = abs(deltaN) >= gradThreshold;

    [[ForceUnroll]]
    for (int i = 0; i < EXTRA_EDGE_STEPS && !atEdgeN; i++) {
      uvN -= uvStep * edgeStepSizes[i];
      deltaN = (options.lumaInAlpha ? srcTex.SampleLevel(sampler, uvN, 0).a : srcTex.SampleLevel(sampler, uvN, 0).g) - edgeLuma;
      atEdgeN = abs(deltaN) >= gradThreshold;
    }
    if (!atEdgeN) {
      uvN -= uvStep * LAST_EDGE_STEP_GUESS;
    }

    float distanceToEndP, distanceToEndN;
    if (horizontal) {
      distanceToEndP = uvP.x - uv.x;
      distanceToEndN = uv.x - uvN.x;
    } else {
      distanceToEndP = uvP.y - uv.y;
      distanceToEndN = uv.y - uvN.y;
    }

    float distanceToEnd;
    bool deltaSign;
    if (distanceToEndP <= distanceToEndN) {
      distanceToEnd = distanceToEndP;
      deltaSign = deltaP >= 0.0;
    } else {
      distanceToEnd = distanceToEndN;
      deltaSign = deltaN >= 0.0;
    }

    if (deltaSign == (n.m - edgeLuma >= 0)) {
      return 0.0;
    } else {
      return 0.5 - distanceToEnd / (distanceToEndP + distanceToEndN);
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

  let edge = Edge(lumaNeighborhood, texelSize);
  let filter = max(subpixelBlendFactor(lumaNeighborhood), edge.edgeBlendFactor(lumaNeighborhood, input.uv, texelSize));

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
