implementing keptech;


public namespace keptech {
  public struct Camera {
    float4x4 projectionMatrix;
    float4x4 viewMatrix;
    float4x4 viewProjectionMatrix;

    float4x4 inverseProjectionMatrix;
    float4x4 inverseViewMatrix;
    float4x4 inverseViewProjectionMatrix;

    public float4 worldToCamera(float4 worldPosition) {
      return mul(viewMatrix, worldPosition);
    }

    public float4 cameraToWorld(float4 cameraPosition) {
      return mul(inverseViewMatrix, cameraPosition);
    }

    public float4 cameraToClip(float4 cameraPosition) {
      return mul(projectionMatrix, cameraPosition);
    }

    public float4 clipToCamera(float4 clipPosition) {
      return mul(inverseProjectionMatrix, clipPosition);
    }

    public float4 worldToClip(float4 worldPosition) {
      return mul(viewProjectionMatrix, worldPosition);
    }

    public float4 clipToWorld(float4 clipPosition) {
      return mul(inverseViewProjectionMatrix, clipPosition);
    }

    public float3 position() {
        return inverseViewMatrix[3].xyz;
    }

    public float3 forward() {
        return normalize(-inverseViewMatrix[2].xyz);
    }

    public float3 up() {
        return normalize(inverseViewMatrix[1].xyz);
    }

    public float3 right() {
        return normalize(inverseViewMatrix[0].xyz);
    }
  };
}

