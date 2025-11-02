#include <iostream>
#include <cmath>
#include <vector>

// GLEW / GLFW
#include <GL/glew.h>
#include <GLFW/glfw3.h>

// Otros
#include "stb_image.h"

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Carga de modelos
#include "SOIL2/SOIL2.h"
#include "Shader.h"
#include "Camera.h"
#include "Model.h"

// Prototipos
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mode);
void MouseCallback(GLFWwindow* window, double xPos, double yPos);
void DoMovement();

// Ventana
const GLuint WIDTH = 800, HEIGHT = 600;
int SCREEN_WIDTH, SCREEN_HEIGHT;

// Cámara
Camera  camera(glm::vec3(0.0f, 0.0f, 3.0f));
GLfloat lastX = WIDTH / 2.0f;
GLfloat lastY = HEIGHT / 2.0f;
bool keys[1024];
bool firstMouse = true;

// Tiempos
GLfloat deltaTime = 0.0f;
GLfloat lastFrame = 0.0f;

// Posiciones base
glm::vec3 gCampPos = glm::vec3(0.0f, 0.0f, -8.0f);
glm::vec3 gTablePos = glm::vec3(-1.8f, 0.0f, -6.0f);  // mesa
glm::vec3 gChairPos = glm::vec3(-1.2f, 0.0f, -6.2f);  // se recalcula relativo a la mesa más abajo

// Shaders embebidos (fogata/madera/tejido/florero)
GLuint gProgFire = 0;

// Fogata
GLuint gVAOLog = 0, gVBOLog = 0; GLsizei gLogVerts = 0;
GLuint gVAOFlame = 0, gVBOFlame = 0; GLsizei gFlameVerts = 0;

// Cubo genérico (mesa/patas/travesaños)
GLuint gVAOCube = 0, gVBOCube = 0; GLsizei gCubeVerts = 0;

// Plano subdividido (asiento)
GLuint gVAOSeat = 0, gVBOSeat = 0; GLsizei gSeatVerts = 0;

// Florero sencillo
GLuint gVAOVase = 0, gVBOVase = 0; GLsizei gVaseVerts = 0;

// --------------------------------------------------
static GLuint Compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[2048]; glGetShaderInfoLog(s, 2048, nullptr, log); std::cout << "Shader error:\n" << log << "\n"; }
    return s;
}
static GLuint Link(GLuint vs, const char* fsrc) {
    GLuint fs = Compile(GL_FRAGMENT_SHADER, fsrc);
    GLuint p = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { char log[2048]; glGetProgramInfoLog(p, 2048, nullptr, log); std::cout << "Program link error:\n" << log << "\n"; }
    glDeleteShader(vs); glDeleteShader(fs);
    return p;
}

static const char* kVS = R"(#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 model, view, projection;
out vec3 vPos;
void main(){
    vPos = aPos;
    gl_Position = projection * view * model * vec4(aPos,1.0);
})";

static const char* kFS = R"(#version 330 core
out vec4 FragColor;
in vec3 vPos;
uniform float uTime;
uniform int   uMode;

// 0=fuego  1=madera  2=florero  5=tejido asiento

float hash(vec2 p){ return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453123); }
float noise(vec2 p){
    vec2 i=floor(p), f=fract(p);
    float a=hash(i), b=hash(i+vec2(1,0)), c=hash(i+vec2(0,1)), d=hash(i+vec2(1,1));
    vec2 u=f*f*(3.0-2.0*f);
    return mix(mix(a,b,u.x), mix(c,d,u.x), u.y);
}

// madera
vec3 wood(vec3 p){
    float rings = sin((p.x*9.0)+noise(p.zy*6.0)*0.8)*0.5+0.5;
    return mix(vec3(0.55,0.36,0.18), vec3(0.74,0.54,0.30), rings);
}

