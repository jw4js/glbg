const float PI = 3.14159265359;

bool spiral( vec2 center, float cf, float sn, float dt, float t, vec2 uv ) {
	// center, radius, curvature factor, slice number, delta t
    t *= dt;
    uv -= center;
    float cr = length(uv);
    float rad = (atan(uv.y, uv.x) + PI + sin(cr) * cf + dt * t) / (2.0 * PI);
    return fract(rad * sn) <= 0.5;
}

vec2 kalei( vec2 center, float sn, vec2 uv ) {
	uv -= center;
	float rad = (atan(uv.y, uv.x) + PI) / (2.0 * PI);
	float c = rad * sn;
	float rot = fract(c);
	if((int(c) % 2) == 0) {
		// return vec2(0, 0);
		rot = 1 - rot;
	}
	rot *= 2 * PI / sn;
	float r = length(uv);
	return vec2(r * sin(rot), r * cos(rot)) + center;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord ) {
	float mn = min(iResolution.x, iResolution.y);
	vec2 uv = fragCoord.xy / mn - 0.5 * vec2(iResolution.x / mn, iResolution.y / mn);
	uv = kalei(vec2(0, 0), 12.0, uv);
	uv = kalei(vec2(0.1 * sin(iTime * 0.05), 0.0), 7, uv);
    vec3 finalColor;
    vec3 c1 = spiral(vec2(-0.3, 0.1), -10.1, 8, 0.1, iTime, uv) ? vec3(0.3 * sin(0.7 * iTime) + 0.2, 0.2, 0.5) : vec3(0.2, 0.7, 0.3 + 0.3 * cos(iTime));
    vec3 c2 = spiral(vec2(0.3, 0.0), -7.1, 4, 0.3, iTime, uv) ? vec3(0.4, 0.1, 0.8) : vec3(0.2, 0.5, 0.5);
    vec3 c3 = spiral(vec2(0.3, 0.5), -30.1, 3, 0.05, iTime, uv) ? vec3(0.8, 0.9, 0.9) : vec3(0.3, 0.6, 0.5);
    finalColor = min(c1, c2 * c3);
        
    fragColor = vec4(finalColor, 1.0);
}
