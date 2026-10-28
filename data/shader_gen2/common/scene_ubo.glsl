// std140 layout matches RenderCore/SceneUBOData.h (SDK 1.0.6).
#ifndef RS2026_SCENE_UBO_GLSL
#define RS2026_SCENE_UBO_GLSL
#define MAX_LIGHTS 12
struct GPULight {
    vec4 position;
    vec4 direction;
    vec4 color;
    vec4 params;
};
layout(std140, binding = 0) uniform SceneUBO {
    mat4 projection;
    mat4 view;
    vec4 cameraPos;
    ivec4 lightInfo;
    GPULight lights[MAX_LIGHTS];
};
#endif
