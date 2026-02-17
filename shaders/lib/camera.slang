implementing keptech;

public namespace keptech {
public struct Camera {
  float4x4 projectionMatrix;
  float4x4 viewMatrix;
  float4x4 viewProjectionMatrix;

  float4x4 inverseProjectionMatrix;
  float4x4 inverseViewMatrix;
  float4x4 inverseViewProjectionMatrix;
  float2 viewportSizeV;

  public float4 worldToCamera(float4 worldPosition) {
    return mul(worldPosition, viewMatrix);
  }

  public float4 cameraToWorld(float4 cameraPosition) {
    return mul(cameraPosition, inverseViewMatrix);
  }

  public float4 cameraToClip(float4 cameraPosition) {
    return mul(cameraPosition, projectionMatrix);
  }

  public float4 clipToCamera(float4 clipPosition) {
    return mul(clipPosition, inverseProjectionMatrix);
  }

  public float4 worldToClip(float4 worldPosition) {
    return mul(worldPosition, viewProjectionMatrix);
  }

  public float4 clipToWorld(float4 clipPosition) {
    return mul(clipPosition, inverseViewProjectionMatrix);
  }

  public property float3 position {
    get { return inverseViewMatrix[3].xyz; }
  }

  public property float2 viewportSize {
    get { return viewportSizeV; }
  }

  public float3 forward() { return normalize(-inverseViewMatrix[2].xyz); }

  public float3 up() { return normalize(inverseViewMatrix[1].xyz); }

  public float3 right() { return normalize(inverseViewMatrix[0].xyz); }
};
}

