//  Escena OpenGL (C++ / GLFW / GLEW / GLM / Assimp)
//  - Mesa, silla (asiento tejido procedural)
//  - Florero, fogata y antorchas (shader preparado, uso base)
//  - Suelo de pasto con TEXTURA (anti-tiling) y pirámides
//  - Cielo con estrellas, SOL/LUNA y ciclo día/noche (HQ)
//  - Árboles y cactus: instancias aleatorias con exclusiones
//  - Shader externo para modelos + shader embebido procedural
// ===========================================================

#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <array>
#include <random>
#include <algorithm>
#include <cstdlib>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "SOIL2/SOIL2.h"
#include "Shader.h"
#include "Camera.h"
#include "Model.h"

glm::vec3 gAxePos(10.0f, 0.0f, -5.0f);  // posición del hacha
float gAxeSpeed = 2.5f;                // velocidad de movimiento



GLuint gVAOSphere = 0, gVBOSphere = 0;
int gSphereVerts = 0;
// ================== Prototipos ==================
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mode);
void MouseCallback(GLFWwindow* window, double xPos, double yPos);
void DoMovement();

// ================== Ventana =====================
const GLuint WIDTH = 800, HEIGHT = 600;
int SCREEN_WIDTH, SCREEN_HEIGHT;

// ================== Cámara ======================
Camera  camera(glm::vec3(0.0f, 1.6f, 6.0f));
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
glm::vec3 gCampPos = glm::vec3(0.0f, 0.0f, 0.0f);

// ================== Shader embebido (procedural)
GLuint gProg = 0;

// ================== VAOs / VBOs =================
GLuint  gVAOCube = 0, gVBOCube = 0;   GLsizei gCubeVerts = 0;
GLuint  gVAOSeat = 0, gVBOSeat = 0;   GLsizei gSeatVerts = 0;
GLuint  gVAOVase = 0, gVBOVase = 0;   GLsizei gVaseVerts = 0;
GLuint  gVAOGround = 0, gVBOGround = 0;   GLsizei gGroundVerts = 0;
GLuint  gVAOCone = 0, gVBOCone = 0;   GLsizei gConeVerts = 0;
bool    gFireOn = false;
// ================== Texturas ====================
GLuint  gTexGrass = 0;

// ================== Instancias árbol/cactus =====
const int AR_COUNT = 45;
std::vector<glm::mat4> gArModels;

const int CA_COUNT = 45;
std::vector<glm::mat4> gcaModels;

const int CO_COUNT = 45;
std::vector<glm::mat4> gCoModels;   // <- MAÍZ

glm::vec3 gWheelPos(20.0f, 0.0f, -15.0f);   // NUEVA POSICIÓN LEJOS
float gWheelYaw = 180.0f;                  // mirando hacia -X (para que avance hacia la cámara)

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

// === REEMPLAZAR EL kFS COMPLETO (LÍNEAS 111-372) CON ESTO ===
static const char* kFS = R"(#version 330 core
out vec4 FragColor;
in vec3 vPos;

uniform float uTime;
uniform float uSun;
uniform vec3  uSunDir;
uniform vec3  uCamp;      // reservado
uniform int   uMode;      // 0=fuego 1=madera 2=cerámica 3/4/6 colores 5=tejido 10=grassProc 11=sky 12=grassTex 13=hojas 14/15/16 flor 17-21=mascara
uniform sampler2D uTex;   // para pasto texturizado
uniform float uTexScale;  // tiling base
uniform float uSeed;      // semilla per-flama
uniform float uFlicker;   // factor de parpadeo externo

// === NUEVO: Uniforms para la luz de la fogata ===
uniform vec3  uFirePos;   // Posición de la fogata
uniform vec3  uFireColor; // Color e intensidad (será vec3(0,0,0) si está apagada)

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

// === NUEVO: Función de iluminación de la fogata ===
vec3 CalcFireLight(vec3 pos, vec3 normal, vec3 albedo) {
    if (uFireColor.r < 0.01) return vec3(0.0); // Luz apagada

    vec3 lightDir = normalize(uFirePos - pos);
    float diff = max(dot(normal, lightDir), 0.0);
    
    float dist = length(uFirePos - pos);
    float atten = 1.0 / (1.0 + 0.05 * dist + 0.015 * dist * dist); 
    
    return albedo * uFireColor * diff * atten; 
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
    // === NUEVO: Calcular normal plana para objetos procedurales ===
    vec3 flatNormal = normalize(cross(dFdx(vPos), dFdy(vPos)));
    
    // === MODIFICADOS: Añadida luz de fogata a todos los modos ===

    if(uMode==1){ 
        vec3 col = wood(vPos);
        col += CalcFireLight(vPos, flatNormal, col);
        FragColor=vec4(col,1.0); 
        return; 
    } 

    // Fuego (cono con alpha + parpadeo) - No se ilumina a sí mismo
    if(uMode==0){
        float h=clamp(vPos.y,0.0,1.0);
        float r=length(vPos.xz);
        float edge = 1.0 - smoothstep(0.15, 0.35, r);
        float t=uTime*2.0 + uSeed*0.37;
        float flick = noise(vec2(r*6.0,h*8.0+t*3.5))*0.6
                    + noise(vec2(r*10.0+t*1.7,h*5.0))*0.4;
        flick *= 0.95 * uFlicker; // Usamos flicker externo
        vec3 c1=vec3(1.0,0.25,0.02), c2=vec3(1.0,0.55,0.05),
             c3=vec3(1.0,0.85,0.25), c4=vec3(1.0,0.95,0.75);
        float k=clamp(h*1.2+flick*0.2,0.0,1.0);
        vec3 col=mix(c1,c2,k); col=mix(col,c3,k*k); col=mix(col,c4,pow(k,4.0));
        float a = edge*(0.85 + 0.25*sin(t*7.0 + r*10.0));
        a *= (0.55 + 0.60*h);
        a = clamp(a * 1.25, 0.0, 1.0);
        FragColor=vec4(col,a); return;
    }

    // Tejido (asiento)
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
        base += CalcFireLight(vPos, flatNormal, base);
        FragColor=vec4(base,1.0); return;
    }

    // Cerámica / florero
    if(uMode==2){
        vec3 terracotta=vec3(0.63,0.28,0.20);
        vec3 crema=vec3(0.90,0.82,0.72);
        float b=smoothstep(-0.15,0.15,sin(vPos.y*18.0));
        vec3 col=mix(terracotta,crema,b*0.25);
        if(abs(vPos.y-0.15)<0.06) col=mix(col,vec3(0.14,0.02,0.02),0.85);
        if(abs(vPos.y-0.00)<0.06) col=mix(col,vec3(0.14,0.02,0.02),0.85);
        col += CalcFireLight(vPos, flatNormal, col);
        FragColor=vec4(col,1.0); return;
    }

    // Colores varios
    if(uMode==3){ vec3 col=vec3(0.84,0.72,0.54); col+=CalcFireLight(vPos,flatNormal,col); FragColor=vec4(col,1.0); return; }
    if(uMode==4){ vec3 col=vec3(0.10,0.07,0.06); col+=CalcFireLight(vPos,flatNormal,col); FragColor=vec4(col,1.0); return; }
    if(uMode==6){ vec3 col=vec3(0.88,0.42,0.55); col+=CalcFireLight(vPos,flatNormal,col); FragColor=vec4(col,1.0); return; }

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
        
        vec3 fireLight = CalcFireLight(vPos, vec3(0,1,0), col);
        col = col * light + fireLight; // Sol multiplica, fogata suma

        FragColor = vec4(col, 1.0); return;
    }

    // Cielo + horizonte (no se ilumina por fogata)
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
        float elev = mix(-0.10, 0.18, ridge);
        float m = 1.0 - smoothstep(elev-0.008, elev+0.008, dir.y);
        m *= (1.0 - smoothstep(0.10, 0.35, dir.y));
        vec3 mountDay   = vec3(0.28,0.30,0.34);
        vec3 mountDusk  = vec3(0.20,0.18,0.22);
        vec3 mountNight = vec3(0.08,0.09,0.12);
        vec3 mountCol   = mix(mountNight, mix(mountDusk, mountDay, clamp(uSun,0.0,1.0)), clamp(uSun,0.0,1.0));
        vec3 sky = base + sunCol + moonCol + starCol;
        vec3 finalCol = mix(sky, mountCol, m);
        FragColor = vec4(finalCol, 1.0); return;
    }

    // Pasto TEXTURIZADO anti-tiling
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

        vec3 fireLight = CalcFireLight(vPos, vec3(0,1,0), albedo);
        FragColor = vec4(albedo * light + fireLight, 1.0);
        return;
    }
    
    // Máscara de Jade
    if(uMode==17){ vec3 col=vec3(0.30, 0.82, 0.70); col+=CalcFireLight(vPos,flatNormal,col); FragColor = vec4(col, 1.0); return; } // jade claro
    if(uMode==18){ vec3 col=vec3(0.12, 0.40, 0.33); col+=CalcFireLight(vPos,flatNormal,col); FragColor = vec4(col, 1.0); return; } // jade oscuro
    if(uMode==19){ vec3 col=vec3(0.83, 0.35, 0.25); col+=CalcFireLight(vPos,flatNormal,col); FragColor = vec4(col, 1.0); return; } // rojo coral
    if(uMode==20){ vec3 col=vec3(0.95, 0.95, 0.90); col+=CalcFireLight(vPos,flatNormal,col); FragColor = vec4(col, 1.0); return; } // blanco ojos
    if(uMode==21){ vec3 col=vec3(0.05, 0.05, 0.05); col+=CalcFireLight(vPos,flatNormal,col); FragColor = vec4(col, 1.0); return; } // negro detalles

    // Flor (tallo/hojas, pétalos, centro)
    if(uMode==14){ vec3 col=vec3(0.10, 0.45, 0.14); col+=CalcFireLight(vPos,flatNormal,col); FragColor = vec4(col, 1.0); return; } // tallo/hojas
    if(uMode==15){ vec3 col=vec3(0.95, 0.60, 0.75); col+=CalcFireLight(vPos,flatNormal,col); FragColor = vec4(col, 1.0); return; } // pétalo rosado
    if(uMode==16){ vec3 col=vec3(0.98, 0.85, 0.25); col+=CalcFireLight(vPos,flatNormal,col); FragColor = vec4(col, 1.0); return; } // centro amarillo

    // Hojas de árbol (canopy)
    if(uMode==13){
        float n = 0.35*noise(vPos.xz*0.7) + 0.20*noise(vPos.xz*2.1) + 0.10*noise(vPos.xz*4.3);
        vec3 leaf1 = vec3(0.07, 0.30, 0.06);
        vec3 leaf2 = vec3(0.18, 0.52, 0.16);
        vec3 col   = mix(leaf1, leaf2, clamp(0.55 + n, 0.0, 1.0));
        float lambert = clamp(dot(normalize(vec3(0,1,0)), normalize(uSunDir)), 0.0, 1.0);
        float sunVis  = clamp(uSun, 0.0, 1.0);
        float ambient = mix(0.35, 0.55, sunVis);
        float light   = ambient + lambert * mix(0.45, 0.95, sunVis);
        
        vec3 fireLight = CalcFireLight(vPos, flatNormal, col);
        col = col * light * (0.92 + 0.08*noise(vPos.xz*3.7)) + fireLight;
        
        FragColor = vec4(col, 1.0);
        return;
    }
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