void main(){
    if(uMode==1){ FragColor = vec4(wood(vPos),1.0); return; }

    if(uMode==0){
        float h = clamp(vPos.y,0.0,1.0);
        float r = length(vPos.xz);
        float edge = smoothstep(0.35,0.15,r);
        float t = uTime*2.0;
        float flick = noise(vec2(r*6.0, h*8.0+t*3.5))*0.6 + noise(vec2(r*10.0+t*1.7, h*5.0))*0.4;
        vec3 colLow=vec3(1.0,0.25,0.02), colMid=vec3(1.0,0.55,0.05), colHigh=vec3(1.0,0.85,0.25), colTop=vec3(1.0,0.95,0.75);
        float k1 = clamp(h*1.2+flick*0.2,0.0,1.0);
        vec3 col = mix(colLow,colMid,k1); col=mix(col,colHigh,k1*k1); col=mix(col,colTop,pow(k1,4.0));
        float a = edge*(0.85+0.15*sin(t*7.0+r*10.0)); a*= (0.35+0.65*h);
        FragColor = vec4(col, clamp(a,0.0,1.0)); return;
    }

    if(uMode==5){
        mat2 R = mat2(0.7071,-0.7071,0.7071,0.7071);
        vec2 uv = R * vPos.xz * 6.0;
        vec2 g = floor(uv);
        float check = mod(g.x+g.y,2.0);
        float ring = floor(max(abs(uv.x),abs(uv.y)));
        float k = mod(ring + check, 2.0);
        vec3 jute=vec3(0.90,0.86,0.72), navy=vec3(0.05,0.10,0.20);
        vec3 base = mix(navy,jute,k);
        base += 0.08*noise(uv*2.5);
        FragColor = vec4(base,1.0); return;
    }

    if(uMode==2){
        vec3 terracotta = vec3(0.63,0.28,0.20);
        vec3 crema      = vec3(0.90,0.82,0.72);
        float b = smoothstep(-0.15,0.15, sin(vPos.y*18.0));
        vec3 col = mix(terracotta, crema, b*0.25);
        if(abs(vPos.y-0.15)<0.06) col = mix(col, vec3(0.14,0.02,0.02), 0.85);
        if(abs(vPos.y-0.00)<0.06) col = mix(col, vec3(0.14,0.02,0.02), 0.85);
        FragColor = vec4(col,1.0); return;
    }

    FragColor = vec4(1,0,1,1);
})";

static void CreateProgram() { gProgFire = Link(Compile(GL_VERTEX_SHADER, kVS), kFS); }

