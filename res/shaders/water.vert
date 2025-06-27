#version 330 core

layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float uTime;

/* -------- tweak here ---------- */
const float AMP  = 0.6;   /* wave height        */
const float FREQ = 0.10;  /* spatial frequency  */
const float SPD  = 1.0;   /* speed (units/sec)  */
/* ------------------------------ */

out vec3 vWorldPos;
out vec3 vNormal;

void main()
{
    vec4 wp = model * vec4(aPos, 1.0);   /* world position before waves */

    float phaseX = wp.x * FREQ + uTime * SPD;
    float phaseZ = wp.z * FREQ + uTime * SPD;

    float height = sin(phaseX) * cos(phaseZ) * AMP;
    wp.y += height;

    /* analytical normal for y = f(x,z) */
    float dHdX = AMP * FREQ * cos(phaseX) * cos(phaseZ);
    float dHdZ = -AMP * FREQ * sin(phaseX) * sin(phaseZ);
    vNormal = normalize(vec3(-dHdX, 1.0, -dHdZ));

    vWorldPos = wp.xyz;
    gl_Position = projection * view * wp;
}
