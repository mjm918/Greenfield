#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;

uniform sampler2D texture0;
uniform vec2 sceneSize;
uniform float clockTime;
uniform float animationAmount;
uniform int actorCount;
uniform vec4 actorBounds[20]; // x, y, width, height in reference pixels
uniform vec4 actorMotion[20]; // speed, phase, strength, gesture
uniform int selectedActor;
uniform float greeting;

float oval(vec2 p, vec2 center, vec2 radius)
{
    vec2 q = (p - center)/radius;
    return 1.0 - smoothstep(0.15, 1.0, dot(q, q));
}

float waterMask(vec3 color)
{
    return smoothstep(0.05, 0.19, color.b - color.r)
         * smoothstep(0.05, 0.20, color.g - color.r)
         * smoothstep(0.39, 0.67, color.b);
}

void main()
{
    vec2 p = fragTexCoord*sceneSize;
    vec4 original = texture(texture0, fragTexCoord);
    if (animationAmount < 0.001) {
        finalColor = original*fragColor;
        return;
    }

    // Twelve poses per second retain the illustrated, pixel-art cadence.
    float t = floor(clockTime*12.0)/12.0;
    vec2 displacement = vec2(0.0);
    float actorArea = 0.0;
    for (int i = 0; i < 20; i++) {
        if (i >= actorCount) break;
        vec4 r = actorBounds[i];
        vec2 q = (p - r.xy)/r.zw;
        if (q.x <= 0.0 || q.x >= 1.0 || q.y <= 0.0 || q.y >= 1.0) continue;

        // Continuous, pinned patch edges: no cutout rectangles or seams.
        float edge = smoothstep(0.0, 0.15, q.x)*smoothstep(0.0, 0.15, 1.0 - q.x)
                   * smoothstep(0.0, 0.10, q.y)*smoothstep(0.0, 0.12, 1.0 - q.y);
        vec4 motion = actorMotion[i];
        float cycle = t*motion.x + motion.y;
        float breath = sin(cycle);
        float upper = 1.0 - smoothstep(0.43, 0.93, q.y);
        float strength = motion.z;

        // Head/torso sway pivots around planted feet, with a separate arm gesture.
        displacement.x += edge*upper*breath*strength*0.72;
        displacement.y += edge*upper*sin(cycle*2.0)*strength*0.72;
        float arm = oval(q, vec2(motion.w < 0.0 ? 0.25 : 0.77, 0.53), vec2(0.28, 0.30));
        displacement.y += arm*edge*sin(cycle + 0.8)*abs(motion.w)*1.9;
        displacement.x += arm*edge*cos(cycle)*motion.w*0.65;

        if (i == selectedActor && greeting > 0.0) {
            displacement.y += arm*edge*sin(t*15.0)*greeting*4.0;
            displacement.x += edge*upper*sin(t*7.0)*greeting*0.9;
        }
        actorArea = max(actorArea, edge);
    }

    // The flag is attached to its original pole; its free edge catches the wind.
    vec2 flag = (p - vec2(194.0, 231.0))/vec2(93.0, 84.0);
    if (flag.x > 0.0 && flag.x < 1.0 && flag.y > 0.0 && flag.y < 1.0) {
        float edge = smoothstep(0.0, 0.12, flag.y)*smoothstep(0.0, 0.16, 1.0 - flag.y)
                   * smoothstep(0.0, 0.12, 1.0 - flag.x);
        displacement.y += sin(t*2.6 - flag.x*5.5)*flag.x*edge*2.2;
    }

    // Only cyan water pixels may sample other cyan water pixels. Rice plants,
    // banks, people, the wheel, and the screenshot's lettering stay in place.
    float water = p.y > 650.0 ? waterMask(original.rgb)*(1.0 - actorArea) : 0.0;
    vec2 flow = vec2(sin(p.y*0.065 + t*2.4), cos(p.x*0.048 - t*2.0))*1.2;
    float nextWater = waterMask(texture(texture0, (p + flow)/sceneSize).rgb);
    displacement += flow*water*nextWater;

    // Point sampling keeps every source texel crisp, even while it moves.
    vec2 samplePixel = floor(p + displacement*animationAmount) + vec2(0.5);
    vec4 result = texture(texture0, samplePixel/sceneSize);
    float shimmer = sin(p.x*0.06 + p.y*0.09 - t*3.2)*water*nextWater*0.025;
    result.rgb += vec3(0.55, 0.90, 1.0)*shimmer;
    finalColor = vec4(clamp(result.rgb, 0.0, 1.0), result.a)*fragColor;
}
