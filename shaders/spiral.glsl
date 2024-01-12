const float PI = 3.14159265359;

bool spiral( vec2 center, float cf, float sn, float dt, float t, vec2 uv ) {
	// center, radius, curvature factor, slice number, delta t
    t *= dt;
    uv -= center;
    float cr = length(uv);
    float rad = (atan(uv.y, uv.x) + PI + sin(cr) * cf + dt * t) / (2.0 * PI);
    return fract(rad * sn) <= 0.5;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord ) {
	float mn = min(iResolution.x, iResolution.y);
	vec2 uv = fragCoord.xy / mn - 0.5 * vec2(iResolution.x / mn, iResolution.y / mn);
    vec3 finalColor = spiral(vec2(0.0, 0.0), -100, 4, 0.8, iTime, uv) ? vec3(0.5, 0.2, 0.5) : vec3(0.8, 0.7, 0.8);
        
    fragColor = vec4(finalColor, 1.0);
}