// -------- geometrías --------
static void BuildCampfireGeometry() {
    float L = 1.6f, H = 0.12f, W = 0.12f;
    float x0 = -L * 0.5f, x1 = L * 0.5f, y0 = -H * 0.5f, y1 = H * 0.5f, z0 = -W * 0.5f, z1 = W * 0.5f;
    glm::vec3 p[] = { {x0,y0,z0},{x1,y0,z0},{x1,y1,z0},{x0,y1,z0},{x0,y0,z1},{x1,y0,z1},{x1,y1,z1},{x0,y1,z1} };
    auto quad = [&](int a, int b, int c, int d, std::vector<glm::vec3>& o) {
        o.push_back(p[a]);o.push_back(p[b]);o.push_back(p[c]);
        o.push_back(p[a]);o.push_back(p[c]);o.push_back(p[d]);
        };
    std::vector<glm::vec3> logVerts;
    quad(0, 1, 2, 3, logVerts); quad(4, 5, 6, 7, logVerts);
    quad(0, 4, 7, 3, logVerts); quad(1, 5, 6, 2, logVerts);
    quad(3, 2, 6, 7, logVerts); quad(0, 1, 5, 4, logVerts);
    gLogVerts = (GLsizei)logVerts.size();
    glGenVertexArrays(1, &gVAOLog); glGenBuffers(1, &gVBOLog);
    glBindVertexArray(gVAOLog);
    glBindBuffer(GL_ARRAY_BUFFER, gVBOLog);
    glBufferData(GL_ARRAY_BUFFER, logVerts.size() * sizeof(glm::vec3), logVerts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glBindVertexArray(0);

    const int seg = 48; const float r = 0.35f;
    std::vector<glm::vec3> cone; cone.push_back(glm::vec3(0, 0, 0));
    for (int i = 0;i <= seg;++i) { float a = i * (2.f * 3.14159265f / seg); cone.push_back(glm::vec3(r * cosf(a), 0, r * sinf(a))); }
    std::vector<glm::vec3> flame; glm::vec3 tip(0, 1, 0);
    for (int i = 1;i <= seg;++i) { flame.push_back(cone[0]); flame.push_back(cone[i]); flame.push_back(cone[i + 1]); }
    for (int i = 1;i <= seg;++i) { flame.push_back(cone[i]); flame.push_back(cone[i + 1]); flame.push_back(tip); }
    gFlameVerts = (GLsizei)flame.size();
    glGenVertexArrays(1, &gVAOFlame); glGenBuffers(1, &gVBOFlame);
    glBindVertexArray(gVAOFlame);
    glBindBuffer(GL_ARRAY_BUFFER, gVBOFlame);
    glBufferData(GL_ARRAY_BUFFER, flame.size() * sizeof(glm::vec3), flame.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glBindVertexArray(0);
}

static void BuildCube() {
    float v[] = {
        -0.5f,-0.5f,0.5f, 0.5f,-0.5f,0.5f, 0.5f,0.5f,0.5f,
        -0.5f,-0.5f,0.5f, 0.5f,0.5f,0.5f, -0.5f,0.5f,0.5f,
        -0.5f,-0.5f,-0.5f, -0.5f,0.5f,-0.5f, 0.5f,0.5f,-0.5f,
        -0.5f,-0.5f,-0.5f, 0.5f,0.5f,-0.5f, 0.5f,-0.5f,-0.5f,
        0.5f,-0.5f,-0.5f, 0.5f,0.5f,-0.5f, 0.5f,0.5f,0.5f,
        0.5f,-0.5f,-0.5f, 0.5f,0.5f,0.5f, 0.5f,-0.5f,0.5f,
        -0.5f,-0.5f,-0.5f, -0.5f,-0.5f,0.5f, -0.5f,0.5f,0.5f,
        -0.5f,-0.5f,-0.5f, -0.5f,0.5f,0.5f, -0.5f,0.5f,-0.5f,
        -0.5f,0.5f,-0.5f, -0.5f,0.5f,0.5f, 0.5f,0.5f,0.5f,
        -0.5f,0.5f,-0.5f, 0.5f,0.5f,0.5f, 0.5f,0.5f,-0.5f,
        -0.5f,-0.5f,-0.5f, 0.5f,-0.5f,-0.5f, 0.5f,-0.5f,0.5f,
        -0.5f,-0.5f,-0.5f,-0.5f,-0.5f,0.5f, 0.5f,-0.5f,0.5f
    };
    gCubeVerts = 36;
    glGenVertexArrays(1, &gVAOCube); glGenBuffers(1, &gVBOCube);
    glBindVertexArray(gVAOCube);
    glBindBuffer(GL_ARRAY_BUFFER, gVBOCube);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);
}

static void BuildSeatPlane(int nx = 40, int nz = 40) {
    std::vector<glm::vec3> verts;
    for (int i = 0;i < nx;i++) {
        float x0 = -0.5f + (float)i / nx, x1 = -0.5f + (float)(i + 1) / nx;
        for (int j = 0;j < nz;j++) {
            float z0 = -0.5f + (float)j / nz, z1 = -0.5f + (float)(j + 1) / nz;
            glm::vec3 a(x0, 0, z0), b(x1, 0, z0), c(x1, 0, z1), d(x0, 0, z1);
            verts.push_back(a); verts.push_back(b); verts.push_back(c);
            verts.push_back(a); verts.push_back(c); verts.push_back(d);
        }
    }
    gSeatVerts = (GLsizei)verts.size();
    glGenVertexArrays(1, &gVAOSeat); glGenBuffers(1, &gVBOSeat);
    glBindVertexArray(gVAOSeat);
    glBindBuffer(GL_ARRAY_BUFFER, gVBOSeat);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(glm::vec3), verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glBindVertexArray(0);
}

static void BuildVase() {
    // revolución simple
    std::vector<glm::vec3> v;
    std::vector<glm::vec2> prof = {
        {0.00f,-0.30f},{0.25f,-0.30f},{0.35f,-0.20f},{0.42f,-0.05f},
        {0.43f, 0.10f},{0.36f, 0.20f},{0.28f, 0.28f},{0.22f, 0.32f},
        {0.18f, 0.34f},{0.16f, 0.35f}
    };
    int seg = 64;
    for (size_t k = 0;k + 1 < prof.size();++k) {
        float r0 = prof[k].x, y0 = prof[k].y;
        float r1 = prof[k + 1].x, y1 = prof[k + 1].y;
        for (int i = 0;i < seg;i++) {
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
    glGenVertexArrays(1, &gVAOVase); glGenBuffers(1, &gVBOVase);
    glBindVertexArray(gVAOVase);
    glBindBuffer(GL_ARRAY_BUFFER, gVBOVase);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(glm::vec3), v.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glBindVertexArray(0);
}

// --------------------------------------------------
int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Modelo Texturizado (sin iluminacion)", nullptr, nullptr);
    if (!window) { std::cout << "Failed to create GLFW window\n"; glfwTerminate(); return EXIT_FAILURE; }
    glfwMakeContextCurrent(window);
    glfwGetFramebufferSize(window, &SCREEN_WIDTH, &SCREEN_HEIGHT);
    glfwSetKeyCallback(window, KeyCallback);
    glfwSetCursorPosCallback(window, MouseCallback);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) { std::cout << "Failed to initialize GLEW\n"; return EXIT_FAILURE; }

    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Shader shader("Shader/modelLoading.vs", "Shader/modelLoading.frag");

    Model CanastaChiles((char*)"Models/CanastaChiles.obj");
    Model Chiles((char*)"Models/Chiles.obj");
    Model PetatesTianguis((char*)"Models/PetatesTianguis.obj");
    Model Aguacates((char*)"Models/Aguacates.obj");
    Model Jarrones((char*)"Models/Jarrones.obj");
    Model Tendedero((char*)"Models/Tendedero.obj");
    Model PielJaguar((char*)"Models/PielJaguar.obj");
    Model Piel2((char*)"Models/Piel2.obj");
    Model Vasijas((char*)"Models/Vasijas.obj");
    Model Piramide((char*)"Models/Piramide.obj");

    CreateProgram();
    BuildCampfireGeometry();
    BuildCube();
    BuildSeatPlane();
    BuildVase();

    glm::mat4 projection = glm::perspective(camera.GetZoom(), (GLfloat)SCREEN_WIDTH / (GLfloat)SCREEN_HEIGHT, 0.1f, 100.0f);

    // Mesa “acostada”: lado largo en Z
    const float topX = 1.6f;   // ancho (X)
    const float topZ = 3.0f;   // profundidad (Z) — lado largo
    const float topY = 0.12f;  // grosor
    const float legH = 0.90f;  // altura pata
    const float legT = 0.09f;  // grosor pata
    const float railT = 0.06f; // grosor travesaño
    const float railH = 0.32f; // altura travesaño

    // Recolocar la silla a la derecha de la mesa (costado +X)
    const float seat = 0.60f, tLeg = 0.06f;
    gChairPos = gTablePos + glm::vec3(topX * 0.5f + 0.40f + seat * 0.5f, 0.0f, 0.0f);

    while (!glfwWindowShouldClose(window)) {
        GLfloat currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame; lastFrame = currentFrame;
        glfwPollEvents(); DoMovement();

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.Use();
        glm::mat4 view = camera.GetViewMatrix();
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "view"), 1, GL_FALSE, glm::value_ptr(view));

        glm::mat4 m(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(m));
        CanastaChiles.Draw(shader);
        glm::mat4 m2(1.0f); glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(m2)); Chiles.Draw(shader);
        glm::mat4 m3(1.0f); glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(m3)); PetatesTianguis.Draw(shader);
        glm::mat4 m4(1.0f); glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(m4)); Aguacates.Draw(shader);
        glm::mat4 m5(1.0f); glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(m5)); Jarrones.Draw(shader);
        glm::mat4 m6(1.0f); glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(m6)); Tendedero.Draw(shader);
        glm::mat4 m7(1.0f); glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(m7)); PielJaguar.Draw(shader);
        glm::mat4 m8(1.0f); glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(m8)); Piel2.Draw(shader);

        glm::mat4 mp(1.0f);
        mp = glm::translate(mp, glm::vec3(0.0f, 0.0f, -100.0f));
        mp = glm::scale(mp, glm::vec3(0.5f));
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(mp));
        Piramide.Draw(shader);

        // ---------- Mesa + Silla + Florero + Fogata ----------
        glUseProgram(gProgFire);
        GLint uProj = glGetUniformLocation(gProgFire, "projection");
        GLint uView = glGetUniformLocation(gProgFire, "view");
        glUniformMatrix4fv(uProj, 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(uView, 1, GL_FALSE, glm::value_ptr(view));
        glUniform1f(glGetUniformLocation(gProgFire, "uTime"), currentFrame);

        auto drawCube = [&](glm::vec3 pos, glm::vec3 scl) {
            glm::mat4 mm(1.0f);
            mm = glm::translate(mm, pos);
            mm = glm::scale(mm, scl);
            glUniformMatrix4fv(glGetUniformLocation(gProgFire, "model"), 1, GL_FALSE, glm::value_ptr(mm));
            glUniform1i(glGetUniformLocation(gProgFire, "uMode"), 1);
            glBindVertexArray(gVAOCube);
            glDrawArrays(GL_TRIANGLES, 0, gCubeVerts);
            };

        // Tablero
        drawCube(gTablePos + glm::vec3(0.0f, legH + topY * 0.5f, 0.0f), glm::vec3(topX, topY, topZ));

        // Patas
        float ox = topX * 0.5f - legT * 0.5f;
        float oz = topZ * 0.5f - legT * 0.5f;
        drawCube(gTablePos + glm::vec3(+ox, legH * 0.5f, +oz), glm::vec3(legT, legH, legT));
        drawCube(gTablePos + glm::vec3(-ox, legH * 0.5f, +oz), glm::vec3(legT, legH, legT));
        drawCube(gTablePos + glm::vec3(+ox, legH * 0.5f, -oz), glm::vec3(legT, legH, legT));
        drawCube(gTablePos + glm::vec3(-ox, legH * 0.5f, -oz), glm::vec3(legT, legH, legT));

        // Travesaños
        drawCube(gTablePos + glm::vec3(0.0f, railH, +oz), glm::vec3(topX - legT * 1.4f, railT, legT));
        drawCube(gTablePos + glm::vec3(0.0f, railH, -oz), glm::vec3(topX - legT * 1.4f, railT, legT));
        drawCube(gTablePos + glm::vec3(+ox, railH, 0.0f), glm::vec3(legT, railT, topZ - legT * 1.4f));
        drawCube(gTablePos + glm::vec3(-ox, railH, 0.0f), glm::vec3(legT, railT, topZ - legT * 1.4f));

        // Silla al costado derecho (patas + travesaños + asiento)
        auto drawChairCube = [&](glm::vec3 localPos, glm::vec3 scl) {
            drawCube(gChairPos + localPos, scl);
            };
        const float hLeg = 0.75f, railHc = 0.35f, railTc = 0.05f;
        // patas
        drawChairCube(glm::vec3(+seat * 0.5f - tLeg * 0.5f, hLeg * 0.5f, +seat * 0.5f - tLeg * 0.5f), glm::vec3(tLeg, hLeg, tLeg));
        drawChairCube(glm::vec3(-seat * 0.5f + tLeg * 0.5f, hLeg * 0.5f, +seat * 0.5f - tLeg * 0.5f), glm::vec3(tLeg, hLeg, tLeg));
        drawChairCube(glm::vec3(+seat * 0.5f - tLeg * 0.5f, hLeg * 0.5f, -seat * 0.5f + tLeg * 0.5f), glm::vec3(tLeg, hLeg, tLeg));
        drawChairCube(glm::vec3(-seat * 0.5f + tLeg * 0.5f, hLeg * 0.5f, -seat * 0.5f + tLeg * 0.5f), glm::vec3(tLeg, hLeg, tLeg));
        // travesaños
        drawChairCube(glm::vec3(0.0f, railHc, +seat * 0.5f - tLeg * 0.5f), glm::vec3(seat - tLeg * 1.2f, railTc, tLeg));
        drawChairCube(glm::vec3(0.0f, railHc, -seat * 0.5f + tLeg * 0.5f), glm::vec3(seat - tLeg * 1.2f, railTc, tLeg));
        drawChairCube(glm::vec3(+seat * 0.5f - tLeg * 0.5f, railHc, 0.0f), glm::vec3(tLeg, railTc, seat - tLeg * 1.2f));
        drawChairCube(glm::vec3(-seat * 0.5f + tLeg * 0.5f, railHc, 0.0f), glm::vec3(tLeg, railTc, seat - tLeg * 1.2f));
        // asiento tejido
        {
            glm::mat4 ms(1.0f);
            ms = glm::translate(ms, gChairPos + glm::vec3(0.0f, hLeg, 0.0f));
            ms = glm::scale(ms, glm::vec3(seat, 1.0f, seat));
            glUniformMatrix4fv(glGetUniformLocation(gProgFire, "model"), 1, GL_FALSE, glm::value_ptr(ms));
            glUniform1i(glGetUniformLocation(gProgFire, "uMode"), 5);
            glBindVertexArray(gVAOSeat);
            glDrawArrays(GL_TRIANGLES, 0, gSeatVerts);
            glBindVertexArray(0);
        }

        // Florero encima de la mesa, “más a la mesa” (ligero +X)
        {
            float vaseH = 0.60f;
            glm::mat4 mv(1.0f);
            mv = glm::translate(mv, gTablePos + glm::vec3(+topX * 0.15f, legH + topY + vaseH * 0.5f, 0.0f));
            mv = glm::scale(mv, glm::vec3(vaseH));
            glUniformMatrix4fv(glGetUniformLocation(gProgFire, "model"), 1, GL_FALSE, glm::value_ptr(mv));
            glUniform1i(glGetUniformLocation(gProgFire, "uMode"), 2);
            glBindVertexArray(gVAOVase);
            glDrawArrays(GL_TRIANGLES, 0, gVaseVerts);
            glBindVertexArray(0);
        }

        // Fogata
        {
            glm::mat4 mf(1.0f);
            mf = glm::translate(mf, gCampPos + glm::vec3(0.0f, 0.10f, 0.0f));
            mf = glm::scale(mf, glm::vec3(1.3f, 1.0f, 1.3f));
            mf = glm::rotate(mf, glm::radians(20.0f), glm::vec3(0, 1, 0));
            glUniformMatrix4fv(glGetUniformLocation(gProgFire, "model"), 1, GL_FALSE, glm::value_ptr(mf));
            glUniform1i(glGetUniformLocation(gProgFire, "uMode"), 1);
            glBindVertexArray(gVAOLog);
            glDrawArrays(GL_TRIANGLES, 0, gLogVerts);
        }
        {
            glm::mat4 mf(1.0f);
            mf = glm::translate(mf, gCampPos + glm::vec3(0.0f, 0.10f, 0.0f));
            mf = glm::scale(mf, glm::vec3(1.3f, 1.0f, 1.3f));
            mf = glm::rotate(mf, glm::radians(110.0f), glm::vec3(0, 1, 0));
            glUniformMatrix4fv(glGetUniformLocation(gProgFire, "model"), 1, GL_FALSE, glm::value_ptr(mf));
            glUniform1i(glGetUniformLocation(gProgFire, "uMode"), 1);
            glBindVertexArray(gVAOLog);
            glDrawArrays(GL_TRIANGLES, 0, gLogVerts);
        }
        {
            glm::mat4 mf(1.0f);
            float flick = 0.05f * sinf(currentFrame * 7.3f) + 0.05f * sinf(currentFrame * 3.9f);
            mf = glm::translate(mf, gCampPos + glm::vec3(0.0f, 0.15f, 0.0f));
            mf = glm::scale(mf, glm::vec3(1.6f + flick, 2.2f + flick, 1.6f + flick));
            glUniformMatrix4fv(glGetUniformLocation(gProgFire, "model"), 1, GL_FALSE, glm::value_ptr(mf));
            glUniform1i(glGetUniformLocation(gProgFire, "uMode"), 0);
            glDepthMask(GL_FALSE);
            glBindVertexArray(gVAOFlame);
            glDrawArrays(GL_TRIANGLES, 0, gFlameVerts);
            glBindVertexArray(0);
            glDepthMask(GL_TRUE);
        }
        glUseProgram(0);

        glfwSwapBuffers(window);
    }
    glfwTerminate();
    return 0;
}

