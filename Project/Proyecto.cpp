// ===========================================================
//  Escena OpenGL (C++ / GLFW / GLEW / GLM / Assimp)
//  - Mesa, silla (asiento tejido procedural)
//  - Florero, chihuahua voxel, máscara, fogata y antorchas
//  - Suelo de pasto con TEXTURA (anti-tiling) y pirámide
//  - Cielo con estrellas, SOL/LUNA y ciclo día/noche (HQ)
//  - Sin montañas PNG (billboard removidas)
// ===========================================================

#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <array>            

// GLEW / GLFW
#include <GL/glew.h>
#include <GLFW/glfw3.h>

// stb_image (solo encabezado; la implementación vive en stb_impl.cpp)
#include "stb_image.h"

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Modelos / utilidades (estructura estilo LearnOpenGL)
#include "SOIL2/SOIL2.h"
#include "Shader.h"
#include "Camera.h"
#include "Model.h"

// ================== Prototipos ==================
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mode);
void MouseCallback(GLFWwindow* window, double xPos, double yPos);
void DoMovement();

// ================== Ventana =====================
const GLuint WIDTH = 800, HEIGHT = 600;
int SCREEN_WIDTH, SCREEN_HEIGHT;

// ================== Cámara ======================
Camera  camera(glm::vec3(0.0f, 0.0f, 3.0f));
GLfloat lastX = WIDTH / 2.0f;
GLfloat lastY = HEIGHT / 2.0f;
bool keys[1024]{};
bool firstMouse = true;

// ================== Tiempo ======================
GLfloat deltaTime = 0.0f;
GLfloat lastFrame = 0.0f;

// ================== Posiciones base =============
glm::vec3 gTablePos = glm::vec3(-1.8f, 0.0f, -6.0f);
glm::vec3 gChairPos = glm::vec3(-1.2f, 0.0f, -6.2f);
glm::vec3 gCampPos = glm::vec3(-3.2f, 0.0f, -9.8f);

// ================== Shader embebido (procedural)
GLuint gProg = 0;

// ================== VAOs / VBOs =================
GLuint  gVAOCube = 0, gVBOCube = 0;    GLsizei gCubeVerts = 0;
GLuint  gVAOSeat = 0, gVBOSeat = 0;    GLsizei gSeatVerts = 0;
GLuint  gVAOVase = 0, gVBOVase = 0;    GLsizei gVaseVerts = 0;
GLuint  gVAOFlame = 0, gVBOFlame = 0;  GLsizei gFlameVerts = 0;
GLuint  gVAOLog = 0, gVBOLog = 0;      GLsizei gLogVerts = 0;
GLuint  gVAOGround = 0, gVBOGround = 0;

// ================== Texturas ====================
GLuint  gTexGrass = 0;

// ===========================================================
// Shaders procedurales (gProg)
// ===========================================================

static const char* kVS = R"(#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
out vec3 vPos;
void main(){
    vPos = (model * vec4(aPos,1.0)).xyz;
    gl_Position = projection * view * vec4(vPos,1.0);
})";

static const char* kFS = R"(#version 330 core
out vec4 FragColor;
in vec3 vPos;

uniform float uTime;
uniform float uSun;
uniform vec3  uSunDir;
uniform vec3  uCamp;
uniform int   uMode;     // 0=fuego 1=madera 2=florero 3/4/6 colores 5=tejido 7..9 máscara 10=grassProc 11=sky 12=grassTex
uniform sampler2D uTex;  // para uMode==12
uniform float uTexScale;

float hash(vec2 p){ return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453123); }
float noise(vec2 p){
    vec2 i=floor(p), f=fract(p);
    float a=hash(i), b=hash(i+vec2(1,0)), c=hash(i+vec2(0,1)), d=hash(i+vec2(1,1));
    vec2 u=f*f*(3.0-2.0*f);
    return mix(mix(a,b,u.x), mix(c,d,u.x), u.y);
}
float fbm(vec2 p){
    float v=0.0, a=0.5;
    for(int i=0;i<5;i++){ v+=a*noise(p); p*=2.02; a*=0.55; }
    return v;
}
float fbm_ridged(vec2 p){
    float v=0.0, a=0.5;
    for(int i=0;i<5;i++){
        float n = noise(p);
        n = 1.0 - abs(2.0*n - 1.0);
        v += a*n;
        p *= 2.03;
        a *= 0.55;
    }
    return v;
}

vec3 wood(vec3 p){
    float rings = sin((p.x*9.0)+noise(p.zy*6.0)*0.8)*0.5+0.5;
    return mix(vec3(0.55,0.36,0.18), vec3(0.74,0.54,0.30), rings);
}

vec3 skyColor(vec3 dir, float dayVis, out float night){
    night = 1.0 - clamp(dayVis, 0.0, 1.0);
    float h = clamp(dir.y*0.5+0.5, 0.0, 1.0);
    vec3 dawnCol = vec3(0.95,0.55,0.35);
    vec3 dayTop  = vec3(0.12,0.45,0.90);
    vec3 dayHzn  = vec3(0.70,0.85,1.00);
    vec3 duskTop = vec3(0.08,0.20,0.45);
    vec3 duskHzn = vec3(0.20,0.18,0.25);
    vec3 dayMix  = mix(dayHzn, dayTop, h);
    vec3 duskMix = mix(duskHzn, duskTop, h);
    vec3 base    = mix(duskMix, dayMix, clamp(dayVis,0.0,1.0));
    float dawn   = smoothstep(0.40,0.60, dayVis)*(1.0-smoothstep(0.65,0.80,dayVis))
                 + smoothstep(0.20,0.35, dayVis)*(1.0-smoothstep(0.35,0.45,dayVis));
    base = mix(base, mix(dawnCol, base, h*h), clamp(dawn,0.0,1.0));
    return base;
}

float diskHalo(float mu, float rCore, float rHalo){
    float ang  = acos(clamp(mu,-1.0,1.0));
    float core = 1.0 - smoothstep(rCore*0.85, rCore, ang);
    float halo = exp(-pow(ang/(rHalo), 2.0));
    return core + 0.35*halo;
}

