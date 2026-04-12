import keptech;

struct v2g {
  float3 worldPosition : POSITION;
};

struct g2f {
  float4 position : SV_Position;
  float3 worldPosition : POSITION;
  int layer : SV_RenderTargetArrayIndex;
};

struct ShadowMatrices {
  float4x4 m[6];
};

struct PushConstants {
  float4x4 modelMatrix;
  float3 lightPos;
  float lightRadius;
  ShadowMatrices *matrices;
};

[shader("vertex")]
v2g vert(kt::Vertex v, uniform PushConstants PC) {
  v2g output;

  output.worldPosition = mul(float4(v.position, 1.0), PC.modelMatrix).xyz;

  return output;
}

[shader("geometry")]
[maxvertexcount(3)]
[instance(6)]
void geom(triangle v2g input[3], uniform PushConstants PC, uint layer: SV_GSInstanceID, inout TriangleStream<g2f> outputStream) {
  for (uint vertex = 0; vertex < 3; vertex++) {
    g2f output;
    output.layer = layer;
    output.worldPosition = input[vertex].worldPosition;
    output.position = mul(float4(output.worldPosition, 1.0), PC.matrices.m[layer]);
    outputStream.Append(output);
  }
  outputStream.RestartStrip();
}

[shader("pixel")]
float frag(g2f input, uniform PushConstants PC) : SV_Depth {
  float lightDistance = distance(input.worldPosition, PC.lightPos);
  lightDistance /= PC.lightRadius;

  return lightDistance;
}
