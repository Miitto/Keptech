implementing keptech;

public namespace kt {
public struct Plane {
  float3 _normal;
  float _distance;

  public __init(float3 normal, float distance) {
    let length = length(normal);
    _normal = normal / length;
    _distance = distance / length;
  }

  float distance(float3 point) { return dot(_normal, point) + _distance; }

  bool intersects(Sphere sphere) { return distance(sphere.center) >= -sphere.radius; }

  property float3 normal {
    get { return _normal; }
  }

  property float distance {
    get { return _distance; }
  }
}

public struct Frustum {
  Plane planes[6]; // Left, Right, Bottom, Top, Near, Far

  public __init(float4x4 vpMatrix) {
    float3 x = { vpMatrix[0][0], vpMatrix[1][0], vpMatrix[2][0] };
    float3 y = { vpMatrix[0][1], vpMatrix[1][1], vpMatrix[2][1] };
    float3 z = { vpMatrix[0][2], vpMatrix[1][2], vpMatrix[2][2] };
    float3 w = { vpMatrix[0][3], vpMatrix[1][3], vpMatrix[2][3] };

    float xd = vpMatrix[3][0];
    float yd = vpMatrix[3][1];
    float zd = vpMatrix[3][2];
    float d = vpMatrix[3][3];

    planes[0] = Plane(w - x, d - xd);
    planes[1] = Plane(w + x, d + xd);
    planes[2] = Plane(w - y, d - yd);
    planes[3] = Plane(w + y, d + yd);
    planes[4] = Plane(w - z, d - zd);
    planes[5] = Plane(w + z, d + zd);
  }

  bool intersects(Sphere sphere) {
    for (int i = 0; i < 6; ++i) {
      if (!planes[i].intersects(sphere)) {
        return false;
      }
    }
    return true;
  }
}
}