void main(){
    if(uMode==1){ FragColor=vec4(wood(vPos),1.0); return; }

    // Fuego
    if(uMode==0){
        float h=clamp(vPos.y,0.0,1.0);
        float r=length(vPos.xz);
        float edge = 1.0 - smoothstep(0.15, 0.35, r);
        float t=uTime*2.0;
        float flick = noise(vec2(r*6.0,h*8.0+t*3.5))*0.6
                    + noise(vec2(r*10.0+t*1.7,h*5.0))*0.4;
        vec3 c1=vec3(1.0,0.25,0.02), c2=vec3(1.0,0.55,0.05),
             c3=vec3(1.0,0.85,0.25), c4=vec3(1.0,0.95,0.75);
        float k=clamp(h*1.2+flick*0.2,0.0,1.0);
        vec3 col=mix(c1,c2,k); col=mix(col,c3,k*k); col=mix(col,c4,pow(k,4.0));
        float a = edge*(0.85 + 0.25*sin(t*7.0 + r*10.0));
        a *= (0.55 + 0.60*h);
        a = clamp(a * 1.25, 0.0, 1.0);
        FragColor=vec4(col,a); return;
    }

    // Asiento tejido
    if(uMode==5){
        mat2 R=mat2(0.7071,-0.7071,0.7071,0.7071);
        vec2 uv=R*vPos.xz*6.0;
        vec2 g=floor(uv);
        float check=mod(g.x+g.y,2.0);
        float ring=floor(max(abs(uv.x),abs(uv.y)));
        float kk=mod(ring+check,2.0);
        vec3 jute=vec3(0.90,0.86,0.72), navy=vec3(0.05,0.10,0.20);
        vec3 base=mix(navy,jute,kk);
        base+=0.08*noise(uv*2.5);
        FragColor=vec4(base,1.0); return;
    }

    // Florero
    if(uMode==2){
        vec3 terracotta=vec3(0.63,0.28,0.20);
        vec3 crema=vec3(0.90,0.82,0.72);
        float b=smoothstep(-0.15,0.15,sin(vPos.y*18.0));
        vec3 col=mix(terracotta,crema,b*0.25);
        if(abs(vPos.y-0.15)<0.06) col=mix(col,vec3(0.14,0.02,0.02),0.85);
        if(abs(vPos.y-0.00)<0.06) col=mix(col,vec3(0.14,0.02,0.02),0.85);
        FragColor=vec4(col,1.0); return;
    }

    if(uMode==3){ FragColor=vec4(0.84,0.72,0.54,1.0); return; }
    if(uMode==4){ FragColor=vec4(0.10,0.07,0.06,1.0); return; }
    if(uMode==6){ FragColor=vec4(0.88,0.42,0.55,1.0); return; }
    if(uMode==7){ FragColor=vec4(0.80,0.12,0.12,1.0); return; }
    if(uMode==8){ FragColor=vec4(0.95,0.95,0.95,1.0); return; }
    if(uMode==9){ FragColor=vec4(0.05,0.05,0.05,1.0); return; }

    // Pasto procedural simple
    if(uMode==10){
        vec2 uv = vPos.xz * 0.45;
        float n1 = noise(uv*2.0);
        float n2 = noise(uv*6.0);
        float n3 = noise(uv*12.0);
        float m  = 0.60 + 0.22*n1 + 0.13*n2 + 0.05*n3;
        vec3 g1 = vec3(0.10, 0.35, 0.08);
        vec3 g2 = vec3(0.25, 0.55, 0.18);
        vec3 col = mix(g1,g2,m);
        float lambert = clamp(dot(normalize(vec3(0,1,0)), normalize(uSunDir)), 0.0, 1.0);
        float sunVis  = clamp(uSun, 0.0, 1.0);
        float light   = 0.40 + lambert * mix(0.55, 1.00, sunVis);
        col *= light;
        FragColor = vec4(col, 1.0); return;
    }

    // Cielo + montañas de horizonte procedurales
    if(uMode==11){
        vec3 dir = normalize(vPos);
        float nightFactor; vec3 base = skyColor(dir, uSun, nightFactor);

        vec2 uvCloud = dir.xz * 0.7 + vec2(0.06*uTime, 0.0);
        float c  = fbm(uvCloud*1.1);
        float cloud = smoothstep(0.52, 0.70, c);
        vec3 cloudCol = mix(vec3(0.88), vec3(1.00), 0.5+0.5*clamp(uSun,0.0,1.0));
        base = mix(base, cloudCol, cloud * (0.30 + 0.40*clamp(uSun,0.0,1.0)));

        float muSun  = dot(dir, normalize(uSunDir));
        float sunDisc= diskHalo(muSun, 0.020, 0.090);
        vec3  sunCol = vec3(1.0, 0.96, 0.85) * sunDisc * clamp(uSun*1.2, 0.0, 1.2);

        vec3  moonDir = -normalize(uSunDir);
        float muMoon  = dot(dir, moonDir);
        float moonDisc= diskHalo(muMoon, 0.016, 0.060);
        vec3  moonCol = vec3(0.85, 0.88, 1.0) * moonDisc * (1.0-clamp(uSun,0.0,1.0)) * 0.75;

        const float PI = 3.14159265359;
        vec2 suv; suv.x = atan(dir.z, dir.x)/(2.0*PI)+0.5; suv.y = asin(clamp(dir.y,-1.0,1.0))/PI+0.5;
        float stars = 0.0;
        float res1=600.0, res2=1200.0, res3=2200.0;
        float rnd;
        rnd = hash(floor(suv*res1)); stars += step(0.9965, rnd);
        rnd = hash(floor(suv*res2)); stars += step(0.9990, rnd)*1.5;
        rnd = hash(floor(suv*res3)); stars += step(0.9997, rnd)*2.0;
        float tw = 0.6 + 0.4*sin(uTime*5.0+suv.x*55.0+suv.y*37.0);
        float starVis = (1.0-clamp(uSun,0.0,1.0)) * (0.25 + 0.75*smoothstep(0.12,0.0,dir.y));
        vec3 starCol = vec3(1.0)*clamp(stars,0.0,4.0)*starVis*tw;

        float az = atan(dir.z, dir.x);
        float wind = 0.025*uTime;
        float ridge1 = fbm_ridged(vec2(az*1.65 + wind, 0.0));
        float ridge2 = fbm_ridged(vec2(az*3.20 - 0.6*wind, 1.3));
        float ridge  = clamp(0.65*pow(ridge1,1.3)+0.35*pow(ridge2,1.6),0.0,1.0);
        float elev = mix(-0.10, 0.24, ridge);
        float m = 1.0 - smoothstep(elev-0.008, elev+0.008, dir.y);
        m *= (1.0 - smoothstep(0.10, 0.35, dir.y));

        vec3 mountDay   = vec3(0.28,0.30,0.34);
        vec3 mountDusk  = vec3(0.20,0.18,0.22);
        vec3 mountNight = vec3(0.08,0.09,0.12);
        vec3 mountCol   = mix(mountNight, mix(mountDusk, mountDay, clamp(uSun,0.0,1.0)), clamp(uSun,0.0,1.0));
        float rock = fbm(vec2(az*6.0, elev*25.0));
        mountCol *= (0.92 + 0.08*rock);
        float hAbove = dir.y - elev;
        float snow = smoothstep(0.06, 0.12, hAbove);
        vec3 snowCol = mix(vec3(0.90), vec3(1.0), 0.35 + 0.35*clamp(uSun,0.0,1.0));
        mountCol = mix(mountCol, snowCol, snow);
        mountCol = mix(mountCol, base, 0.35*smoothstep(-0.35,0.15,dir.y));

        vec3 sky = base + sunCol + moonCol + starCol;
        vec3 finalCol = mix(sky, mountCol, m);
        FragColor = vec4(finalCol, 1.0); return;
    }

    // Pasto TEXTURIZADO (anti-tiling)
    if(uMode==12){
        vec2 uv0 = vPos.xz * uTexScale;
        vec2 warp = vec2(fbm(uv0*0.45), fbm(uv0*0.45 + 37.3));
        uv0 += (warp-0.5)*0.18;

        vec2 cell = floor(uv0);
        vec2 f    = fract(uv0);
        float r   = hash(cell*0.721);
        float ang = (r*2.0 - 1.0)*3.14159;
        mat2  R   = mat2(cos(ang), -sin(ang), sin(ang), cos(ang));
        vec2  jitter = vec2(hash(cell+41.0), hash(cell+173.0)) - 0.5;
        vec2  uvA = (R*(f-0.5) + 0.5) + jitter*0.18 + cell;

        vec2 uv1 = vPos.xz * (uTexScale*0.57);
        vec2 cell1 = floor(uv1);
        float r1   = hash(cell1*1.937);
        float ang1 = (r1*2.0 - 1.0)*3.14159;
        mat2  R1   = mat2(cos(ang1), -sin(ang1), sin(ang1), cos(ang1));
        vec2  f1   = fract(uv1);
        vec2  jitter1 = vec2(hash(cell1+7.0), hash(cell1+89.0)) - 0.5;
        vec2  uvB = (R1*(f1-0.5) + 0.5) + jitter1*0.18 + cell1;

        vec3 texA = texture(uTex, uvA).rgb;
        vec3 texB = texture(uTex, uvB).rgb;

        float mask  = smoothstep(0.40, 0.70, fbm(vPos.xz*0.07 + 13.1));
        vec3  albedo = mix(texA, texB, mask);

        float micro = fbm(vPos.xz*1.2);
        albedo *= mix(0.96, 1.06, micro);

        float lambert = clamp(dot(vec3(0,1,0), normalize(uSunDir)), 0.0, 1.0);
        float sunVis  = clamp(uSun, 0.0, 1.0);
        float ambient = mix(0.40, 0.62, sunVis);
        float light   = ambient + lambert * mix(0.55, 1.00, sunVis);

        FragColor = vec4(albedo * light, 1.0);
        return;
    }

    FragColor = vec4(1,0,1,1);
})";

