#version 330 core

in vec3 FragPos;
in vec3 Normal;
in vec3 Color;

out vec4 FragColor;

uniform vec3 lightDir; // Directional light
uniform vec3 viewPos;

void main()
{
    vec3 norm = normalize(Normal);
    vec3 lightDirNorm = normalize(-lightDir);
    
    // Improved lighting calculation
    float diff = max(dot(norm, lightDirNorm), 0.0);
    
    vec3 ambient = vec3(0.3);
    vec3 diffuse = diff * vec3(1.0);
    
    // Simple rim lighting
    vec3 viewDir = normalize(viewPos - FragPos);

    float noise = fract(sin(dot(FragPos.xy ,vec2(12.9898,78.233))) * 43758.5453);
    vec3 dither = vec3(noise * 0.005); // tweak strength
    
    vec3 result = (ambient + diffuse) * Color + dither;
    FragColor = vec4(result, 1.0);
}