// ------------------ Dispersión ------------------
struct Exclusion { glm::vec2 centerXZ; float radius; };
static bool IsFarEnough(const glm::vec2& p, const std::vector<glm::vec2>& used, float minDistSq) {
    for (const auto& u : used) { glm::vec2 d = p - u; if (glm::dot(d, d) < minDistSq) return false; }
    return true;
}
static bool OutsideExclusions(const glm::vec2& p, const std::vector<Exclusion>& ex) {
    for (const auto& e : ex) { glm::vec2 d = p - e.centerXZ; if (glm::dot(d, d) < (e.radius * e.radius)) return false; }
    return true;
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
    int seg = 64; v.reserve((int)((prof.size() - 1) * seg * 6));
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

static void BuildGround(float S = 220.0f) {
    float v[] = { -S,0.0f,-S,  S,0.0f,-S,  S,0.0f, S,
                  -S,0.0f,-S,  S,0.0f, S, -S,0.0f, S };
    gGroundVerts = 6;
    glGenVertexArrays(1, &gVAOGround);
    glGenBuffers(1, &gVBOGround);
    glBindVertexArray(gVAOGround);
    glBindBuffer(GL_ARRAY_BUFFER, gVBOGround);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);
}

// === FUNCIÓN NUEVA ===
static void BuildCone(int slices = 16) {
    std::vector<glm::vec3> v;
    v.reserve(slices * 6);
    const float R = 0.5f, H = 1.0f;
    // Lateral
    for (int i = 0; i < slices; ++i) {
        float a0 = (2.0f * 3.14159265f * i) / slices;
        float a1 = (2.0f * 3.14159265f * (i + 1)) / slices;
        glm::vec3 apex(0.0f, H, 0.0f);
        glm::vec3 p0(R * cosf(a0), 0.0f, R * sinf(a0));
        glm::vec3 p1(R * cosf(a1), 0.0f, R * sinf(a1));
        v.push_back(apex); v.push_back(p0); v.push_back(p1);
    }

  

    // Base
    for (int i = 0; i < slices; ++i) {
        float a0 = (2.0f * 3.14159265f * i) / slices;
        float a1 = (2.0f * 3.14159265f * (i + 1)) / slices;
        glm::vec3 c(0.0f, 0.0f, 0.0f);
        glm::vec3 p0(R * cosf(a0), 0.0f, R * sinf(a0));
        glm::vec3 p1(R * cosf(a1), 0.0f, R * sinf(a1));
        v.push_back(c); v.push_back(p1); v.push_back(p0);
    }
    glGenVertexArrays(1, &gVAOCone);
    glGenBuffers(1, &gVBOCone);
    glBindVertexArray(gVAOCone);
    glBindBuffer(GL_ARRAY_BUFFER, gVBOCone);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(glm::vec3), v.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glBindVertexArray(0);
    gConeVerts = (GLsizei)v.size();
}
static void BuildSphere(int segments = 32, int rings = 16) {
    std::vector<glm::vec3> v;
    v.reserve(rings * segments * 6); // Pre-alocar memoria

    const float PI = 3.1415926535f;
    float R = 0.5f; // Radio 0.5, para que el diámetro sea 1.0 (como el cubo)

    for (int i = 0; i < rings; ++i) {
        float phi0 = PI * (float)i / rings;       // Ángulo para este anillo
        float phi1 = PI * (float)(i + 1) / rings; // Ángulo para el siguiente

        float y0 = R * cosf(phi0);
        float r0 = R * sinf(phi0);
        float y1 = R * cosf(phi1);
        float r1 = R * sinf(phi1);

        for (int j = 0; j < segments; ++j) {
            float theta0 = (2.0f * PI * j) / segments;       // Ángulo para este segmento
            float theta1 = (2.0f * PI * (j + 1)) / segments; // Ángulo para el siguiente

            // Calculamos los 4 vértices del "quad"
            glm::vec3 p0(r0 * cosf(theta0), y0, r0 * sinf(theta0));
            glm::vec3 p1(r0 * cosf(theta1), y0, r0 * sinf(theta1));
            glm::vec3 p2(r1 * cosf(theta1), y1, r1 * sinf(theta1));
            glm::vec3 p3(r1 * cosf(theta0), y1, r1 * sinf(theta0));

            // Añadimos los dos triángulos que forman el quad
            v.push_back(p0); v.push_back(p1); v.push_back(p2);
            v.push_back(p0); v.push_back(p2); v.push_back(p3);
        }
    }
    gSphereVerts = (GLsizei)v.size();
    glGenVertexArrays(1, &gVAOSphere);
    glGenBuffers(1, &gVBOSphere);
    glBindVertexArray(gVAOSphere);
    glBindBuffer(GL_ARRAY_BUFFER, gVBOSphere);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(glm::vec3), v.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glBindVertexArray(0);
}