// ===========================================================
// Utilidades
// ===========================================================
static GLuint Compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type); glShaderSource(s, 1, &src, nullptr); glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[2048]; glGetShaderInfoLog(s, 2048, nullptr, log); std::cout << "Shader error:\n" << log << "\n"; }
    return s;
}
static GLuint Link(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram(); glAttachShader(p, vs); glAttachShader(p, fs); glLinkProgram(p);
    GLint ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { char log[2048]; glGetProgramInfoLog(p, 2048, nullptr, log); std::cout << "Link error:\n" << log << "\n"; }
    glDeleteShader(vs); glDeleteShader(fs); return p;
}
static void CreateProgram() { gProg = Link(Compile(GL_VERTEX_SHADER, kVS), Compile(GL_FRAGMENT_SHADER, kFS)); }

// Carga de textura 2D (mipmaps + anisotrópico si existe)
static GLuint LoadTexture2D(const char* path, bool repeat = true) {
    int w = 0, h = 0, nc = 0;
    stbi_uc* data = stbi_load(path, &w, &h, &nc, 3);
    if (!data) { std::cout << "No se pudo cargar " << path << "\n"; return 0; }
    GLuint tex = 0; glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    if (GLEW_EXT_texture_filter_anisotropic) {
        GLfloat aniso = 0.0f; glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &aniso);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, std::min(8.0f, aniso));
    }
    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

// ===========================================================
// Geometrías
// ===========================================================
static void BuildCube() {
    float v[] = {
        -0.5f,-0.5f, 0.5f,  0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f,
        -0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f,
        -0.5f,-0.5f,-0.5f, -0.5f, 0.5f,-0.5f,  0.5f, 0.5f,-0.5f,
        -0.5f,-0.5f,-0.5f,  0.5f, 0.5f,-0.5f,  0.5f,-0.5f,-0.5f,
         0.5f,-0.5f,-0.5f,  0.5f, 0.5f,-0.5f,  0.5f, 0.5f, 0.5f,
         0.5f,-0.5f,-0.5f,  0.5f, 0.5f, 0.5f,  0.5f,-0.5f, 0.5f,
        -0.5f,-0.5f,-0.5f, -0.5f,-0.5f, 0.5f, -0.5f, 0.5f, 0.5f,
        -0.5f,-0.5f,-0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f,-0.5f,
        -0.5f, 0.5f,-0.5f, -0.5f, 0.5f, 0.5f,  0.5f, 0.5f, 0.5f,
        -0.5f, 0.5f,-0.5f,  0.5f, 0.5f, 0.5f,  0.5f, 0.5f,-0.5f,
        -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f,-0.5f, 0.5f,
        -0.5f,-0.5f,-0.5f, -0.5f,-0.5f, 0.5f,  0.5f,-0.5f, 0.5f
    };
    gCubeVerts = 36;
    glGenVertexArrays(1, &gVAOCube);
    glGenBuffers(1, &gVBOCube);
    glBindVertexArray(gVAOCube);
    glBindBuffer(GL_ARRAY_BUFFER, gVBOCube);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);
}

