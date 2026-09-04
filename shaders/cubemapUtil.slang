import keptech;

static const float4x4 projectionMatrix =
    float4x4(float4(1.0, 0.0, 0.0, 0.0), float4(0.0, 1.0, 0.0, 0.0), float4(0.0, 0.0, 0, -1), float4(0.0, 0.0, 1, 1));
static const float3x3 viewMatricies[6] = {
  float3x3(float3(0, 0, 1), float3(0, -1, 0), float3(1, 0, 0)),  float3x3(float3(0, 0, -1), float3(0, -1, 0), float3(-1, 0, 0)),
  float3x3(float3(-1, 0, 0), float3(0, 0, 1), float3(0, 1, 0)),  float3x3(float3(-1, 0, 0), float3(0, 0, -1), float3(0, -1, 0)),
  float3x3(float3(-1, 0, 0), float3(0, -1, 0), float3(0, 0, 1)), float3x3(float3(1, 0, 0), float3(0, -1, 0), float3(0, 0, -1)),
};

struct CubeV2G {
  float4 position : SV_POSITION;
  float3 viewPosition : TEXCOORD0;
};

CubeV2G cubeVertex(uint vid) {
  CubeV2G output;
  float2 coord = float2((vid << 1) & 2, vid & 2);
  output.position = float4(float2(-1.0, 1.0) + coord * float2(2.0, -2.0), 0.0, 1.0);
  output.viewPosition = mul(projectionMatrix, output.position).xyz;
  return output;
}

struct CubeG2F {
  float4 position : SV_POSITION;
  float3 direction : TEXCOORD0;
  uint layer : SV_RenderTargetArrayIndex;
};

void cubeGeom(in CubeV2G input[3], uint layer, inout TriangleStream<CubeG2F> outputStream) {
  for (uint vertex = 0; vertex < 3; vertex++) {
    CubeG2F output;
    output.layer = layer;
    output.position = input[vertex].position;
    output.direction = mul(input[vertex].viewPosition, viewMatricies[layer]);
    outputStream.Append(output);
  }
  outputStream.RestartStrip();
}