// ===========================================================
// main
// ===========================================================
int main() {
    // GLFW
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

    // Shaders/modelos
    Shader shader("Shader/modelLoading.vs", "Shader/modelLoading.frag");
    Model CanastaChiles((char*)"Models/CanastaChiles.obj");
    Model Chiles((char*)"Models/Chiles.obj");
    Model PetatesTianguis((char*)"Models/PetatesTianguis.obj");
    Model Aguacates((char*)"Models/Aguacates.obj");
    Model Jarrones((char*)"Models/Jarrones.obj");
    Model Tendedero((char*)"Models/Tendedero.obj");
    Model PielJaguar((char*)"Models/PielJaguar.obj");


    Model TechosChozas((char*)"Models/TechosChozas.obj");
    Model ParedesChozas((char*)"Models/ParedesChozas.obj");
    Model tula((char*)"Models/tula.obj");
    Model ar((char*)"Models/arbol.obj");
    Model ca((char*)"Models/10436_Cactus_v1_max2010_it2.obj");
    Model piramidesol((char*)"Models/PiramideSol.obj");
    Model perro((char*)"Models/perro.obj");
    Model JuegoPelota((char*)"Models/Juego_Pelota.obj");
    Model corn((char*)"Models/10439_Corn_Field_v1_max2010_it2.obj");
    Model PielesPiso((char*)"Models/PielesPiso.obj");
    Model Piramide((char*)"Models/Piramide.obj");
    Model VasijasYMolcajete((char*)"Models/VasijasYMolcajete.obj");
    Model Tunas((char*)"Models/Tunas.obj");
    Model Vasijas((char*)"Models/Vasijas.obj");
    Model CasaGrande((char*)"Models/CasaGrande.obj");
    Model FuegoCocinaCG((char*)"Models/FuegoCocinaCG.obj");
    Model ArbolTianguis((char*)"Models/MaicesCampo.obj");
    Model CasaAmue((char*)"Models/CocinaCasa.obj");

    // Programa procedural + geometrías
    CreateProgram();
    BuildCube();
    BuildSeatPlane();
    BuildVase();
    BuildGround();
    BuildCone(14);
    BuildSphere();
    // Texturas
    stbi_set_flip_vertically_on_load(0);
    gTexGrass = LoadTexture2D("Models/pasto.jpg", true);

    // Proyección
    glm::mat4 projection = glm::perspective(camera.GetZoom(),
        (GLfloat)SCREEN_WIDTH / (GLfloat)SCREEN_HEIGHT, 0.1f, 1000.0f);

    // Dimensiones mesa/silla
    const float topX = 1.6f, topZ = 3.0f, topY = 0.12f;
    const float legH = 0.90f, legT = 0.09f, railT = 0.06f, railH = 0.32f;
    const float seat = 0.60f, tLeg = 0.06f;

    gChairPos = gTablePos + glm::vec3(topX * 0.5f + 0.45f + seat * 0.5f, 0.0f, -topZ * 0.25f);

    // --------- Instancias aleatorias de árboles ('ar') — alejados de la zona central ---------
    {
        // Dos “cinturones”: izquierda-fondo y derecha-fondo
        const glm::vec2 X_RANGE_L(-150.0f, -80.0f);
        const glm::vec2 Z_RANGE_L(-150.0f, -80.0f);

        const glm::vec2 X_RANGE_R(80.0f, 150.0f);
        const glm::vec2 Z_RANGE_R(-150.0f, -80.0f);

        // Excluir áreas cercanas a mesa/campamento y a los monumentos
        std::vector<Exclusion> ex = {
            { glm::vec2(gTablePos.x,  gTablePos.z), 18.0f },  // mesa
            { glm::vec2(gCampPos.x,   gCampPos.z), 14.0f },  // fogata
            { glm::vec2(-60.0f,        -95.0f), 22.0f }, // Tula (según bloque que movimos)
            { glm::vec2(75.0f,       -145.0f), 28.0f }, // Pirámide del Sol
            { glm::vec2(0.0f,          0.0f), 40.0f }  // área central amplia
        };

        const float MIN_DIST = 7.0f;                 // separación entre árboles
        const float MIN_DIST2 = MIN_DIST * MIN_DIST;
        const int   MAX_TRIES = 12000;
        const float Y_ROT_MIN = 0.0f, Y_ROT_MAX = 360.0f;
        const float S_MIN = 0.85f, S_MAX = 1.45f; // alturas más contenidas

        std::mt19937 rng(20251108);
        std::uniform_real_distribution<float> distYaw(Y_ROT_MIN, Y_ROT_MAX);
        std::uniform_real_distribution<float> distS(S_MIN, S_MAX);

        auto fillBelt = [&](glm::vec2 XR, glm::vec2 ZR, int target) {
            std::uniform_real_distribution<float> distX(XR.x, XR.y);
            std::uniform_real_distribution<float> distZ(ZR.x, ZR.y);

            std::vector<glm::vec2> usedXZ; usedXZ.reserve(target);
            int placed = 0, tries = 0;
            while (tries < MAX_TRIES && placed < target) {
                ++tries;
                glm::vec2 p(distX(rng), distZ(rng));
                if (!OutsideExclusions(p, ex)) continue;
                if (!IsFarEnough(p, usedXZ, MIN_DIST2)) continue;

                usedXZ.push_back(p);
                float yaw = distYaw(rng);
                float s = distS(rng);

                glm::mat4 M(1.0f);
                M = glm::translate(M, glm::vec3(p.x, 0.0f, p.y));
                M = glm::rotate(M, glm::radians(yaw), glm::vec3(0, 1, 0));
                M = glm::scale(M, glm::vec3(s));

                gArModels.push_back(M);
                ++placed;
            }
            };

        gArModels.clear();
        gArModels.reserve(AR_COUNT);

        // Distribución 50/50 entre ambos cinturones
        int leftCount = AR_COUNT / 2;
        int rightCount = AR_COUNT - leftCount;

        fillBelt(X_RANGE_L, Z_RANGE_L, leftCount);
        fillBelt(X_RANGE_R, Z_RANGE_R, rightCount);

        // Relleno (si faltó alguno) usando el cinturón izquierdo
        std::uniform_real_distribution<float> distX_L(X_RANGE_L.x, X_RANGE_L.y);
        std::uniform_real_distribution<float> distZ_L(Z_RANGE_L.x, Z_RANGE_L.y);
        while ((int)gArModels.size() < AR_COUNT) {
            glm::vec2 p(distX_L(rng), distZ_L(rng));
            if (!OutsideExclusions(p, ex)) continue;
            float yaw = distYaw(rng);
            float s = distS(rng);
            glm::mat4 M(1.0f);
            M = glm::translate(M, glm::vec3(p.x, 0.0f, p.y));
            M = glm::rotate(M, glm::radians(yaw), glm::vec3(0, 1, 0));
            M = glm::scale(M, glm::vec3(s));
            gArModels.push_back(M);
        }
    }


    // --------- Instancias aleatorias de cactus ('ca') ----------
    {
        const glm::vec2 X_RANGE_L(-150.0f, -80.0f);
        const glm::vec2 Z_RANGE_L(-150.0f, -80.0f);

        const glm::vec2 X_RANGE_R(80.0f, 150.0f);
        const glm::vec2 Z_RANGE_R(-150.0f, -80.0f);

        // Excluir áreas cercanas a mesa/campamento y a los monumentos
        std::vector<Exclusion> ex = {
            { glm::vec2(gTablePos.x,  gTablePos.z), 18.0f },
            { glm::vec2(gCampPos.x,   gCampPos.z), 14.0f },
            { glm::vec2(-60.0f,       -95.0f),     22.0f }, // Tula
            { glm::vec2(75.0f,        -145.0f),    28.0f }, // Pirámide del Sol
            { glm::vec2(0.0f,          0.0f),      40.0f }  // área central despejada
        };

        const float MIN_DIST = 6.0f;
        const float MIN_DIST2 = MIN_DIST * MIN_DIST;
        const int   MAX_TRIES = 100;
        const float Y_ROT_MIN = 0.0f, Y_ROT_MAX = 360.0f;
        const float S_MIN = 0.05f, S_MAX = 0.12f;

        std::mt19937 rng(20251107);
        std::uniform_real_distribution<float> distYaw(Y_ROT_MIN, Y_ROT_MAX);
        std::uniform_real_distribution<float> distS(S_MIN, S_MAX);

        auto fillBelt = [&](glm::vec2 XR, glm::vec2 ZR, int target) {
            std::uniform_real_distribution<float> distX(XR.x, XR.y);
            std::uniform_real_distribution<float> distZ(ZR.x, ZR.y);

            std::vector<glm::vec2> usedXZ; usedXZ.reserve(target);
            int placed = 0, tries = 0;
            while (tries < MAX_TRIES && placed < target) {
                ++tries;
                glm::vec2 p(distX(rng), distZ(rng));
                if (!OutsideExclusions(p, ex)) continue;
                if (!IsFarEnough(p, usedXZ, MIN_DIST2)) continue;

                usedXZ.push_back(p);
                float yaw = distYaw(rng);
                float s = distS(rng);

                glm::mat4 M(1.0f);
                M = glm::translate(M, glm::vec3(p.x, 0.0f, p.y));
                M = glm::rotate(M, glm::radians(yaw), glm::vec3(0, 1, 0)); // solo rotación en Y
                M = glm::scale(M, glm::vec3(s));

                gcaModels.push_back(M);
                ++placed;
            }
            };

        gcaModels.clear();
        gcaModels.reserve(CA_COUNT);

        int leftCount = (int)std::round(CA_COUNT * 0.60f);
        int rightCount = CA_COUNT - leftCount;

        fillBelt(X_RANGE_L, Z_RANGE_L, leftCount);
        fillBelt(X_RANGE_R, Z_RANGE_R, rightCount);

        // Relleno (si faltan cactus)
        std::uniform_real_distribution<float> distX_L(X_RANGE_L.x, X_RANGE_L.y);
        std::uniform_real_distribution<float> distZ_L(Z_RANGE_L.x, Z_RANGE_L.y);
        while ((int)gcaModels.size() < CA_COUNT) {
            glm::vec2 p(distX_L(rng), distZ_L(rng));
            if (!OutsideExclusions(p, ex)) continue;
            float yaw = distYaw(rng);
            float s = distS(rng);

            glm::mat4 M(1.0f);
            M = glm::translate(M, glm::vec3(p.x, 0.0f, p.y));
            M = glm::rotate(M, glm::radians(yaw), glm::vec3(0, 1, 0)); // sin inclinarlos
            M = glm::scale(M, glm::vec3(s));
            gcaModels.push_back(M);
        }
    }

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

        // === para fuego ===
        glm::vec3 fireColor(0.0f);
        float fireFlicker = 1.0f;
        if (gFireOn) {
            fireFlicker = 0.85f + 0.25f * sinf(currentFrame * 7.0f);
            fireColor = glm::vec3(1.0f, 0.45f, 0.1f) * fireFlicker * 2.5f; // Intensidad 2.5
        }
        glm::vec3 firePos = gCampPos + glm::vec3(0.0f, 0.2f, 0.0f);
       
        // ---------------- Cielo ----------------
        {
            glUseProgram(gProg);
            glm::mat4 viewNoTrans = glm::mat4(glm::mat3(view));
            glUniformMatrix4fv(glGetUniformLocation(gProg, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
            glUniformMatrix4fv(glGetUniformLocation(gProg, "view"), 1, GL_FALSE, glm::value_ptr(viewNoTrans));
            glUniform1f(glGetUniformLocation(gProg, "uTime"), currentFrame);
            glUniform1f(glGetUniformLocation(gProg, "uSun"), sun);
            glUniform3fv(glGetUniformLocation(gProg, "uSunDir"), 1, glm::value_ptr(sunDir));
            glUniform3fv(glGetUniformLocation(gProg, "uFirePos"), 1, glm::value_ptr(firePos));
            glUniform3fv(glGetUniformLocation(gProg, "uFireColor"), 1, glm::value_ptr(fireColor));
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

        // ---------------- Suelo (pasto texturizado) ----------------
        glUseProgram(gProg);
        glUniformMatrix4fv(glGetUniformLocation(gProg, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(gProg, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniform1f(glGetUniformLocation(gProg, "uTime"), currentFrame);
        glUniform1f(glGetUniformLocation(gProg, "uSun"), sun);
        glUniform3fv(glGetUniformLocation(gProg, "uSunDir"), 1, glm::value_ptr(sunDir));
        glUniform3fv(glGetUniformLocation(gProg, "uFirePos"), 1, glm::value_ptr(firePos));
        glUniform3fv(glGetUniformLocation(gProg, "uFireColor"), 1, glm::value_ptr(fireColor));
        glUniform1i(glGetUniformLocation(gProg, "uMode"), 12);
        glUniform1f(glGetUniformLocation(gProg, "uTexScale"), 0.28f);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gTexGrass);
        glUniform1i(glGetUniformLocation(gProg, "uTex"), 0);

        glm::mat4 MG(1.0f);
        MG = glm::translate(MG, glm::vec3(0.0f, -0.001f, 0.0f));
        glUniformMatrix4fv(glGetUniformLocation(gProg, "model"), 1, GL_FALSE, glm::value_ptr(MG));
        glBindVertexArray(gVAOGround);
        glDrawArrays(GL_TRIANGLES, 0, gGroundVerts);
        glBindVertexArray(0);
        glUseProgram(0);

        // =======================================================
        // MODELOS DEL TIANGUIS (shader externo)
        // =======================================================
        shader.Use();

        // Proyección y vista
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "view"), 1, GL_FALSE, glm::value_ptr(view));

        // Luz direccional basada en el sol (nombres compatibles)
        auto U = [&](const char* n) { return glGetUniformLocation(shader.Program, n); };
        glm::vec3 Ldir = -sunDir;
        float amb = glm::mix(0.05f, 0.22f, sun);
        float dif = glm::mix(0.10f, 1.00f, sun);
        float spe = glm::mix(0.05f, 0.50f, sun);
        glUniform3fv(U("viewPos"), 1, glm::value_ptr(camera.GetPosition()));
        glUniform3fv(U("dirLight.direction"), 1, glm::value_ptr(Ldir));
        glUniform3f(U("dirLight.ambient"), amb, amb, amb);
        glUniform3f(U("dirLight.diffuse"), dif, dif, dif);
        glUniform3f(U("dirLight.specular"), spe, spe, spe);
        glUniform3fv(U("light.direction"), 1, glm::value_ptr(Ldir));
        glUniform3f(U("light.ambient"), amb, amb, amb);
        glUniform3f(U("light.diffuse"), dif, dif, dif);
        glUniform3f(U("light.specular"), spe, spe, spe);

        // --- Luz de Fogata (Punto 0) ---
        glUniform3fv(U("pointLights[0].position"), 1, glm::value_ptr(firePos));
        glUniform3fv(U("pointLights[0].diffuse"), 1, glm::value_ptr(fireColor));
        glUniform3f(U("pointLights[0].ambient"), fireColor.r * 0.05f, fireColor.g * 0.05f, fireColor.b * 0.05f);
        glUniform3f(U("pointLights[0].specular"), 1.0f, 1.0f, 1.0f);
        glUniform1f(U("pointLights[0].constant"), 1.0f);
        glUniform1f(U("pointLights[0].linear"), 0.05f);      // Atenuación para modelos
        glUniform1f(U("pointLights[0].quadratic"), 0.015f);  // Atenuación para modelos

        // Apagar las otras 3 luces (si el shader las soporta)
        glUniform3f(U("pointLights[1].diffuse"), 0.0f, 0.0f, 0.0f);
        glUniform3f(U("pointLights[2].diffuse"), 0.0f, 0.0f, 0.0f);
        glUniform3f(U("pointLights[3].diffuse"), 0.0f, 0.0f, 0.0f);
        // === FIN DEL BLOQUE NUEVO ===

        // Dibujo de props del tianguis
        {
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

            glm::mat4 model13(1.0f);
            glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model13));
            JuegoPelota.Draw(shader);

            glm::mat4 model16(1.0f);
            glUniformMatrix4fv(U("model"), 1, GL_FALSE, glm::value_ptr(model16));
            ParedesChozas.Draw(shader);
            glm::mat4 model17(1.0f);
            glUniformMatrix4fv(U("model"), 1, GL_FALSE, glm::value_ptr(model17));
            TechosChozas.Draw(shader);

            glm::mat4 model18(1.0f);
            glUniformMatrix4fv(U("model"), 1, GL_FALSE, glm::value_ptr(model18));
            VasijasYMolcajete.Draw(shader);

            glm::mat4 model19(1.0f);
            glUniformMatrix4fv(U("model"), 1, GL_FALSE, glm::value_ptr(model19));
            Tunas.Draw(shader);

            glm::mat4 model20(1.0f);
            glUniformMatrix4fv(U("model"), 1, GL_FALSE, glm::value_ptr(model20));
            Vasijas.Draw(shader);

            glm::mat4 model23(1.0f);
            glUniformMatrix4fv(U("model"), 1, GL_FALSE, glm::value_ptr(model23));
            CasaGrande.Draw(shader);

            glm::mat4 model24(1.0f);
            glUniformMatrix4fv(U("model"), 1, GL_FALSE, glm::value_ptr(model24));
            FuegoCocinaCG.Draw(shader);
          

            glm::mat4 model25(1.0f);
            glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model25));
            ArbolTianguis.Draw(shader);


            glm::mat4 model26(1.0f);
            glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model26));
            CasaAmue.Draw(shader);
        }
        // --------- Instancias aleatorias de maíz ('co') ----------
        {
            const glm::vec2 X_RANGE_L(-40.0f, 0.0f);
            const glm::vec2 Z_RANGE_L(-180.0f, -120.0f);

            const glm::vec2 X_RANGE_R(0.0f, 40.0f);
            const glm::vec2 Z_RANGE_R(-180.0f, -120.0f);

            std::vector<Exclusion> ex = {
                { glm::vec2(gTablePos.x,  gTablePos.z), 18.0f },
                { glm::vec2(gCampPos.x,   gCampPos.z), 14.0f },
                { glm::vec2(-60.0f,       -95.0f),     22.0f }, // Tula
                { glm::vec2(25.0f,       -145.0f),     28.0f }, // Pirámide del Sol
                { glm::vec2(0.0f,          0.0f),      40.0f }  // área central
            };

            const float MIN_DIST = 3.5f;
            const float MIN_DIST2 = MIN_DIST * MIN_DIST;
            const int   MAX_TRIES = 200;
            const float Y_ROT_MIN = 0.0f, Y_ROT_MAX = 360.0f;
            const float S_MIN = 0.06f, S_MAX = 0.10f;

            std::mt19937 rng(20251109);
            std::uniform_real_distribution<float> distYaw(Y_ROT_MIN, Y_ROT_MAX);
            std::uniform_real_distribution<float> distS(S_MIN, S_MAX);

            auto fillBeltCorn = [&](glm::vec2 XR, glm::vec2 ZR, int target) {
                std::uniform_real_distribution<float> distX(XR.x, XR.y);
                std::uniform_real_distribution<float> distZ(ZR.x, ZR.y);

                std::vector<glm::vec2> usedXZ;
                usedXZ.reserve(target);
                int placed = 0, tries = 0;

                while (tries < MAX_TRIES && placed < target) {
                    ++tries;
                    glm::vec2 p(distX(rng), distZ(rng));
                    if (!OutsideExclusions(p, ex)) continue;
                    if (!IsFarEnough(p, usedXZ, MIN_DIST2)) continue;

                    usedXZ.push_back(p);
                    float yaw = distYaw(rng);
                    float s = distS(rng);

                    glm::mat4 M(1.0f);
                    M = glm::translate(M, glm::vec3(p.x, 0.0f, p.y));
                    M = glm::rotate(M, glm::radians(yaw), glm::vec3(0, 1, 0));
                    M = glm::scale(M, glm::vec3(s));

                    gCoModels.push_back(M);
                    ++placed;
                }
                };
            {
                // Pirámide (cercana)
                glm::mat4 model9(1.0f);
                model9 = glm::translate(model9, glm::vec3(90.0f, 1.0f, -30.0f));
                model9 = glm::scale(model9, glm::vec3(0.7f));
                glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model9));
                Piramide.Draw(shader);


                glm::mat4 model10(1.0f);
                glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model10));
                tula.Draw(shader);

                // --- PIRÁMIDE DEL SOL ---
                glm::mat4 model11(1.0f);
                model11 = glm::translate(model11, glm::vec3(+25.0f, 0.0f, -145.0f)); // nueva posición al fondo derecho
                model11 = glm::rotate(model11, glm::radians(10.0f), glm::vec3(0.0f, 1.0f, 0.0f)); // ligera orientación
                model11 = glm::scale(model11, glm::vec3(1.4f));   // escala acorde a la distancia
                glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model11));
                piramidesol.Draw(shader);
            }
            gCoModels.clear();
            gCoModels.reserve(CO_COUNT);

            int leftCount = CO_COUNT / 2;
            int rightCount = CO_COUNT - leftCount;

            fillBeltCorn(X_RANGE_L, Z_RANGE_L, leftCount);
            fillBeltCorn(X_RANGE_R, Z_RANGE_R, rightCount);

            // Relleno (por si faltan algunos)
            std::uniform_real_distribution<float> distX_L(X_RANGE_L.x, X_RANGE_L.y);
            std::uniform_real_distribution<float> distZ_L(Z_RANGE_L.x, Z_RANGE_L.y);
            while ((int)gCoModels.size() < CO_COUNT) {
                glm::vec2 p(distX_L(rng), distZ_L(rng));
                if (!OutsideExclusions(p, ex)) continue;

                float yaw = distYaw(rng);
                float s = distS(rng);

                glm::mat4 M(1.0f);
                M = glm::translate(M, glm::vec3(p.x, 0.0f, p.y));
                M = glm::rotate(M, glm::radians(yaw), glm::vec3(0, 1, 0));
                M = glm::scale(M, glm::vec3(s));
                gCoModels.push_back(M);
            }
        }

        // ---------------- Cactus (enderezados con Rfix) ----------------
        {
            const float kScaleJitterXY = 0.10f;
            const float kScaleJitterY = 0.25f;
            const float kTiltMaxDeg = 2.0f;
            const float kYawJitterDeg = 8.0f;
            const float kYOffset = 0.02f;
            const float kCullDistance = 220.0f;

            auto rand01 = [](uint32_t seed) {
                float s = std::sin(seed * 12.9898f) * 43758.5453f;
                return s - std::floor(s);
                };

            glm::mat4 Rfix(1.0f);

            Rfix = glm::rotate(Rfix, glm::radians(-90.0f), glm::vec3(1, 0, 0));

            int idx = 0;
            for (const glm::mat4& M : gcaModels) {
                glm::vec3 posWorld = glm::vec3(M[3]);
                if (glm::distance(posWorld, camera.GetPosition()) > kCullDistance) { ++idx; continue; }

                float r0 = rand01(idx * 3u + 0u);
                float r1 = rand01(idx * 3u + 1u);
                float r2 = rand01(idx * 3u + 2u);

                float sx = 1.0f + (r0 - 0.5f) * kScaleJitterXY * 2.0f;
                float sy = 1.0f + (r1 - 0.5f) * kScaleJitterY * 2.0f;
                float sz = 1.0f + (r2 - 0.5f) * kScaleJitterXY * 2.0f;

                float rx = (r0 - 0.5f) * kTiltMaxDeg * 2.0f;
                float rz = (r1 - 0.5f) * kTiltMaxDeg * 2.0f;
                float ry = (r2 - 0.5f) * kYawJitterDeg * 2.0f;

                glm::mat4 tweak(1.0f);
                tweak = glm::translate(tweak, glm::vec3(0.0f, kYOffset, 0.0f));
                tweak = glm::rotate(tweak, glm::radians(rx), glm::vec3(1, 0, 0));
                tweak = glm::rotate(tweak, glm::radians(ry), glm::vec3(0, 1, 0));
                tweak = glm::rotate(tweak, glm::radians(rz), glm::vec3(0, 0, 1));
                tweak = glm::scale(tweak, glm::vec3(sx, sy, sz));

                glm::mat4 M_final = M * Rfix * tweak;

                glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(M_final));
                ca.Draw(shader);

                ++idx;
            }
        }
       

       
        

        // ===== Árboles =====
        for (const glm::mat4& M : gArModels) {
            glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(M));
            ar.Draw(shader);
        }

        // ===== Campo de maíz =====
        {
            glm::mat4 RfixCorn(1.0f);
            RfixCorn = glm::rotate(RfixCorn, glm::radians(-90.0f), glm::vec3(1, 0, 0)); // levantarlo

            for (const glm::mat4& M : gCoModels) {
                glm::mat4 Mfinal = M * RfixCorn;
                glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"),
                    1, GL_FALSE, glm::value_ptr(Mfinal));
                corn.Draw(shader);
            }
        }



        // -------- Procedural (mesa, silla, florero + flor) con gProg
        glUseProgram(gProg);
        glUniformMatrix4fv(glGetUniformLocation(gProg, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(gProg, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniform1f(glGetUniformLocation(gProg, "uTime"), currentFrame);
        glUniform1f(glGetUniformLocation(gProg, "uSun"), sun);
        glUniform3fv(glGetUniformLocation(gProg, "uSunDir"), 1, glm::value_ptr(sunDir));

        glUniform3fv(glGetUniformLocation(gProg, "uFirePos"), 1, glm::value_ptr(firePos));
        glUniform3fv(glGetUniformLocation(gProg, "uFireColor"), 1, glm::value_ptr(fireColor));
        auto drawCubeAt = [&](glm::vec3 pos, glm::vec3 scl, int mode = 1) {
            glm::mat4 MM(1.0f); MM = glm::translate(MM, pos); MM = glm::scale(MM, scl);
            glUniformMatrix4fv(glGetUniformLocation(gProg, "model"), 1, GL_FALSE, glm::value_ptr(MM));
            glUniform1i(glGetUniformLocation(gProg, "uMode"), mode);
            glBindVertexArray(gVAOCube); glDrawArrays(GL_TRIANGLES, 0, gCubeVerts);
            };

        auto drawCubeAtRP = [&](glm::vec3 pos, glm::vec3 scl, float yawDeg, float pitchDeg, int mode = 1) {
            glm::mat4 M(1.0f);
            M = glm::translate(M, pos);
            M = glm::rotate(M, glm::radians(yawDeg), glm::vec3(0, 1, 0));
            M = glm::rotate(M, glm::radians(pitchDeg), glm::vec3(1, 0, 0));
            M = glm::scale(M, scl);
            glUniformMatrix4fv(glGetUniformLocation(gProg, "model"), 1, GL_FALSE, glm::value_ptr(M));
            glUniform1i(glGetUniformLocation(gProg, "uMode"), mode);
            glBindVertexArray(gVAOCube);
            glDrawArrays(GL_TRIANGLES, 0, gCubeVerts);
            glBindVertexArray(0);
            };
        auto drawConeAt = [&](glm::vec3 pos, glm::vec3 scl, int mode, float seed, float flicker) {
            glm::mat4 M(1.0f);
            M = glm::translate(M, pos);
            M = glm::scale(M, scl);
            glUniformMatrix4fv(glGetUniformLocation(gProg, "model"), 1, GL_FALSE, glm::value_ptr(M));
            glUniform1i(glGetUniformLocation(gProg, "uMode"), mode);       // 0 = fuego
            glUniform1f(glGetUniformLocation(gProg, "uSeed"), seed);        // semilla
            glUniform1f(glGetUniformLocation(gProg, "uFlicker"), flicker);  // parpadeo externo
            glBindVertexArray(gVAOCone);
            glDrawArrays(GL_TRIANGLES, 0, gConeVerts);
            glBindVertexArray(0);
            };
        // ===============================


        // Mesa
        {
            float ox = topX * 0.5f - 0.09f * 0.5f;
            float oz = topZ * 0.5f - 0.09f * 0.5f;
            drawCubeAt(gTablePos + glm::vec3(0.0f, legH + topY * 0.5f, 0.0f), glm::vec3(topX, topY, topZ), 1);
            drawCubeAt(gTablePos + glm::vec3(+ox, legH * 0.5f, +oz), glm::vec3(0.09f, legH, 0.09f), 1);
            drawCubeAt(gTablePos + glm::vec3(-ox, legH * 0.5f, +oz), glm::vec3(0.09f, legH, 0.09f), 1);
            drawCubeAt(gTablePos + glm::vec3(+ox, legH * 0.5f, -oz), glm::vec3(0.09f, legH, 0.09f), 1);
            drawCubeAt(gTablePos + glm::vec3(-ox, legH * 0.5f, -oz), glm::vec3(0.09f, legH, 0.09f), 1);
            drawCubeAt(gTablePos + glm::vec3(0.0f, railH, +oz), glm::vec3(topX - 0.09f * 1.4f, 0.06f, 0.09f), 1);
            drawCubeAt(gTablePos + glm::vec3(0.0f, railH, -oz), glm::vec3(topX - 0.09f * 1.4f, 0.06f, 0.09f), 1);
            drawCubeAt(gTablePos + glm::vec3(+ox, railH, 0.0f), glm::vec3(0.09f, 0.06f, topZ - 0.09f * 1.4f), 1);
            drawCubeAt(gTablePos + glm::vec3(-ox, railH, 0.0f), glm::vec3(0.09f, 0.06f, topZ - 0.09f * 1.4f), 1);
        }
        {
            if (gFireOn) {
                // 3 flamas (uMode=0) con ligeras variaciones
                drawConeAt(gCampPos + glm::vec3(-0.18f, 0.00f, 0.00f),
                    glm::vec3(0.32f, 0.85f + 0.08f * sinf(currentFrame * 3.4f), 0.32f), 0, 11.0f, fireFlicker);

                drawConeAt(gCampPos + glm::vec3(0.16f, 0.00f, -0.08f),
                    glm::vec3(0.26f, 0.70f + 0.07f * sinf(currentFrame * 4.1f + 1.2f), 0.26f), 0, 17.0f, fireFlicker);

                drawConeAt(gCampPos + glm::vec3(0.05f, 0.00f, 0.15f),
                    glm::vec3(0.22f, 0.60f + 0.06f * sinf(currentFrame * 5.0f + 2.1f), 0.22f), 0, 23.0f, fireFlicker);
            }
        }
        // =======================
 // MÁSCARA DE JADE MOSAICO (colores sólidos, sin sombras)
 // =======================
        {
            glUseProgram(gProg);

            const float yTop = legH + topY;
            const float t = 0.020f;
            const float eps = 0.010f;

            // Proporción casi cuadrada
            const float SX = topX * 0.15f;
            const float SZ = SX * 1.05f;

            // Frente al florero
            glm::vec3 C = gTablePos + glm::vec3(-topX * 0.23f, yTop + t * 0.5f + eps, +topZ * 0.12f);

            auto part = [&](glm::vec3 off, glm::vec3 scl, int mode) {
                drawCubeAt(C + off, scl, mode);
                };

            // === BASE (jade claro)
            part(glm::vec3(0, 0, 0), glm::vec3(SX, t, SZ), 17);

            // === CONTORNO (jade oscuro)
            part(glm::vec3(0, 0, 0), glm::vec3(SX * 0.96f, t, SZ * 0.96f), 18);

            // === DETALLES MOSAICO ===
            // Ceja superior (jade oscuro)
            part(glm::vec3(0, 0, +SZ * 0.18f), glm::vec3(SX * 0.85f, t, SZ * 0.05f), 18);

            // Línea roja (franja decorativa)
            part(glm::vec3(0, 0, +SZ * 0.14f), glm::vec3(SX * 0.90f, t, SZ * 0.02f), 19);

            // Ojos (blanco y negro)
            part(glm::vec3(-SX * 0.25f, 0, +SZ * 0.10f), glm::vec3(SX * 0.22f, t, SZ * 0.04f), 20);
            part(glm::vec3(+SX * 0.25f, 0, +SZ * 0.10f), glm::vec3(SX * 0.22f, t, SZ * 0.04f), 20);
            // Pupilas negras
            part(glm::vec3(-SX * 0.25f, 0, +SZ * 0.10f), glm::vec3(SX * 0.06f, t, SZ * 0.02f), 21);
            part(glm::vec3(+SX * 0.25f, 0, +SZ * 0.10f), glm::vec3(SX * 0.06f, t, SZ * 0.02f), 21);

            // Nariz (jade oscuro y rojo coral)
            part(glm::vec3(0, 0, 0), glm::vec3(SX * 0.09f, t, SZ * 0.12f), 18);
            part(glm::vec3(0, 0, -SZ * 0.03f), glm::vec3(SX * 0.06f, t, SZ * 0.02f), 19);

            // Boca (rojo coral)
            part(glm::vec3(0, 0, -SZ * 0.12f), glm::vec3(SX * 0.55f, t, SZ * 0.04f), 19);
            // Línea negra en medio
            part(glm::vec3(0, 0, -SZ * 0.12f), glm::vec3(SX * 0.25f, t, SZ * 0.01f), 21);

            // Mejillas (jade claro más brillante)
            part(glm::vec3(-SX * 0.32f, 0, -SZ * 0.03f), glm::vec3(SX * 0.12f, t, SZ * 0.08f), 17);
            part(glm::vec3(+SX * 0.32f, 0, -SZ * 0.03f), glm::vec3(SX * 0.12f, t, SZ * 0.08f), 17);

            // Grecas laterales rojas
            part(glm::vec3(-SX * 0.34f, 0, -SZ * 0.06f), glm::vec3(SX * 0.06f, t, SZ * 0.05f), 19);
            part(glm::vec3(+SX * 0.34f, 0, -SZ * 0.06f), glm::vec3(SX * 0.06f, t, SZ * 0.05f), 19);
        }



        // ---------------- PLATOS (2) sobre la tapa ----------------
        {
            const float yTop = legH + topY;
            const float eps = 0.010f;

            // semiextensos de la tapa
            const float hx = topX * 0.5f;
            const float hz = topZ * 0.5f;

            // tamaño y grosor del plato
            const float sPlX = std::min(topX, topZ) * 0.28f;
            const float sPlZ = sPlX;
            const float tPl = 0.026f;

            // helper plato: base cerámica (uMode=2) + aro sutil (uMode=16)
            auto plateAt = [&](glm::vec3 c) {
                drawCubeAt(c + glm::vec3(0, tPl * 0.5f + eps, 0), glm::vec3(sPlX, tPl, sPlZ), 2);
                drawCubeAt(c + glm::vec3(0, tPl * 0.95f + eps, 0), glm::vec3(sPlX * 0.90f, tPl * 0.22f, sPlZ * 0.90f), 16);
                };

            // posiciones: frente-izq y frente-der, con margen
            const float mX = 0.06f, mZ = 0.06f;
            glm::vec3 PL = gTablePos + glm::vec3(-hx + mX + sPlX * 0.5f, yTop, +hz - mZ - sPlZ * 0.5f);
            glm::vec3 PR = gTablePos + glm::vec3(+hx - mX - sPlX * 0.5f, yTop, +hz - mZ - sPlZ * 0.5f);

            plateAt(PL);
            plateAt(PR);
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
      
// Carretilla simple con cubos
// ===============================
        {
            auto wb = [&](glm::vec3 lp, glm::vec3 sc, int mode = 1)
                {
                    drawCubeAt(gWheelPos + lp, sc, mode);
                };

            float bodyW = 1.6f;   // ancho
            float bodyH = 0.40f;  // alto caja
            float bodyL = 2.3f;   // largo

            // ---------------------------
            // Caja
            // ---------------------------
            wb(glm::vec3(0.0f, bodyH * 0.5f + 0.4f, 0.0f),
                glm::vec3(bodyW, bodyH, bodyL), 1);

            // Bordes laterales
            float edgeH = 0.25f;
            wb(glm::vec3(+bodyW * 0.5f, 0.4f + bodyH + edgeH * 0.5f, 0.0f),
                glm::vec3(0.10f, edgeH, bodyL), 1);

            wb(glm::vec3(-bodyW * 0.5f, 0.4f + bodyH + edgeH * 0.5f, 0.0f),
                glm::vec3(0.10f, edgeH, bodyL), 1);

            // ---------------------------
            // Manubrios
            // ---------------------------
            wb(glm::vec3(+bodyW * 0.45f, 0.4f + bodyH + 0.25f, bodyL * 0.60f),
                glm::vec3(0.10f, 0.10f, 1.6f), 1);

            wb(glm::vec3(-bodyW * 0.45f, 0.4f + bodyH + 0.25f, bodyL * 0.60f),
                glm::vec3(0.10f, 0.10f, 1.6f), 1);

            // ---------------------------
            // Piernas de soporte
            // ---------------------------
            wb(glm::vec3(+0.55f, 0.2f, -0.90f),
                glm::vec3(0.15f, 0.40f, 0.15f), 1);

            wb(glm::vec3(-0.55f, 0.2f, -0.90f),
                glm::vec3(0.15f, 0.40f, 0.15f), 1);

            // ---------------------------
            // Rueda (cilindro simulado con cubo ancho)
            // ---------------------------
            wb(glm::vec3(0.0f, 0.35f, -bodyL * 0.5f),
                glm::vec3(0.80f, 0.80f, 0.20f), 3);
        }

        // ========================================
// Hacha pequeña pegando una piedra
// ========================================
        {
            // Pivot: punta del mango en el piso
            glm::vec3 pivot = gAxePos;   // por ejemplo glm::vec3(10.0f, 0.0f, -5.0f);

            // ---------- Animación del golpe ----------
            float t = (float)glfwGetTime();
            float s = (sin(t * 3.0f) + 1.0f) * 0.5f;   // 0..1
            float angDeg = -20.0f - 30.0f * s;         // de -20° a -50° (menos exagerado)
            float angRad = glm::radians(angDeg);

            auto part = [&](glm::vec3 local, glm::vec3 scl, int mode)
                {
                    glm::mat4 M(1.0f);

                    // pivot en el piso
                    M = glm::translate(M, pivot);
                    // rotación hacia adelante alrededor de X
                    M = glm::rotate(M, angRad, glm::vec3(1.0f, 0.0f, 0.0f));
                    // posición local de la pieza (desde el pivot)
                    M = glm::translate(M, local);
                    M = glm::scale(M, scl);

                    glUniformMatrix4fv(glGetUniformLocation(gProg, "model"), 1, GL_FALSE, glm::value_ptr(M));
                    glUniform1i(glGetUniformLocation(gProg, "uMode"), mode); // 1=madera, 2=piedra, 3=cuerda
                    glBindVertexArray(gVAOCube);
                    glDrawArrays(GL_TRIANGLES, 0, gCubeVerts);
                };

            // ---------- Tamaños más pequeños ----------
            float handleLen = 1.6f;          // antes era más largo
            float zStone = 0.55f;         // posición de la piedra en Z
            float zBlade = 0.55f;         // donde cae el filo

            // MANGO
            part(glm::vec3(0.0f, handleLen * 0.5f, 0.0f),
                glm::vec3(0.10f, handleLen, 0.10f), 1);

            // CABEZA (bloque de piedra)
            part(glm::vec3(0.0f, handleLen + 0.15f, 0.30f),
                glm::vec3(0.45f, 0.28f, 0.40f), 2);

            // FILO (un poco más salido, alineado con la piedra)
            part(glm::vec3(0.0f, handleLen + 0.15f, zBlade),
                glm::vec3(0.55f, 0.24f, 0.12f), 2);

            // CUERDA alrededor
            part(glm::vec3(0.0f, handleLen + 0.05f, 0.20f),
                glm::vec3(0.22f, 0.20f, 0.22f), 3);
            part(glm::vec3(0.0f, handleLen + 0.10f, 0.10f),
                glm::vec3(0.26f, 0.18f, 0.26f), 3);

        }


        // Florero
        {
            float vaseH = 0.60f;
            glm::mat4 MV(1.0f);
            MV = glm::translate(MV, gTablePos + glm::vec3(0.0f, 0.75f + 0.12f + vaseH * 0.5f, 0.0f));
            MV = glm::scale(MV, glm::vec3(vaseH));
            glUniformMatrix4fv(glGetUniformLocation(gProg, "model"), 1, GL_FALSE, glm::value_ptr(MV));
            glUniform1i(glGetUniformLocation(gProg, "uMode"), 2);
            glBindVertexArray(gVAOVase);
            glDrawArrays(GL_TRIANGLES, 0, gVaseVerts);
            glBindVertexArray(0);
        }

        // ====== Flor dentro del florero (alineada al cuello) ======
        {
            const float vaseH = 0.60f;      // misma altura que usas al dibujar el florero
            const float tableH = 0.75f;     // altura de la mesa que usas arriba
            const float topT = 0.12f;     // grosor de la tapa de la mesa

            // Altura del borde superior del florero (centro + mitad de su altura)
            const float yMouth = tableH + topT + vaseH;

            // Pequeño hundimiento para que el tallo nazca desde dentro del cuello
            const float sink = 0.08f;

            // Punto base: centro del cuello del florero
            glm::vec3 mouth = gTablePos + glm::vec3(0.0f, yMouth, 0.0f);

            // ---- Tallo (uMode=14, verde)
            float stemH = 0.50f;
            glm::mat4 MT(1.0f);
            MT = glm::translate(MT, mouth + glm::vec3(0.0f, -sink + stemH * 0.5f, 0.0f));
            MT = glm::scale(MT, glm::vec3(0.045f, stemH, 0.045f));
            glUniformMatrix4fv(glGetUniformLocation(gProg, "model"), 1, GL_FALSE, glm::value_ptr(MT));
            glUniform1i(glGetUniformLocation(gProg, "uMode"), 14);
            glBindVertexArray(gVAOCube);
            glDrawArrays(GL_TRIANGLES, 0, gCubeVerts);



            // ---- Hojas (uMode=14, verdes) — a media altura del tallo
            auto leaf = [&](glm::vec3 off, glm::vec3 scl, float yawDeg) {
                glm::mat4 ML(1.0f);
                ML = glm::translate(ML, mouth + off);
                ML = glm::rotate(ML, glm::radians(yawDeg), glm::vec3(0, 1, 0));
                ML = glm::scale(ML, scl);
                glUniformMatrix4fv(glGetUniformLocation(gProg, "model"), 1, GL_FALSE, glm::value_ptr(ML));
                glUniform1i(glGetUniformLocation(gProg, "uMode"), 14);
                glBindVertexArray(gVAOCube);
                glDrawArrays(GL_TRIANGLES, 0, gCubeVerts);
                };
            leaf(glm::vec3(0.0f, -sink + 0.22f, 0.0f), glm::vec3(0.12f, 0.02f, 0.06f), 35.0f);
            leaf(glm::vec3(0.0f, -sink + 0.30f, 0.0f), glm::vec3(0.12f, 0.02f, 0.06f), -35.0f);

            // ---- Pétalos (uMode=15, rosa) alrededor del centro
            const float petalRingY = -sink + stemH + 0.02f; // justo sobre el tallo
            const float petalR = 0.075f;                // radio del anillo de pétalos
            for (int i = 0; i < 6; ++i) {
                float ang = glm::radians(i * 60.0f);
                glm::mat4 MP(1.0f);
                MP = glm::translate(MP, mouth + glm::vec3(0.0f, petalRingY, 0.0f));
                MP = glm::rotate(MP, ang, glm::vec3(0, 1, 0));                 // gira alrededor del tallo
                MP = glm::rotate(MP, glm::radians(-18.0f), glm::vec3(1, 0, 0)); // ligera inclinación
                MP = glm::translate(MP, glm::vec3(0.0f, 0.0f, petalR));       // empuja hacia afuera
                MP = glm::scale(MP, glm::vec3(0.06f, 0.02f, 0.12f));          // “lámina” del pétalo
                glUniformMatrix4fv(glGetUniformLocation(gProg, "model"), 1, GL_FALSE, glm::value_ptr(MP));
                glUniform1i(glGetUniformLocation(gProg, "uMode"), 15);
                glBindVertexArray(gVAOCube);
                glDrawArrays(GL_TRIANGLES, 0, gCubeVerts);
            }



            // ---- Centro (uMode=16, amarillo)
            glm::mat4 MC(1.0f);
            MC = glm::translate(MC, mouth + glm::vec3(0.0f, petalRingY + 0.005f, 0.0f));
            MC = glm::scale(MC, glm::vec3(0.05f));
            glUniformMatrix4fv(glGetUniformLocation(gProg, "model"), 1, GL_FALSE, glm::value_ptr(MC));
            glUniform1i(glGetUniformLocation(gProg, "uMode"), 16);
            glBindVertexArray(gVAOCube);
            glDrawArrays(GL_TRIANGLES, 0, gCubeVerts);
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
        {
            const float zDog = gTablePos.z + 3.0f * 0.5f + 0.55f;
            const float xDog = gTablePos.x - 0.30f;
            drawDog(glm::vec3(xDog, 0.0f, zDog), 10.0f, 1.0f);
        }


        auto dog_drawPartR3 = [&](const glm::vec3& base, float yawDeg,
            const glm::vec3& local,
            float rotXDeg, float rotYDeg, float rotZDeg,
            const glm::vec3& scl, int mode)
            {
                glm::mat4 M(1.0f);
                M = glm::translate(M, base);
                M = glm::rotate(M, glm::radians(yawDeg), glm::vec3(0, 1, 0));
                M = glm::translate(M, local);
                if (rotXDeg != 0.0f) M = glm::rotate(M, glm::radians(rotXDeg), glm::vec3(1, 0, 0));
                if (rotYDeg != 0.0f) M = glm::rotate(M, glm::radians(rotYDeg), glm::vec3(0, 1, 0)); // cola
                if (rotZDeg != 0.0f) M = glm::rotate(M, glm::radians(rotZDeg), glm::vec3(0, 0, 1));
                M = glm::scale(M, scl);

                glUniformMatrix4fv(glGetUniformLocation(gProg, "model"), 1, GL_FALSE, glm::value_ptr(M));
                glUniform1i(glGetUniformLocation(gProg, "uMode"), mode);
                glBindVertexArray(gVAOCube);
                glDrawArrays(GL_TRIANGLES, 0, gCubeVerts);
            };

        // ---- Dibuja perrito (patas + cola animadas, lengua rosa; ojos/orejas fijos)
        auto dog_drawAnimated = [&](const glm::vec3& base, float yawDeg, float s, float t)
            {
                // Animación
                const float stepHz = 2.0f;
                const float phase = t * 2.0f * 3.14159265f * stepHz;
                const float legSwing = 22.0f * std::sin(phase);
                const float legOpp = 22.0f * std::sin(phase + 3.14159265f);
                const float tailYaw = 20.0f * std::sin(phase * 1.5f);

                // Cuerpo (dos bloques)
                dog_drawPartR3(base, yawDeg, glm::vec3(+0.10f, 0.18f, 0.0f), 0, 0, 0, glm::vec3(0.38f * s, 0.22f * s, 0.22f * s), 3);
                dog_drawPartR3(base, yawDeg, glm::vec3(-0.18f, 0.19f, 0.0f), 0, 0, 0, glm::vec3(0.30f * s, 0.20f * s, 0.22f * s), 3);

                // Cabeza/hocico
                dog_drawPartR3(base, yawDeg, glm::vec3(+0.30f, 0.28f, 0.0f), 0, 0, 0, glm::vec3(0.10f * s, 0.16f * s, 0.16f * s), 3);
                dog_drawPartR3(base, yawDeg, glm::vec3(+0.43f, 0.34f, 0.0f), 0, 0, 0, glm::vec3(0.18f * s, 0.16f * s, 0.18f * s), 3);
                dog_drawPartR3(base, yawDeg, glm::vec3(+0.55f, 0.30f, 0.0f), 0, 0, 0, glm::vec3(0.12f * s, 0.10f * s, 0.12f * s), 3);

                // Nariz, ojos y orejas (fijos)
                dog_drawPartR3(base, yawDeg, glm::vec3(+0.62f, 0.30f, 0.0f), 0, 0, 0, glm::vec3(0.06f * s), 4); // nariz
                // Lengua (rosa)
                // Lengua (rosa)


                dog_drawPartR3(base, yawDeg, glm::vec3(+0.52f, 0.36f, +0.08f), 0, 0, 0, glm::vec3(0.03f * s), 4); // ojo der
                dog_drawPartR3(base, yawDeg, glm::vec3(+0.52f, 0.36f, -0.08f), 0, 0, 0, glm::vec3(0.03f * s), 4); // ojo izq
                dog_drawPartR3(base, yawDeg, glm::vec3(+0.46f, 0.48f, +0.07f), 0, 0, 0, glm::vec3(0.06f * s, 0.14f * s, 0.06f * s), 4); // oreja
                dog_drawPartR3(base, yawDeg, glm::vec3(+0.46f, 0.48f, -0.07f), 0, 0, 0, glm::vec3(0.06f * s, 0.14f * s, 0.06f * s), 4); // oreja


                // Cola (visible y con meneo)
                dog_drawPartR3(base, yawDeg, glm::vec3(+0.64f, 0.24f, 0.0f), 0.0f, tailYaw, 0.0f,
                    glm::vec3(0.10f * s, 0.035f * s, 0.06f * s), 3);

                // Patas alternadas
                dog_drawPartR3(base, yawDeg, glm::vec3(+0.20f, 0.09f, +0.09f), legSwing, 0, 0,
                    glm::vec3(0.07f * s, 0.18f * s, 0.07f * s), 3); // FL
                dog_drawPartR3(base, yawDeg, glm::vec3(+0.20f, 0.09f, -0.09f), legOpp, 0, 0,
                    glm::vec3(0.07f * s, 0.18f * s, 0.07f * s), 3); // FR
                dog_drawPartR3(base, yawDeg, glm::vec3(-0.22f, 0.09f, +0.09f), legOpp, 0, 0,
                    glm::vec3(0.07f * s, 0.18f * s, 0.07f * s), 3); // BL
                dog_drawPartR3(base, yawDeg, glm::vec3(-0.22f, 0.09f, -0.09f), legSwing, 0, 0,
                    glm::vec3(0.07f * s, 0.18f * s, 0.07f * s), 3); // BR

                // Cuello
                dog_drawPartR3(base, yawDeg, glm::vec3(-0.32f, 0.32f, 0.0f), 0, 0, 0,
                    glm::vec3(0.05f * s, 0.16f * s, 0.05f * s), 3);
            };

        // ---- Movimiento: lejos de la mesa, trayecto circular amplio
        {
            // Posición base alejada (ajusta a gusto)
            const float x0 = gTablePos.x + 12.0f;  // derecha de la mesa
            const float z0 = gTablePos.z + 8.0f;   // al frente de la mesa
            const float y0 = 0.0f;

            // Dirección/sentido
            const float yaw0Deg = 90.0f;            // perro mirando +X
            const float MODEL_FORWARD_SIGN = +1.0f; // si parece ir en reversa, usa -1.0f

            // Tiempo y giro
            const float t = currentFrame;        // o currentFrame*deltaTime
            const float v = 0.9f;                // velocidad lineal
            const float wDeg = 15.0f;               // °/s -> radio amplio (baja más para círculo mayor)
            const float w = glm::radians(wDeg);

            // Orientación actual
            const float yawDeg = yaw0Deg + wDeg * t * MODEL_FORWARD_SIGN;

            // Trayectoria circular (modelo unicycle exacto)
            const float yaw0 = glm::radians(yaw0Deg);
            const float R = (w != 0.0f) ? (v / w) : 0.0f;

            float x = x0, z = z0;
            if (w != 0.0f) {
                x = x0 + R * (std::sin(yaw0 + w * t) - std::sin(yaw0));
                z = z0 + R * (-std::cos(yaw0 + w * t) + std::cos(yaw0));
            }
            else {
                x = x0 + v * std::cos(yaw0) * t;
                z = z0 + v * std::sin(yaw0) * t;
            }

            glm::vec3 base(x, y0, z);
            base.y += 0.02f * std::sin(t * 6.0f);   // rebote suave

            dog_drawAnimated(base, yawDeg, 1.0f, t);
        }

        {
            // Usamos gProg (el shader de cubos/procedurales)
            glUseProgram(gProg);
            glUniformMatrix4fv(glGetUniformLocation(gProg, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
            glUniformMatrix4fv(glGetUniformLocation(gProg, "view"), 1, GL_FALSE, glm::value_ptr(view));
            glUniform1f(glGetUniformLocation(gProg, "uTime"), currentFrame);



            const glm::vec3 basePos(45.0f, 0.0f, 15.0f);
            const float ballScale = 2.0f;
            const float bounceHeight = 3.8f;       // Altura máxima del rebote (para que alcance el aro)
            const float bounceFrequency = 3.0f;    // Frecuencia del rebote (más alto = más rápido)
            const float horizontalSpeed = 0.8f;    // Velocidad de movimiento horizontal
            const float maxHorizontalOffset = 3.0f; // Máximo desplazamiento horizontal

            // Cálculo del rebote vertical (va de 0 a 1)
            float bounceFactor = std::abs(std::sin(currentFrame * bounceFrequency));

            // Altura vertical de la pelota: basePos.y + offset por rebote + mitad de la escala (para que su base toque el suelo)
            float yOffset = bounceFactor * bounceHeight + (ballScale * 0.5f);


            float xMove = std::sin(currentFrame * horizontalSpeed * 0.7f) * maxHorizontalOffset;
            float zMove = std::cos(currentFrame * horizontalSpeed * 0.6f) * maxHorizontalOffset * 0.5f; // Un poco menos en Z

            // Posición final
            glm::vec3 finalPos = basePos + glm::vec3(xMove, yOffset, zMove);


            glm::mat4 MBall(1.0f);
            MBall = glm::translate(MBall, finalPos);
            MBall = glm::scale(MBall, glm::vec3(ballScale));

            glUniformMatrix4fv(glGetUniformLocation(gProg, "model"), 1, GL_FALSE, glm::value_ptr(MBall));

            glBindVertexArray(gVAOSphere);
            glDrawArrays(GL_TRIANGLES, 0, gSphereVerts);

            glBindVertexArray(0);
            glUseProgram(0);
        }





        glUseProgram(0);
        glfwSwapBuffers(window);
    } // while

    glfwTerminate();
    return 0;
}

// ===========================================================
// Input
// ===========================================================
void DoMovement() {
    // Cámara
    if (keys[GLFW_KEY_W] || keys[GLFW_KEY_UP])    camera.ProcessKeyboard(FORWARD, deltaTime);
    if (keys[GLFW_KEY_S] || keys[GLFW_KEY_DOWN])  camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (keys[GLFW_KEY_A] || keys[GLFW_KEY_LEFT])  camera.ProcessKeyboard(LEFT, deltaTime);
    if (keys[GLFW_KEY_D] || keys[GLFW_KEY_RIGHT]) camera.ProcessKeyboard(RIGHT, deltaTime);
    



    // Carretilla (G = adelante, H = atrás)
    float vel = 5.0f * deltaTime;

    if (keys[GLFW_KEY_G]) {
        gWheelPos.z -= 5.0f * deltaTime;  // hacia adelante en cámara
    }
 
    if (keys[GLFW_KEY_H]) {
        gWheelPos.z += 5.0f * deltaTime;  // hacia atrás
    }
}


    void KeyCallback(GLFWwindow * window, int key, int scancode, int action, int mode) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            glfwSetWindowShouldClose(window, GL_TRUE);

 
        if (key == GLFW_KEY_F && action == GLFW_PRESS) {
            gFireOn = !gFireOn;
        }
        // === FIN DE LA CORRECCIÓN ===

        if (key >= 0 && key < 1024) {
            if (action == GLFW_PRESS)   keys[key] = true;
            if (action == GLFW_RELEASE) keys[key] = false;
        }
    }

    void MouseCallback(GLFWwindow* window, double xPos, double yPos)
    {
        if (firstMouse)
        {
            lastX = (GLfloat)xPos;
            lastY = (GLfloat)yPos;
            firstMouse = false;
        }

        GLfloat xOffset = (GLfloat)xPos - lastX;
        GLfloat yOffset = lastY - (GLfloat)yPos;  

        lastX = (GLfloat)xPos;
        lastY = (GLfloat)yPos;

        camera.ProcessMouseMovement(xOffset, yOffset);
    }
