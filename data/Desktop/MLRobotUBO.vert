#version 420 core

#include "common/scene_ubo.glsl"


uniform mat4 model;

// =================================
// Vertex Attributes
// =================================
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTex;
layout(location = 3) in vec4 aColor;

// =================================
// Outputs
// =================================
out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vTexCoord;
out vec4 vVertexColor;

void main()
{
    vec4 worldPos = model * vec4(aPos, 1.0);

    vWorldPos = worldPos.xyz;
    vNormal   = mat3(transpose(inverse(model))) * aNormal;
    vTexCoord = aTex;
    vVertexColor = aColor;

    gl_Position = projection * view * worldPos;
}
