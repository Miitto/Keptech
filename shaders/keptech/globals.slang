[vk::binding(0, 0)]
ConstantBuffer<keptech::Camera> camera : register(b0);

[vk::binding(0, 1)]
StructuredBuffer<DescriptorHandle<Texture2D>> textures;
