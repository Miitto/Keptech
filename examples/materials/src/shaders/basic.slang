import keptech;

#include "keptech/cameraUniform.slang"

struct Vertex {
  float3 position;
  float uvX;
  float3 normal;
  float uvY;
  float4 color;
  float4 tangent;
};

[vk::push_constant]
uniform Vertex* vBuffer;

struct VertexOutput
{
    float4 position : SV_Position;
    float4 color : COLOR;
};

[shader("vertex")]
VertexOutput vert(uint vid: SV_VertexID) {
  VertexOutput output;

  Vertex v = vBuffer[vid];

  output.position = camera.worldToClip(float4(v.position, 1.0));
  output.color = v.color;

  return output;
}

[shader("pixel")]
float4 frag(VertexOutput input) : SV_Target {
    return input.color;
}
