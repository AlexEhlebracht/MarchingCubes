#version 330 core

in vec3 vWorldPos;
in vec3 vNormal;

out vec4 FragColor;

uniform vec3 camPos;   /* camera position in world */
uniform vec3 lightDir; /* normalized */

const vec3 BASE_CLR   = vec3(0.04, 0.30, 0.70); /* deep blue */
const vec3 FRESNEL_CLR= vec3(0.80, 0.90, 1.00); /* edge tint */

float fresnel(vec3 eyeDir, vec3 n)
{
    float f = 1.0 - clamp(dot(eyeDir, n), 0.0, 1.0);
    return pow(f, 5.0);      /* Schlick */
}

void main()
{
    vec3 n = normalize(vNormal);
    vec3 eyeDir = normalize(camPos - vWorldPos);

    /* simple top-light */
    float ndotl = clamp(dot(n, normalize(lightDir)), 0.0, 1.0);
    vec3 color = BASE_CLR * mix(0.5, 1.0, ndotl);

    /* Fresnel tint so color shifts with view angle */
    color = mix(color, FRESNEL_CLR, fresnel(eyeDir, n) * 0.3);

    FragColor = vec4(color, 0.80);  /* 80 % alpha */
}