static void BuildSeatPlane(int nx = 40, int nz = 40) {
    std::vector<glm::vec3> verts; verts.reserve(nx * nz * 6);
    for (int i = 0; i < nx; ++i) {
        float x0 = -0.5f + (float)i / nx;
        float x1 = -0.5f + (float)(i + 1) / nx;
        for (int j = 0; j < nz; ++j) {
            float z0 = -0.5f + (float)j / nz;
            float z1 = -0.5f + (float)(j + 1) / nz;
            glm::vec3 a(x0, 0, z0), b(x1, 0, z0), c(x1, 0, z1), d(x0, 0, z1);
            verts.push_back(a); verts.push_back(b); verts.push_back(c);
            verts.push_back(a); verts.push_back(c); verts.push_back(d);
        }
    }
    gSeatVerts = (GLsizei)verts.size();
    glGenVertexArrays(1, &gVAOSeat);
    glGenBuffers(1, &gVBOSeat);
    glBindVertexArray(gVAOSeat);
    glBindBuffer(GL_ARRAY_BUFFER, gVBOSeat);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(glm::vec3), verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glBindVertexArray(0);
}

static void BuildVase() {
    std::vector<glm::vec3> v;
    std::vector<glm::vec2> prof = {
        {0.00f,-0.30f},{0.25f,-0.30f},{0.35f,-0.20f},{0.42f,-0.05f},
        {0.43f, 0.10f},{0.36f, 0.20f},{0.28f, 0.28f},{0.22f, 0.32f},
        {0.18f, 0.34f},{0.16f, 0.35f}
    };
    int seg = 64; v.reserve((prof.size() - 1) * seg * 6);
    for (size_t k = 0; k + 1 < prof.size(); ++k) {
        float r0 = prof[k].x, y0 = prof[k].y;
        float r1 = prof[k + 1].x, y1 = prof[k + 1].y;
        for (int i = 0; i < seg; ++i) {
            float a0 = i * (2.f * 3.14159265f / seg);
            float a1 = (i + 1) * (2.f * 3.14159265f / seg);
            glm::vec3 A(r0 * cosf(a0), y0, r0 * sinf(a0));
            glm::vec3 B(r0 * cosf(a1), y0, r0 * sinf(a1));
            glm::vec3 C(r1 * cosf(a1), y1, r1 * sinf(a1));
            glm::vec3 D(r1 * cosf(a0), y1, r1 * sinf(a0));
            v.push_back(A); v.push_back(B); v.push_back(C);
            v.push_back(A); v.push_back(C); v.push_back(D);
        }
    }
    gVaseVerts = (GLsizei)v.size();
    glGenVertexArrays(1, &gVAOVase);
    glGenBuffers(1, &gVBOVase);
    glBindVertexArray(gVAOVase);
    glBindBuffer(GL_ARRAY_BUFFER, gVBOVase);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(glm::vec3), v.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glBindVertexArray(0);
}

