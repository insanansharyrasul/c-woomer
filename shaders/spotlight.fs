#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Custom uniforms
uniform vec4 spotlightTint;
uniform vec2 cursorPosition;
uniform float spotlightRadiusMultiplier;

// Output fragment color
out vec4 finalColor;

void main()
{
    // Get the base texture color
    vec4 texelColor = texture(texture0, fragTexCoord);
    
    // Calculate distance from current fragment to cursor position
    vec2 fragPos = gl_FragCoord.xy;
    float distance = length(fragPos - cursorPosition);
    
    // Calculate spotlight radius (you can adjust this formula)
    float baseRadius = 150.0;
    float spotlightRadius = baseRadius * spotlightRadiusMultiplier;
    
    // Calculate spotlight effect
    float spotlightEffect = 1.0 - smoothstep(0.0, spotlightRadius, distance);
    
    // Mix between the tinted version and the original texture
    vec4 tintedColor = mix(spotlightTint, texelColor, spotlightEffect);
    
    finalColor = tintedColor * colDiffuse * fragColor;
}
