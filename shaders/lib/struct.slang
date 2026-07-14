implementing keptech;

public namespace kt {
public struct DrawMeshTasksIndirectCommand {
  public __init() {
    x = 0;
    y = 0;
    z = 0;
  }

  public __init(uint32_t x, uint32_t y, uint32_t z) {
    this.x = x;
    this.y = y;
    this.z = z;
  }

  public uint32_t x;
  public uint32_t y;
  public uint32_t z;
};
}
