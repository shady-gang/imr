vec2 hash( vec2 p ) // replace this by something better
{
	p = vec2( dot(p,vec2(127.1,311.7)), dot(p,vec2(269.5,183.3)) );
        p = vec2(sinf(p.x), sinf(p.y));
        p = p * vec2(43758.5453123, 43758.5453123);
        vec2 q = floor(p);
        p = p - q;
	return p * vec2(2.0, 2.0) - vec2(1.0, 1.0);
}

float noise(vec2 p)
{
    const float K1 = 0.366025404; // (sqrt(3)-1)/2;
    const float K2 = 0.211324865; // (3-sqrt(3))/6;

    vec2  i = floor( p + (p.x+p.y)*K1 );
    vec2  a = p - i + (i.x+i.y)*K2;
    float m = step(a.y, a.x);
    vec2  o = vec2(m, 1.0f - m);
    vec2  b = a - o + K2;
    vec2  c = a - 1.0 + 2.0 * K2;
    vec3  h = max(vec3(0.5) - vec3(dot(a,a), dot(b,b), dot(c,c)), vec3(0.0));
    vec3  n = h*h*h*h*vec3( dot(a,hash(i+0.0)), dot(b,hash(i+o)), dot(c,hash(i+1.0)));
    return dot( n, vec3(70.0) );
}

vec2 matmulnomat(vec2 m1, vec2 m2, vec2 uv) {
    float x = dot(m1, uv);
    float y = dot(m2, uv);

    return vec2(x, y);
}

float perlin_noise(vec2 uv) {
    vec2 uv1 = uv;
    float f = 0.0f;
    uv = uv * 0.5f;
    //mat2 m = mat2( 1.6,  1.2, -1.2,  1.6 );
    vec2 m1 = vec2 (1.6, 1.2);
    vec2 m2 = vec2 (-1.2, 1.6);
    float k = 1, c = 0.5;
    f += k*noise( uv ); uv = matmulnomat(m1, m2, uv); k *= c;
    f += k*noise( uv ); uv = matmulnomat(m1, m2, uv); k *= noise(uv1 + vec2(15314.151, 0.22415));
    f += k*noise( uv ); uv = matmulnomat(m1, m2, uv); k *= c;
    f += k*noise( uv ); uv = matmulnomat(m1, m2, uv); k *= c;
    f += k*noise( uv ); uv = matmulnomat(m1, m2, uv); k *= noise(uv1 + vec2(15314.151, 0.22415));
    return f;
}
