#version 120

// Input vertex attributes (from vertex shader)
varying vec2 fragTexCoord;
varying vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;     // Main water texture
uniform sampler2D rippleTex;    // 150x1 ripple heightmap
uniform vec4 colDiffuse;
uniform float waterLevel;       // Water height (0.0 to 1.0)

// Time-based animation
uniform float secondes;

// Wave parameters (for base animation)
uniform vec2 size;
uniform float freqX;
uniform float freqY;
uniform float ampX;
uniform float ampY;
uniform float speedX;
uniform float speedY;

void main() {
    float pixelWidth = 1.0 / size.x;
    float pixelHeight = 1.0 / size.y;
    float aspect = pixelHeight / pixelWidth;
    float boxLeft = 0.0;
    float boxTop = 0.0;

    vec2 uv = fragTexCoord;
    
    // Base wave distortion (existing behavior)
    uv.x += cos((fragTexCoord.y - boxTop) * freqX / (pixelWidth * 750.0) + (secondes * speedX)) * ampX * pixelWidth;
    uv.y += sin((fragTexCoord.x - boxLeft) * freqY * aspect / (pixelHeight * 750.0) + (secondes * speedY)) * ampY * pixelHeight;

    // Sample ripple heightmap (centered at 0.5, range [0, 1])
    float ripple = texture2D(rippleTex, vec2(fragTexCoord.x, 0.5)).r - 0.5;

    // Apply ripple distortion
    uv.y += ripple * 0.1;
    
    // Sample the main texture with distorted UV
    vec4 color = texture2D(texture0, uv);
    
    // Modulate brightness based on ripple height
    color.rgb += ripple * 0.25;
    
    // Calculate gradient for normal-like lighting effect
    float dx = 1.0 / 150.0;  // Step size in ripple texture
    float left = texture2D(rippleTex, vec2(fragTexCoord.x - dx, 0.5)).r - 0.5;
    float right = texture2D(rippleTex, vec2(fragTexCoord.x + dx, 0.5)).r - 0.5;
    float grad = (right - left);
    
    // Apply gradient-based shading
    color.rgb += grad * 0.1;
    
    gl_FragColor = color * colDiffuse * fragColor;
}
