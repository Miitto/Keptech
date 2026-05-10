import keptech;

struct v2g {
  float3 worldPosition : POSITION;
};

struct g2f {
  float4 position : SV_Position;
  float3 worldPosition : POSITION;
  int layer : SV_RenderTargetArrayIndex;
};

struct Object {
  float4x4 modelMatrix;
  uint materialIndex;
  float padding[3];
};

struct PushConstants {
  Object *objectBuffer;
  kt::PointLight *lightBuffer;
  float4x4 *shadowMatrixBuffer;
  uint lightIndex;
  uint shadowMapIndex;
};

[shader("vertex")]
v2g vert(float3 pos, uint iid: SV_InstanceID, uint bid: SV_StartInstanceLocation, uniform PushConstants PC) {
  v2g output;

  Object obj = PC.objectBuffer[iid + bid];

  output.worldPosition = mul(float4(pos, 1.0), obj.modelMatrix).xyz;

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
    output.position = mul(float4(output.worldPosition, 1.0), PC.shadowMatrixBuffer[PC.shadowMapIndex + layer]);
    outputStream.Append(output);
  }
  outputStream.RestartStrip();
}

[shader("pixel")]
float frag(g2f input, uniform PushConstants PC) : SV_Depth {
  kt::PointLight light = PC.lightBuffer[PC.lightIndex];
  float lightDistance = distance(input.worldPosition, light.position);

  lightDistance /= light.radius;

  return lightDistance;
}