// Input
void DoMovement() {
    if (keys[GLFW_KEY_W] || keys[GLFW_KEY_UP])    camera.ProcessKeyboard(FORWARD, deltaTime);
    if (keys[GLFW_KEY_S] || keys[GLFW_KEY_DOWN])  camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (keys[GLFW_KEY_A] || keys[GLFW_KEY_LEFT])  camera.ProcessKeyboard(LEFT, deltaTime);
    if (keys[GLFW_KEY_D] || keys[GLFW_KEY_RIGHT]) camera.ProcessKeyboard(RIGHT, deltaTime);
}
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mode) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) glfwSetWindowShouldClose(window, GL_TRUE);
    if (key >= 0 && key < 1024) { if (action == GLFW_PRESS) keys[key] = true; else if (action == GLFW_RELEASE) keys[key] = false; }
}
void MouseCallback(GLFWwindow* window, double xPos, double yPos) {
    if (firstMouse) { lastX = (GLfloat)xPos; lastY = (GLfloat)yPos; firstMouse = false; }
    GLfloat xOffset = (GLfloat)xPos - lastX; GLfloat yOffset = lastY - (GLfloat)yPos;
    lastX = (GLfloat)xPos; lastY = (GLfloat)yPos;
    camera.ProcessMouseMovement(xOffset, yOffset);
}