static void BuildCampfire() {
    float L = 1.4f, H = 0.12f, W = 0.12f;
    float x0 = -L * 0.5f, x1 = L * 0.5f, y0 = -H * 0.5f, y1 = H * 0.5f, z0 = -W * 0.5f, z1 = W * 0.5f;
    glm::vec3 p[] = { {x0,y0,z0},{x1,y0,z0},{x1,y1,z0},{x0,y1,z0},{x0,y0,z1},{x1,y0,z1},{x1,y1,z1},{x0,y1,z1} };
    auto quad = [&](int a, int b, int c, int d, std::vector<glm::vec3>& o) {
        o.push_back(p[a]); o.push_back(p[b]); o.push_back(p[c]);
        o.push_back(p[a]); o.push_back(p[c]); o.push_back(p[d]);
        };
    std::vector<glm::vec3> log; log.reserve(36);
    quad(0, 1, 2, 3, log); quad(4, 5, 6, 7, log);
    quad(0, 4, 7, 3, log); quad(1, 5, 6, 2, log);
    quad(3, 2, 6, 7, log); quad(0, 1, 5, 4, log);
    gLogVerts = (GLsizei)log.size();
    glGenVertexArrays(1, &gVAOLog);
    glGenBuffers(1, &gVBOLog);
    glBindVertexArray(gVAOLog);
    glBindBuffer(GL_ARRAY_BUFFER, gVBOLog);
    glBufferData(GL_ARRAY_BUFFER, log.size() * sizeof(glm::vec3), log.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glBindVertexArray(0);

    const int seg = 48;
    const float r = 0.45f;
    std::vector<glm::vec3> flame; flame.reserve(seg * 6);
    glm::vec3 tip(0, 1.35f, 0);
    for (int i = 0; i < seg; ++i) {
        float a0 = i * (2.f * 3.14159265f / seg);
        float a1 = (i + 1) * (2.f * 3.14159265f / seg);
        glm::vec3 A(0, 0, 0), B(r * cosf(a0), 0, r * sinf(a0)), C(r * cosf(a1), 0, r * sinf(a1));
        flame.push_back(A); flame.push_back(B); flame.push_back(C);
        flame.push_back(B); flame.push_back(C); flame.push_back(tip);
    }
    gFlameVerts = (GLsizei)flame.size();
    glGenVertexArrays(1, &gVAOFlame);
    glGenBuffers(1, &gVBOFlame);
    glBindVertexArray(gVAOFlame);
    glBindBuffer(GL_ARRAY_BUFFER, gVBOFlame);
    glBufferData(GL_ARRAY_BUFFER, flame.size() * sizeof(glm::vec3), flame.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glBindVertexArray(0);
}

static void BuildGround(float S = 220.0f) {
    float v[] = {
        -S, 0.0f, -S,
         S, 0.0f, -S,
         S, 0.0f,  S,
        -S, 0.0f, -S,
         S, 0.0f,  S,
        -S, 0.0f,  S
    };
    glGenVertexArrays(1, &gVAOGround);
    glGenBuffers(1, &gVBOGround);
    glBindVertexArray(gVAOGround);
    glBindBuffer(GL_ARRAY_BUFFER, gVBOGround);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);
}

// ===========================================================
// main
// ===========================================================
int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Escena (pasto + cielo procedural)", nullptr, nullptr);
    if (!window) { std::cout << "Failed to create GLFW window\n"; glfwTerminate(); return EXIT_FAILURE; }

    glfwMakeContextCurrent(window);
    glfwGetFramebufferSize(window, &SCREEN_WIDTH, &SCREEN_HEIGHT);
    glfwSetKeyCallback(window, KeyCallback);
    glfwSetCursorPosCallback(window, MouseCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) { std::cout << "Failed to initialize GLEW\n"; return EXIT_FAILURE; }

    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);
    glEnable(GL_FRAMEBUFFER_SRGB);
    glEnable(GL_MULTISAMPLE);

    // -------------- Carga de modelos (tu shader externo)
    Shader shader("Shader/modelLoading.vs", "Shader/modelLoading.frag");
    Model CanastaChiles((char*)"Models/CanastaChiles.obj");
    Model Chiles((char*)"Models/Chiles.obj");
    Model PetatesTianguis((char*)"Models/PetatesTianguis.obj");
    Model Aguacates((char*)"Models/Aguacates.obj");
    Model Jarrones((char*)"Models/Jarrones.obj");
    Model Tendedero((char*)"Models/Tendedero.obj");
    Model PielJaguar((char*)"Models/PielJaguar.obj");
    Model PielesPiso((char*)"Models/PielesPiso.obj");
    Model Piramide((char*)"Models/Piramide.obj");
    Model TechosChozas((char*)"Models/TechosChozas.obj");
    Model ParedesChozas((char*)"Models/ParedesChozas.obj");
    Model Calendario((char*)"Models/calendario_azteca1.obj");
    Model VasijasYMolcajete((char*)"Models/VasijasYMolcajete.obj");
    Model Tunas((char*)"Models/Tunas.obj");
    Model Vasijas((char*)"Models/Vasijas.obj");

    // -------------- Programa procedural + geometrías
    CreateProgram();
    BuildCube();
    BuildSeatPlane();
    BuildVase();
    BuildCampfire();
    BuildGround();

    // -------------- Texturas
    stbi_set_flip_vertically_on_load(0);
    gTexGrass = LoadTexture2D("Models/pasto.jpg", true);

    glm::mat4 projection = glm::perspective(camera.GetZoom(),
        (GLfloat)SCREEN_WIDTH / (GLfloat)SCREEN_HEIGHT, 0.1f, 1000.0f);

    const float topX = 1.6f, topZ = 3.0f, topY = 0.12f;
    const float legH = 0.90f, legT = 0.09f, railT = 0.06f, railH = 0.32f;
    const float seat = 0.60f, tLeg = 0.06f;

    gChairPos = gTablePos + glm::vec3(topX * 0.5f + 0.45f + seat * 0.5f, 0.0f, -topZ * 0.25f);

    const float cycleSeconds = 60.0f;

    while (!glfwWindowShouldClose(window)) {
        GLfloat currentFrame = (GLfloat)glfwGetTime();
        deltaTime = currentFrame - lastFrame; lastFrame = currentFrame;

        glfwPollEvents();
        DoMovement();

        glClearColor(0.05f, 0.05f, 0.06f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = camera.GetViewMatrix();

        float t = fmodf(currentFrame / cycleSeconds, 1.0f);
        float az = t * 6.2831853f;
        float el = sinf(az) * 0.8f;
        float ce = cosf(el), se = sinf(el);
        glm::vec3 sunDir = glm::normalize(glm::vec3(ce * cosf(az), se, ce * sinf(az)));
        float sun = 0.5f + 0.5f * se;

        // -------- CIELO (procedural)
        {
            glUseProgram(gProg);
            glm::mat4 viewNoTrans = glm::mat4(glm::mat3(view));
            glUniformMatrix4fv(glGetUniformLocation(gProg, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
            glUniformMatrix4fv(glGetUniformLocation(gProg, "view"), 1, GL_FALSE, glm::value_ptr(viewNoTrans));
            glUniform1f(glGetUniformLocation(gProg, "uTime"), currentFrame);
            glUniform1f(glGetUniformLocation(gProg, "uSun"), sun);
            glUniform3fv(glGetUniformLocation(gProg, "uSunDir"), 1, glm::value_ptr(sunDir));
            glUniform1i(glGetUniformLocation(gProg, "uMode"), 11);

            glm::mat4 MSky(1.0f); MSky = glm::scale(MSky, glm::vec3(500.0f));
            glUniformMatrix4fv(glGetUniformLocation(gProg, "model"), 1, GL_FALSE, glm::value_ptr(MSky));

            glDepthFunc(GL_LEQUAL);
            glDepthMask(GL_FALSE);
            glBindVertexArray(gVAOCube);
            glDrawArrays(GL_TRIANGLES, 0, gCubeVerts);
            glBindVertexArray(0);
            glDepthMask(GL_TRUE);
            glDepthFunc(GL_LESS);
            glUseProgram(0);
        }

        // -------- SUELO (PASTO TEXTURIZADO)
        glUseProgram(gProg);
        glUniformMatrix4fv(glGetUniformLocation(gProg, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(gProg, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniform1f(glGetUniformLocation(gProg, "uTime"), currentFrame);
        glUniform1f(glGetUniformLocation(gProg, "uSun"), sun);
        glUniform3fv(glGetUniformLocation(gProg, "uSunDir"), 1, glm::value_ptr(sunDir));
        glUniform1i(glGetUniformLocation(gProg, "uMode"), 12);
        glUniform1f(glGetUniformLocation(gProg, "uTexScale"), 0.28f);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gTexGrass);
        glUniform1i(glGetUniformLocation(gProg, "uTex"), 0);

        glm::mat4 MG(1.0f);
        MG = glm::translate(MG, glm::vec3(0.0f, -0.001f, 0.0f));
        glUniformMatrix4fv(glGetUniformLocation(gProg, "model"), 1, GL_FALSE, glm::value_ptr(MG));
        glBindVertexArray(gVAOGround);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        glUseProgram(0);

        // ====== DIBUJAR MODELOS DEL TIANGUIS ======
        shader.Use();
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "view"), 1, GL_FALSE, glm::value_ptr(view));

        glm::mat4 model(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model));
        CanastaChiles.Draw(shader);

        glm::mat4 model2(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model2));
        Chiles.Draw(shader);

        glm::mat4 model3(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model3));
        PetatesTianguis.Draw(shader);

        glm::mat4 model4(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model4));
        Aguacates.Draw(shader);

        glm::mat4 model5(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model5));
        Jarrones.Draw(shader);

        glm::mat4 model6(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model6));
        Tendedero.Draw(shader);

        glm::mat4 model7(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model7));
        PielJaguar.Draw(shader);

        glm::mat4 model8(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model8));
        PielesPiso.Draw(shader);


        glm::mat4 model9(1.0f);
        model9 = glm::translate(model9, glm::vec3(0.0f, 0.0f, -100.0f));
        model9 = glm::scale(model9, glm::vec3(0.5f, 0.5f, 0.5f));
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model9));
        Piramide.Draw(shader);

        glm::mat4 model10(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model10));
        ParedesChozas.Draw(shader);


        glm::mat4 model11(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model11));
        TechosChozas.Draw(shader);

        glm::mat4 model12(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model12));
        VasijasYMolcajete.Draw(shader);

        glm::mat4 model13(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model13));
        Tunas.Draw(shader);
       
        glm::mat4 model14(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model14));
        Vasijas.Draw(shader);

        // -------- Procedural (mesa, silla, etc.) con gProg
        glUseProgram(gProg);
        GLint uProj = glGetUniformLocation(gProg, "projection");
        GLint uView = glGetUniformLocation(gProg, "view");
        glUniformMatrix4fv(uProj, 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(uView, 1, GL_FALSE, glm::value_ptr(view));
        glUniform1f(glGetUniformLocation(gProg, "uTime"), currentFrame);
        glUniform1f(glGetUniformLocation(gProg, "uSun"), sun);
        glUniform3fv(glGetUniformLocation(gProg, "uSunDir"), 1, glm::value_ptr(sunDir));

        auto drawCubeAt = [&](glm::vec3 pos, glm::vec3 scl, int mode = 1) {
            glm::mat4 MM(1.0f); MM = glm::translate(MM, pos); MM = glm::scale(MM, scl);
            glUniformMatrix4fv(glGetUniformLocation(gProg, "model"), 1, GL_FALSE, glm::value_ptr(MM));
            glUniform1i(glGetUniformLocation(gProg, "uMode"), mode);
            glBindVertexArray(gVAOCube); glDrawArrays(GL_TRIANGLES, 0, gCubeVerts);
            };

        // Mesa
        {
            float ox = topX * 0.5f - legT * 0.5f;
            float oz = topZ * 0.5f - legT * 0.5f;
            drawCubeAt(gTablePos + glm::vec3(0.0f, legH + topY * 0.5f, 0.0f), glm::vec3(topX, topY, topZ), 1);
            drawCubeAt(gTablePos + glm::vec3(+ox, legH * 0.5f, +oz), glm::vec3(legT, legH, legT), 1);
            drawCubeAt(gTablePos + glm::vec3(-ox, legH * 0.5f, +oz), glm::vec3(legT, legH, legT), 1);
            drawCubeAt(gTablePos + glm::vec3(+ox, legH * 0.5f, -oz), glm::vec3(legT, legH, legT), 1);
            drawCubeAt(gTablePos + glm::vec3(-ox, legH * 0.5f, -oz), glm::vec3(legT, legH, legT), 1);
            drawCubeAt(gTablePos + glm::vec3(0.0f, railH, +oz), glm::vec3(topX - legT * 1.4f, railT, legT), 1);
            drawCubeAt(gTablePos + glm::vec3(0.0f, railH, -oz), glm::vec3(topX - legT * 1.4f, railT, legT), 1);
            drawCubeAt(gTablePos + glm::vec3(+ox, railH, 0.0f), glm::vec3(legT, railT, topZ - legT * 1.4f), 1);
            drawCubeAt(gTablePos + glm::vec3(-ox, railH, 0.0f), glm::vec3(legT, railT, topZ - legT * 1.4f), 1);
        }

        // Silla + asiento tejido
        {
            auto chair = [&](glm::vec3 lp, glm::vec3 s2) { drawCubeAt(gChairPos + lp, s2, 1); };
            chair(glm::vec3(+seat * 0.5f - tLeg * 0.5f, 0.75f * 0.5f, +seat * 0.5f - tLeg * 0.5f), glm::vec3(tLeg, 0.75f, tLeg));
            chair(glm::vec3(-seat * 0.5f + tLeg * 0.5f, 0.75f * 0.5f, +seat * 0.5f - tLeg * 0.5f), glm::vec3(tLeg, 0.75f, tLeg));
            chair(glm::vec3(+seat * 0.5f - tLeg * 0.5f, 0.75f * 0.5f, -seat * 0.5f + tLeg * 0.5f), glm::vec3(tLeg, 0.75f, tLeg));
            chair(glm::vec3(-seat * 0.5f + tLeg * 0.5f, 0.75f * 0.5f, -seat * 0.5f + tLeg * 0.5f), glm::vec3(tLeg, 0.75f, tLeg));
            chair(glm::vec3(0.0f, 0.35f, +seat * 0.5f - tLeg * 0.5f), glm::vec3(seat - tLeg * 1.2f, 0.05f, tLeg));
            chair(glm::vec3(0.0f, 0.35f, -seat * 0.5f + tLeg * 0.5f), glm::vec3(seat - tLeg * 1.2f, 0.05f, tLeg));
            chair(glm::vec3(+seat * 0.5f - tLeg * 0.5f, 0.35f, 0.0f), glm::vec3(tLeg, 0.05f, seat - tLeg * 1.2f));
            chair(glm::vec3(-seat * 0.5f + tLeg * 0.5f, 0.35f, 0.0f), glm::vec3(tLeg, 0.05f, seat - tLeg * 1.2f));

            glm::mat4 MS(1.0f);
            MS = glm::translate(MS, gChairPos + glm::vec3(0.0f, 0.75f, 0.0f));
            MS = glm::scale(MS, glm::vec3(seat, 1.0f, seat));
            glUniformMatrix4fv(glGetUniformLocation(gProg, "model"), 1, GL_FALSE, glm::value_ptr(MS));
            glUniform1i(glGetUniformLocation(gProg, "uMode"), 5);
            glBindVertexArray(gVAOSeat);
            glDrawArrays(GL_TRIANGLES, 0, gSeatVerts);
            glBindVertexArray(0);
        }

        // Florero
        {
            float vaseH = 0.60f;
            glm::mat4 MV(1.0f);
            MV = glm::translate(MV, gTablePos + glm::vec3(0.0f, 0.90f + 0.12f + vaseH * 0.5f, 0.0f));
            MV = glm::scale(MV, glm::vec3(vaseH));
            glUniformMatrix4fv(glGetUniformLocation(gProg, "model"), 1, GL_FALSE, glm::value_ptr(MV));
            glUniform1i(glGetUniformLocation(gProg, "uMode"), 2);
            glBindVertexArray(gVAOVase);
            glDrawArrays(GL_TRIANGLES, 0, gVaseVerts);
            glBindVertexArray(0);
        }

        // Chihuahua (voxel)
        auto drawPart = [&](const glm::vec3& base, float yaw, glm::vec3 local, glm::vec3 scl, int mode) {
            glm::mat4 MD(1.0f);
            MD = glm::translate(MD, base);
            MD = glm::rotate(MD, glm::radians(yaw), glm::vec3(0, 1, 0));
            MD = glm::translate(MD, local);
            MD = glm::scale(MD, scl);
            glUniformMatrix4fv(glGetUniformLocation(gProg, "model"), 1, GL_FALSE, glm::value_ptr(MD));
            glUniform1i(glGetUniformLocation(gProg, "uMode"), mode);
            glBindVertexArray(gVAOCube);
            glDrawArrays(GL_TRIANGLES, 0, gCubeVerts);
            };
        auto drawDog = [&](glm::vec3 base, float yaw, float s) {
            drawPart(base, yaw, glm::vec3(+0.10f, 0.18f, 0.0f), glm::vec3(0.38f * s, 0.22f * s, 0.22f * s), 3);
            drawPart(base, yaw, glm::vec3(-0.18f, 0.19f, 0.0f), glm::vec3(0.30f * s, 0.20f * s, 0.22f * s), 3);
            drawPart(base, yaw, glm::vec3(+0.30f, 0.28f, 0.0f), glm::vec3(0.10f * s, 0.16f * s, 0.16f * s), 3);
            drawPart(base, yaw, glm::vec3(+0.43f, 0.34f, 0.0f), glm::vec3(0.18f * s, 0.16f * s, 0.18f * s), 3);
            drawPart(base, yaw, glm::vec3(+0.55f, 0.30f, 0.0f), glm::vec3(0.12f * s, 0.10f * s, 0.12f * s), 3);
            drawPart(base, yaw, glm::vec3(+0.62f, 0.30f, 0.0f), glm::vec3(0.06f * s, 0.06f * s, 0.06f * s), 4);
            drawPart(base, yaw, glm::vec3(+0.58f, 0.22f, 0.0f), glm::vec3(0.06f * s, 0.02f * s, 0.04f * s), 6);
            drawPart(base, yaw, glm::vec3(+0.46f, 0.48f, +0.07f), glm::vec3(0.06f * s, 0.14f * s, 0.06f * s), 4);
            drawPart(base, yaw, glm::vec3(+0.46f, 0.48f, -0.07f), glm::vec3(0.06f * s, 0.14f * s, 0.06f * s), 4);
            drawPart(base, yaw, glm::vec3(+0.52f, 0.36f, +0.08f), glm::vec3(0.03f * s, 0.03f * s, 0.03f * s), 4);
            drawPart(base, yaw, glm::vec3(+0.52f, 0.36f, -0.08f), glm::vec3(0.03f * s, 0.03f * s, 0.03f * s), 4);
            drawPart(base, yaw, glm::vec3(+0.20f, 0.09f, +0.09f), glm::vec3(0.07f * s, 0.18f * s, 0.07f * s), 3);
            drawPart(base, yaw, glm::vec3(+0.20f, 0.09f, -0.09f), glm::vec3(0.07f * s, 0.18f * s, 0.07f * s), 3);
            drawPart(base, yaw, glm::vec3(-0.22f, 0.09f, +0.09f), glm::vec3(0.07f * s, 0.18f * s, 0.07f * s), 3);
            drawPart(base, yaw, glm::vec3(-0.22f, 0.09f, -0.09f), glm::vec3(0.07f * s, 0.18f * s, 0.07f * s), 3);
            drawPart(base, yaw, glm::vec3(-0.32f, 0.32f, 0.0f), glm::vec3(0.05f * s, 0.16f * s, 0.05f * s), 3);
            };
        float zDog = gTablePos.z + 3.0f * 0.5f + 0.55f;
        float xDog = gTablePos.x - 0.30f;
        drawDog(glm::vec3(xDog, 0.0f, zDog), 10.0f, 1.0f);

        // Máscara
        {
            auto drawCubeLocal = [&](glm::vec3 pos, glm::vec3 scl, int mode = 1) {
                glm::mat4 MM(1.0f); MM = glm::translate(MM, pos); MM = glm::scale(MM, scl);
                glUniformMatrix4fv(glGetUniformLocation(gProg, "model"), 1, GL_FALSE, glm::value_ptr(MM));
                glUniform1i(glGetUniformLocation(gProg, "uMode"), mode);
                glBindVertexArray(gVAOCube); glDrawArrays(GL_TRIANGLES, 0, gCubeVerts);
                };
            glm::vec3 base = gTablePos + glm::vec3(+0.45f, 0.90f + 0.12f + 0.03f, +0.35f);
            drawCubeLocal(base, glm::vec3(0.22f, 0.02f, 0.16f), 7);
            drawCubeLocal(base + glm::vec3(0.0f, 0.011f, 0.0f), glm::vec3(0.24f, 0.006f, 0.05f), 8);
            drawCubeLocal(base + glm::vec3(0.0f, 0.011f, 0.0f), glm::vec3(0.05f, 0.006f, 0.18f), 8);
            drawCubeLocal(base + glm::vec3(+0.06f, 0.013f, +0.05f), glm::vec3(0.04f, 0.004f, 0.04f), 9);
            drawCubeLocal(base + glm::vec3(+0.06f, 0.013f, -0.05f), glm::vec3(0.04f, 0.004f, 0.04f), 9);
        }

        // Fogata + antorchas
        auto place = [&](glm::vec3 pos, glm::vec3 rotDeg, glm::vec3 scl, int mode) {
            glm::mat4 MF(1.0f);
            MF = glm::translate(MF, pos);
            if (rotDeg.x != 0) MF = glm::rotate(MF, glm::radians(rotDeg.x), glm::vec3(1, 0, 0));
            if (rotDeg.y != 0) MF = glm::rotate(MF, glm::radians(rotDeg.y), glm::vec3(0, 1, 0));
            if (rotDeg.z != 0) MF = glm::rotate(MF, glm::radians(rotDeg.z), glm::vec3(0, 0, 1));
            MF = glm::scale(MF, scl);
            glUniformMatrix4fv(glGetUniformLocation(gProg, "model"), 1, GL_FALSE, glm::value_ptr(MF));
            glUniform1i(glGetUniformLocation(gProg, "uMode"), mode);
            };

        // Troncos
        place(gCampPos + glm::vec3(0.0f, 0.06f, 0.0f), glm::vec3(0, 20, 0), glm::vec3(1.0f), 1);
        glBindVertexArray(gVAOLog); glDrawArrays(GL_TRIANGLES, 0, gLogVerts);
        place(gCampPos + glm::vec3(0.0f, 0.06f, 0.0f), glm::vec3(0, 110, 0), glm::vec3(1.0f), 1);
        glBindVertexArray(gVAOLog); glDrawArrays(GL_TRIANGLES, 0, gLogVerts);

        // Llamas (aditivo)
        {
            place(gCampPos + glm::vec3(0.0f, 0.18f, 0.0f), glm::vec3(0, 0, 0),
                glm::vec3(1.25f, 1.35f, 1.25f), 0);

            GLboolean oldCull = glIsEnabled(GL_CULL_FACE);
            GLint oldSrcRGB, oldDstRGB, oldSrcA, oldDstA;
            glGetIntegerv(GL_BLEND_SRC_RGB, &oldSrcRGB);
            glGetIntegerv(GL_BLEND_DST_RGB, &oldDstRGB);
            glGetIntegerv(GL_BLEND_SRC_ALPHA, &oldSrcA);
            glGetIntegerv(GL_BLEND_DST_ALPHA, &oldDstA);

            glDisable(GL_CULL_FACE);
            glDepthMask(GL_FALSE);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);

            glBindVertexArray(gVAOFlame);
            glDrawArrays(GL_TRIANGLES, 0, gFlameVerts);

            glDepthMask(GL_TRUE);
            if (oldCull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
            glBlendFunc(oldSrcRGB, oldDstRGB);
        }

        auto drawTorch = [&](glm::vec3 p, float h) {
            glm::mat4 TM(1.0f);
            TM = glm::translate(TM, p + glm::vec3(0, h * 0.5f, 0));
            TM = glm::scale(TM, glm::vec3(0.08f, h, 0.08f));
            glUniformMatrix4fv(glGetUniformLocation(gProg, "model"), 1, GL_FALSE, glm::value_ptr(TM));
            glUniform1i(glGetUniformLocation(gProg, "uMode"), 1);
            glBindVertexArray(gVAOCube); glDrawArrays(GL_TRIANGLES, 0, gCubeVerts);

            TM = glm::mat4(1.0f);
            TM = glm::translate(TM, p + glm::vec3(0, h + 0.04f, 0));
            TM = glm::scale(TM, glm::vec3(0.16f, 0.06f, 0.16f));
            glUniformMatrix4fv(glGetUniformLocation(gProg, "model"), 1, GL_FALSE, glm::value_ptr(TM));
            glUniform1i(glGetUniformLocation(gProg, "uMode"), 1);
            glBindVertexArray(gVAOCube); glDrawArrays(GL_TRIANGLES, 0, gCubeVerts);

            // Fuego antorcha
            glm::mat4 F(1.0f);
            F = glm::translate(F, p + glm::vec3(0, h + 0.10f, 0));
            glUniformMatrix4fv(glGetUniformLocation(gProg, "model"), 1, GL_FALSE, glm::value_ptr(F));
            glUniform1i(glGetUniformLocation(gProg, "uMode"), 0);

            GLboolean oldCull = glIsEnabled(GL_CULL_FACE);
            GLint oldSrcRGB, oldDstRGB, oldSrcA, oldDstA;
            glGetIntegerv(GL_BLEND_SRC_RGB, &oldSrcRGB);
            glGetIntegerv(GL_BLEND_DST_RGB, &oldDstRGB);
            glGetIntegerv(GL_BLEND_SRC_ALPHA, &oldSrcA);
            glGetIntegerv(GL_BLEND_DST_ALPHA, &oldDstA);

            glDisable(GL_CULL_FACE);
            glDepthMask(GL_FALSE);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);

            glBindVertexArray(gVAOFlame);
            glDrawArrays(GL_TRIANGLES, 0, gFlameVerts);

            glDepthMask(GL_TRUE);
            if (oldCull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
            glBlendFunc(oldSrcRGB, oldDstRGB);
            };
        drawTorch(gCampPos + glm::vec3(+0.9f, 0.0f, +0.6f), 1.2f);
        drawTorch(gCampPos + glm::vec3(-0.9f, 0.0f, +0.6f), 1.2f);
        drawTorch(glm::vec3(-1.0f, 0.0f, -97.0f), 1.6f);
        drawTorch(glm::vec3(+1.0f, 0.0f, -97.0f), 1.6f);

        glUseProgram(0);

        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}

// ===========================================================
// Input
// ===========================================================
void DoMovement() {
    if (keys[GLFW_KEY_W] || keys[GLFW_KEY_UP])    camera.ProcessKeyboard(FORWARD, deltaTime);
    if (keys[GLFW_KEY_S] || keys[GLFW_KEY_DOWN])  camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (keys[GLFW_KEY_A] || keys[GLFW_KEY_LEFT])  camera.ProcessKeyboard(LEFT, deltaTime);
    if (keys[GLFW_KEY_D] || keys[GLFW_KEY_RIGHT]) camera.ProcessKeyboard(RIGHT, deltaTime);
}
void KeyCallback(GLFWwindow* window, int key, int, int action, int) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) glfwSetWindowShouldClose(window, GL_TRUE);
    if (key >= 0 && key < 1024) {
        if (action == GLFW_PRESS) keys[key] = true;
        else if (action == GLFW_RELEASE) keys[key] = false;
    }
}
void MouseCallback(GLFWwindow*, double xPos, double yPos) {
    if (firstMouse) { lastX = (GLfloat)xPos; lastY = (GLfloat)yPos; firstMouse = false; }
    GLfloat xOffset = (GLfloat)xPos - lastX; GLfloat yOffset = lastY - (GLfloat)yPos;
    lastX = (GLfloat)xPos; lastY = (GLfloat)yPos;
    camera.ProcessMouseMovement(xOffset, yOffset);
}