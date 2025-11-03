// ===========================================================
//  Escena OpenGL (C++ / GLFW / GLEW / GLM / Assimp)
//  - Mesa, silla con asiento tejido procedural
//  - Florero, chihuahua voxel, máscara, fogata y antorchas
//  - Suelo de pasto procedural y pirámide con efecto desértico
// ===========================================================

#include <iostream>
#include <vector>
#include <cmath>

// GLEW / GLFW
#include <GL/glew.h>
#include <GLFW/glfw3.h>

// stb_image
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
bool keys[1024];
bool firstMouse = true;

// ================== Tiempo ======================
GLfloat deltaTime = 0.0f;
GLfloat lastFrame = 0.0f;

// ================== Posiciones base =============
glm::vec3 gTablePos = glm::vec3(-1.8f, 0.0f, -6.0f);
glm::vec3 gChairPos = glm::vec3(-1.2f, 0.0f, -6.2f);
glm::vec3 gCampPos = glm::vec3(-3.2f, 0.0f, -9.8f);

// ================== Shader embebido único =======
GLuint gProg = 0;

// ================== VAOs / VBOs =================
GLuint  gVAOCube = 0, gVBOCube = 0;   GLsizei gCubeVerts = 0;
GLuint  gVAOSeat = 0, gVBOSeat = 0;   GLsizei gSeatVerts = 0;
GLuint  gVAOVase = 0, gVBOVase = 0;   GLsizei gVaseVerts = 0;
GLuint  gVAOFlame = 0, gVBOFlame = 0;  GLsizei gFlameVerts = 0;
GLuint  gVAOLog = 0, gVBOLog = 0;    GLsizei gLogVerts = 0;

// ===========================================================
// Shaders embebidos (procedurales)
// ===========================================================
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
uniform int   uMode; // 0=fuego 1=madera 2=florero 3=pelaje_claro 4=oscuro 5=tejido 6=rosa
                     // 7=mask_red 8=mask_white 9=mask_black 10=grass

float hash(vec2 p){ return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453123); }
float noise(vec2 p){
    vec2 i=floor(p), f=fract(p);
    float a=hash(i), b=hash(i+vec2(1,0)), c=hash(i+vec2(0,1)), d=hash(i+vec2(1,1));
    vec2 u=f*f*(3.0-2.0*f);
    return mix(mix(a,b,u.x), mix(c,d,u.x), u.y);
}
vec3 wood(vec3 p){
    float rings = sin((p.x*9.0)+noise(p.zy*6.0)*0.8)*0.5+0.5;
    return mix(vec3(0.55,0.36,0.18), vec3(0.74,0.54,0.30), rings);
}

void main(){
    // madera
    if(uMode==1){ FragColor=vec4(wood(vPos),1.0); return; }

    // fuego
    if(uMode==0){
        float h=clamp(vPos.y,0.0,1.0);
        float r=length(vPos.xz);
        float edge=smoothstep(0.35,0.15,r);
        float t=uTime*2.0;
        float flick = noise(vec2(r*6.0,h*8.0+t*3.5))*0.6
                    + noise(vec2(r*10.0+t*1.7,h*5.0))*0.4;
        vec3 c1=vec3(1.0,0.25,0.02), c2=vec3(1.0,0.55,0.05),
             c3=vec3(1.0,0.85,0.25), c4=vec3(1.0,0.95,0.75);
        float k=clamp(h*1.2+flick*0.2,0.0,1.0);
        vec3 col=mix(c1,c2,k);
        col=mix(col,c3,k*k);
        col=mix(col,c4,pow(k,4.0));
        float a=edge*(0.85+0.15*sin(t*7.0+r*10.0));
        a*=(0.35+0.65*h);
        FragColor=vec4(col,clamp(a,0.0,1.0));
        return;
    }

    // asiento tejido
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
        FragColor=vec4(base,1.0);
        return;
    }

    // florero
    if(uMode==2){
        vec3 terracotta=vec3(0.63,0.28,0.20);
        vec3 crema=vec3(0.90,0.82,0.72);
        float b=smoothstep(-0.15,0.15,sin(vPos.y*18.0));
        vec3 col=mix(terracotta,crema,b*0.25);
        if(abs(vPos.y-0.15)<0.06) col=mix(col,vec3(0.14,0.02,0.02),0.85);
        if(abs(vPos.y-0.00)<0.06) col=mix(col,vec3(0.14,0.02,0.02),0.85);
        FragColor=vec4(col,1.0);
        return;
    }

    // pelajes / lengua
    if(uMode==3){ FragColor=vec4(0.84,0.72,0.54,1.0); return; } // claro
    if(uMode==4){ FragColor=vec4(0.10,0.07,0.06,1.0); return; } // oscuro
    if(uMode==6){ FragColor=vec4(0.88,0.42,0.55,1.0); return; } // rosa

    // máscara colores
    if(uMode==7){ FragColor=vec4(0.80,0.12,0.12,1.0); return; } // rojo
    if(uMode==8){ FragColor=vec4(0.95,0.95,0.95,1.0); return; } // blanco
    if(uMode==9){ FragColor=vec4(0.05,0.05,0.05,1.0); return; } // negro

    // pasto
    if(uMode==10){
        vec2 uv = vPos.xz * 0.6;
        float n1 = noise(uv*2.0);
        float n2 = noise(uv*6.0);
        float m = 0.6 + 0.25*n1 + 0.15*n2;
        vec3 g1 = vec3(0.10, 0.35, 0.08);
        vec3 g2 = vec3(0.25, 0.55, 0.18);
        vec3 col = mix(g1,g2,m);
        FragColor = vec4(col, 1.0);
        return;
    }

    FragColor=vec4(1,0,1,1);
})";

// ===========================================================
// Utilidades de compilación / enlace
// ===========================================================
static GLuint Compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, 2048, nullptr, log);
        std::cout << "Shader error:\n" << log << "\n";
    }
    return s;
}

static GLuint Link(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(p, 2048, nullptr, log);
        std::cout << "Link error:\n" << log << "\n";
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return p;
}

static void CreateProgram() {
    gProg = Link(Compile(GL_VERTEX_SHADER, kVS), Compile(GL_FRAGMENT_SHADER, kFS));
}

// ===========================================================
// Geometrías
// ===========================================================
static void BuildCube() {
    float v[] = {
        // +Z
        -0.5f,-0.5f, 0.5f,  0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f,
        -0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f,
        // -Z
        -0.5f,-0.5f,-0.5f, -0.5f, 0.5f,-0.5f,  0.5f, 0.5f,-0.5f,
        -0.5f,-0.5f,-0.5f,  0.5f, 0.5f,-0.5f,  0.5f,-0.5f,-0.5f,
        // +X
         0.5f,-0.5f,-0.5f,  0.5f, 0.5f,-0.5f,  0.5f, 0.5f, 0.5f,
         0.5f,-0.5f,-0.5f,  0.5f, 0.5f, 0.5f,  0.5f,-0.5f, 0.5f,
         // -X
         -0.5f,-0.5f,-0.5f, -0.5f,-0.5f, 0.5f, -0.5f, 0.5f, 0.5f,
         -0.5f,-0.5f,-0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f,-0.5f,
         // +Y
         -0.5f, 0.5f,-0.5f, -0.5f, 0.5f, 0.5f,  0.5f, 0.5f, 0.5f,
         -0.5f, 0.5f,-0.5f,  0.5f, 0.5f, 0.5f,  0.5f, 0.5f,-0.5f,
         // -Y
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
    std::vector<glm::vec3> verts;
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

    int seg = 64;
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
    // tronco (caja)
    float L = 1.4f, H = 0.12f, W = 0.12f;
    float x0 = -L * 0.5f, x1 = L * 0.5f, y0 = -H * 0.5f, y1 = H * 0.5f, z0 = -W * 0.5f, z1 = W * 0.5f;
    glm::vec3 p[] = { {x0,y0,z0},{x1,y0,z0},{x1,y1,z0},{x0,y1,z0},{x0,y0,z1},{x1,y0,z1},{x1,y1,z1},{x0,y1,z1} };
    auto quad = [&](int a, int b, int c, int d, std::vector<glm::vec3>& o) {
        o.push_back(p[a]); o.push_back(p[b]); o.push_back(p[c]);
        o.push_back(p[a]); o.push_back(p[c]); o.push_back(p[d]);
        };

    std::vector<glm::vec3> log;
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

    // llama (cono en triángulos)
    const int seg = 48; const float r = 0.38f;
    std::vector<glm::vec3> flame; glm::vec3 tip(0, 1.2f, 0);
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

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Modelo Texturizado (sin iluminacion)", nullptr, nullptr);
    if (!window) {
        std::cout << "Failed to create GLFW window\n";
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);
    glfwGetFramebufferSize(window, &SCREEN_WIDTH, &SCREEN_HEIGHT);
    glfwSetKeyCallback(window, KeyCallback);
    glfwSetCursorPosCallback(window, MouseCallback);

    // GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cout << "Failed to initialize GLEW\n";
        return EXIT_FAILURE;
    }

    // OpenGL state
    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Shaders para modelos
    Shader shader("Shader/modelLoading.vs", "Shader/modelLoading.frag");

    // Modelos del tianguis y pirámide
    Model CanastaChiles((char*)"Models/CanastaChiles.obj");
    Model Chiles((char*)"Models/Chiles.obj");
    Model PetatesTianguis((char*)"Models/PetatesTianguis.obj");
    Model Aguacates((char*)"Models/Aguacates.obj");
    Model Jarrones((char*)"Models/Jarrones.obj");
    Model Tendedero((char*)"Models/Tendedero.obj");
    Model PielJaguar((char*)"Models/PielJaguar.obj");
    Model Piel2((char*)"Models/Piel2.obj");
    Model Piramide((char*)"Models/Piramide.obj");

    Model TechosChozas((char*)"Models/TechosChozas.obj"); 
    Model ParedesChozas((char*)"Models/ParedesChozas.obj");

    Model Calendario((char*)"Models/calendario_azteca1.obj");


    // Program/VAOs procedurales
    CreateProgram();
    BuildCube();
    BuildSeatPlane();
    BuildVase();
    BuildCampfire();

    // Matrices
    glm::mat4 projection = glm::perspective(camera.GetZoom(),
        (GLfloat)SCREEN_WIDTH / (GLfloat)SCREEN_HEIGHT, 0.1f, 100.0f);

    // Dimensiones mesa/silla
    const float topX = 1.6f, topZ = 3.0f, topY = 0.12f;
    const float legH = 0.90f, legT = 0.09f, railT = 0.06f, railH = 0.32f;
    const float seat = 0.60f, tLeg = 0.06f;

    // Reposicionar silla respecto a la mesa
    gChairPos = gTablePos + glm::vec3(topX * 0.5f + 0.45f + seat * 0.5f, 0.0f, -topZ * 0.25f);

    // Loop principal
    while (!glfwWindowShouldClose(window)) {
        // tiempo
        GLfloat currentFrame = (GLfloat)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input
        glfwPollEvents();
        DoMovement();

        // clear
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ====== Modelos (shader con texturas) ======
        shader.Use();
        glm::mat4 view = camera.GetViewMatrix();
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "view"), 1, GL_FALSE, glm::value_ptr(view));

        // Canasta
        glm::mat4 model(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(glGetUniformLocation(shader.Program, "view"), 0);
        CanastaChiles.Draw(shader);

        // Chiles
        glm::mat4 model2(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model2));
        glUniform1i(glGetUniformLocation(shader.Program, "view"), 0);
        Chiles.Draw(shader);

        // Petates
        glm::mat4 model3(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model3));
        glUniform1i(glGetUniformLocation(shader.Program, "view"), 0);
        PetatesTianguis.Draw(shader);

        // Aguacates
        glm::mat4 model4(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model4));
        glUniform1i(glGetUniformLocation(shader.Program, "view"), 0);
        Aguacates.Draw(shader);

        // Jarrones
        glm::mat4 model5(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model5));
        glUniform1i(glGetUniformLocation(shader.Program, "view"), 0);
        Jarrones.Draw(shader);

        // Tendedero
        glm::mat4 model6(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model6));
        glUniform1i(glGetUniformLocation(shader.Program, "view"), 0);
        Tendedero.Draw(shader);

        // Pieles
        glm::mat4 model7(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model7));
        glUniform1i(glGetUniformLocation(shader.Program, "view"), 0);
        PielJaguar.Draw(shader);

        glm::mat4 model8(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model8));
        glUniform1i(glGetUniformLocation(shader.Program, "view"), 0);
        Piel2.Draw(shader);

		//Dibujo Paredes Chozas
        glm::mat4 model10(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model10));
        glUniform1i(glGetUniformLocation(shader.Program, "view"), 0);
        ParedesChozas.Draw(shader);


		//Techo Chozas
        glm::mat4 model11(1.0f); 
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model11));
        glUniform1i(glGetUniformLocation(shader.Program, "view"), 0);
        TechosChozas.Draw(shader);


        // Pirámide
        glm::mat4 model12(1.0f);
        model12 = glm::translate(model12, glm::vec3(0.0f, 0.0f, -100.0f));
        model12 = glm::scale(model12, glm::vec3(0.5f));
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model12));
        glUniform1i(glGetUniformLocation(shader.Program, "view"), 1);
        Piramide.Draw(shader);


        //calendario
        glm::mat4 model13(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model13));
        glUniform1i(glGetUniformLocation(shader.Program, "uApplyDesert"), 1);
        Calendario.Draw(shader);

        // ====== Mesa / silla / florero / chihuahua / fogata ======

        
    

        // ====== Procedural (gProg) ======

        glUseProgram(gProg);
        GLint uProj = glGetUniformLocation(gProg, "projection");
        GLint uView = glGetUniformLocation(gProg, "view");
        glUniformMatrix4fv(uProj, 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(uView, 1, GL_FALSE, glm::value_ptr(view));
        glUniform1f(glGetUniformLocation(gProg, "uTime"), currentFrame);

        auto drawCubeAt = [&](glm::vec3 pos, glm::vec3 scl, int mode = 1) {
            glm::mat4 M(1.0f);
            M = glm::translate(M, pos);
            M = glm::scale(M, scl);
            glUniformMatrix4fv(glGetUniformLocation(gProg, "model"), 1, GL_FALSE, glm::value_ptr(M));
            glUniform1i(glGetUniformLocation(gProg, "uMode"), mode);
            glBindVertexArray(gVAOCube);
            glDrawArrays(GL_TRIANGLES, 0, gCubeVerts);
            };

        // Suelo verde (pasto)
        {
            glm::mat4 MG(1.0f);
            MG = glm::translate(MG, glm::vec3(0.0f, -0.001f, 0.0f));
            MG = glm::scale(MG, glm::vec3(200.0f, 1.0f, 200.0f));
            glUniformMatrix4fv(glGetUniformLocation(gProg, "model"), 1, GL_FALSE, glm::value_ptr(MG));
            glUniform1i(glGetUniformLocation(gProg, "uMode"), 10);
            glBindVertexArray(gVAOSeat);
            glDrawArrays(GL_TRIANGLES, 0, gSeatVerts);
            glBindVertexArray(0);
        }

        // Mesa
        {
            float ox = topX * 0.5f - legT * 0.5f;
            float oz = topZ * 0.5f - legT * 0.5f;
            drawCubeAt(gTablePos + glm::vec3(0.0f, legH + topY * 0.5f, 0.0f), glm::vec3(topX, topY, topZ)); // cubierta
            // patas
            drawCubeAt(gTablePos + glm::vec3(+ox, legH * 0.5f, +oz), glm::vec3(legT, legH, legT));
            drawCubeAt(gTablePos + glm::vec3(-ox, legH * 0.5f, +oz), glm::vec3(legT, legH, legT));
            drawCubeAt(gTablePos + glm::vec3(+ox, legH * 0.5f, -oz), glm::vec3(legT, legH, legT));
            drawCubeAt(gTablePos + glm::vec3(-ox, legH * 0.5f, -oz), glm::vec3(legT, legH, legT));
            // travesaños
            drawCubeAt(gTablePos + glm::vec3(0.0f, railH, +oz), glm::vec3(topX - legT * 1.4f, railT, legT));
            drawCubeAt(gTablePos + glm::vec3(0.0f, railH, -oz), glm::vec3(topX - legT * 1.4f, railT, legT));
            drawCubeAt(gTablePos + glm::vec3(+ox, railH, 0.0f), glm::vec3(legT, railT, topZ - legT * 1.4f));
            drawCubeAt(gTablePos + glm::vec3(-ox, railH, 0.0f), glm::vec3(legT, railT, topZ - legT * 1.4f));
        }

        // Silla (estructura)
        auto chair = [&](glm::vec3 lp, glm::vec3 s) {
            drawCubeAt(gChairPos + lp, s, 1);
            };

        chair(glm::vec3(+seat * 0.5f - tLeg * 0.5f, 0.75f * 0.5f, +seat * 0.5f - tLeg * 0.5f), glm::vec3(tLeg, 0.75f, tLeg));
        chair(glm::vec3(-seat * 0.5f + tLeg * 0.5f, 0.75f * 0.5f, +seat * 0.5f - tLeg * 0.5f), glm::vec3(tLeg, 0.75f, tLeg));
        chair(glm::vec3(+seat * 0.5f - tLeg * 0.5f, 0.75f * 0.5f, -seat * 0.5f + tLeg * 0.5f), glm::vec3(tLeg, 0.75f, tLeg));
        chair(glm::vec3(-seat * 0.5f + tLeg * 0.5f, 0.75f * 0.5f, -seat * 0.5f + tLeg * 0.5f), glm::vec3(tLeg, 0.75f, tLeg));
        chair(glm::vec3(0.0f, 0.35f, +seat * 0.5f - tLeg * 0.5f), glm::vec3(seat - tLeg * 1.2f, 0.05f, tLeg));
        chair(glm::vec3(0.0f, 0.35f, -seat * 0.5f + tLeg * 0.5f), glm::vec3(seat - tLeg * 1.2f, 0.05f, tLeg));
        chair(glm::vec3(+seat * 0.5f - tLeg * 0.5f, 0.35f, 0.0f), glm::vec3(tLeg, 0.05f, seat - tLeg * 1.2f));
        chair(glm::vec3(-seat * 0.5f + tLeg * 0.5f, 0.35f, 0.0f), glm::vec3(tLeg, 0.05f, seat - tLeg * 1.2f));

        // Asiento tejido
        {
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
            MV = glm::translate(MV, gTablePos + glm::vec3(0.0f, legH + topY + vaseH * 0.5f, 0.0f));
            MV = glm::scale(MV, glm::vec3(vaseH));
            glUniformMatrix4fv(glGetUniformLocation(gProg, "model"), 1, GL_FALSE, glm::value_ptr(MV));
            glUniform1i(glGetUniformLocation(gProg, "uMode"), 2);
            glBindVertexArray(gVAOVase);
            glDrawArrays(GL_TRIANGLES, 0, gVaseVerts);
            glBindVertexArray(0);
        }

        // Chihuahua (único)
        auto drawPart = [&](const glm::vec3& base, float yaw, glm::vec3 local, glm::vec3 scl, int mode) {
            glm::mat4 M(1.0f);
            M = glm::translate(M, base);
            M = glm::rotate(M, glm::radians(yaw), glm::vec3(0, 1, 0));
            M = glm::translate(M, local);
            M = glm::scale(M, scl);
            glUniformMatrix4fv(glGetUniformLocation(gProg, "model"), 1, GL_FALSE, glm::value_ptr(M));
            glUniform1i(glGetUniformLocation(gProg, "uMode"), mode);
            glBindVertexArray(gVAOCube);
            glDrawArrays(GL_TRIANGLES, 0, gCubeVerts);
            };

        auto drawDog = [&](glm::vec3 base, float yaw, float s) {
            // cuerpo
            drawPart(base, yaw, glm::vec3(+0.10f, 0.18f, 0.0f), glm::vec3(0.38f * s, 0.22f * s, 0.22f * s), 3);
            drawPart(base, yaw, glm::vec3(-0.18f, 0.19f, 0.0f), glm::vec3(0.30f * s, 0.20f * s, 0.22f * s), 3);
            // cuello/cabeza/hocico/nariz
            drawPart(base, yaw, glm::vec3(+0.30f, 0.28f, 0.0f), glm::vec3(0.10f * s, 0.16f * s, 0.16f * s), 3);
            drawPart(base, yaw, glm::vec3(+0.43f, 0.34f, 0.0f), glm::vec3(0.18f * s, 0.16f * s, 0.18f * s), 3);
            drawPart(base, yaw, glm::vec3(+0.55f, 0.30f, 0.0f), glm::vec3(0.12f * s, 0.10f * s, 0.12f * s), 3);
            drawPart(base, yaw, glm::vec3(+0.62f, 0.30f, 0.0f), glm::vec3(0.06f * s, 0.06f * s, 0.06f * s), 4);
            // lengua
            drawPart(base, yaw, glm::vec3(+0.58f, 0.22f, 0.0f), glm::vec3(0.06f * s, 0.02f * s, 0.04f * s), 6);
            // orejas/ojos
            drawPart(base, yaw, glm::vec3(+0.46f, 0.48f, +0.07f), glm::vec3(0.06f * s, 0.14f * s, 0.06f * s), 4);
            drawPart(base, yaw, glm::vec3(+0.46f, 0.48f, -0.07f), glm::vec3(0.06f * s, 0.14f * s, 0.06f * s), 4);
            drawPart(base, yaw, glm::vec3(+0.52f, 0.36f, +0.08f), glm::vec3(0.03f * s, 0.03f * s, 0.03f * s), 4);
            drawPart(base, yaw, glm::vec3(+0.52f, 0.36f, -0.08f), glm::vec3(0.03f * s, 0.03f * s, 0.03f * s), 4);
            // patas
            drawPart(base, yaw, glm::vec3(+0.20f, 0.09f, +0.09f), glm::vec3(0.07f * s, 0.18f * s, 0.07f * s), 3);
            drawPart(base, yaw, glm::vec3(+0.20f, 0.09f, -0.09f), glm::vec3(0.07f * s, 0.18f * s, 0.07f * s), 3);
            drawPart(base, yaw, glm::vec3(-0.22f, 0.09f, +0.09f), glm::vec3(0.07f * s, 0.18f * s, 0.07f * s), 3);
            drawPart(base, yaw, glm::vec3(-0.22f, 0.09f, -0.09f), glm::vec3(0.07f * s, 0.18f * s, 0.07f * s), 3);
            // cola
            drawPart(base, yaw, glm::vec3(-0.32f, 0.32f, 0.0f), glm::vec3(0.05f * s, 0.16f * s, 0.05f * s), 3);
            };

        float zDog = gTablePos.z + topZ * 0.5f + 0.55f;
        float xDog = gTablePos.x - 0.30f;
        drawDog(glm::vec3(xDog, 0.0f, zDog), 10.0f, 1.0f);

        // Máscara de luchador (sobre la mesa)
        {
            glm::vec3 base = gTablePos + glm::vec3(+0.45f, legH + topY + 0.03f, +0.35f);
            // base roja + cruces blancas + ojos negros
            drawCubeAt(base, glm::vec3(0.22f, 0.02f, 0.16f), 7);
            drawCubeAt(base + glm::vec3(0.0f, 0.011f, 0.0f), glm::vec3(0.24f, 0.006f, 0.05f), 8);
            drawCubeAt(base + glm::vec3(0.0f, 0.011f, 0.0f), glm::vec3(0.05f, 0.006f, 0.18f), 8);
            drawCubeAt(base + glm::vec3(+0.06f, 0.013f, +0.05f), glm::vec3(0.04f, 0.004f, 0.04f), 9);
            drawCubeAt(base + glm::vec3(+0.06f, 0.013f, -0.05f), glm::vec3(0.04f, 0.004f, 0.04f), 9);
        }

        // Fogata (troncos + llama)
        auto place = [&](glm::vec3 pos, glm::vec3 rotDeg, glm::vec3 scl, int mode) {
            glm::mat4 M(1.0f);
            M = glm::translate(M, pos);
            if (rotDeg.x != 0) M = glm::rotate(M, glm::radians(rotDeg.x), glm::vec3(1, 0, 0));
            if (rotDeg.y != 0) M = glm::rotate(M, glm::radians(rotDeg.y), glm::vec3(0, 1, 0));
            if (rotDeg.z != 0) M = glm::rotate(M, glm::radians(rotDeg.z), glm::vec3(0, 0, 1));
            M = glm::scale(M, scl);
            glUniformMatrix4fv(glGetUniformLocation(gProg, "model"), 1, GL_FALSE, glm::value_ptr(M));
            glUniform1i(glGetUniformLocation(gProg, "uMode"), mode);
            };

        // troncos
        place(gCampPos + glm::vec3(0.0f, 0.06f, 0.0f), glm::vec3(0, 20, 0), glm::vec3(1.0f), 1);
        glBindVertexArray(gVAOLog); glDrawArrays(GL_TRIANGLES, 0, gLogVerts);

        place(gCampPos + glm::vec3(0.0f, 0.06f, 0.0f), glm::vec3(0, 110, 0), glm::vec3(1.0f), 1);
        glBindVertexArray(gVAOLog); glDrawArrays(GL_TRIANGLES, 0, gLogVerts);

        // llama
        place(gCampPos + glm::vec3(0.0f, 0.06f, 0.0f), glm::vec3(0, 0, 0), glm::vec3(1.0f), 0);
        glDepthMask(GL_FALSE);
        glBindVertexArray(gVAOFlame); glDrawArrays(GL_TRIANGLES, 0, gFlameVerts);
        glBindVertexArray(0);
        glDepthMask(GL_TRUE);

        // Antorchas
        auto drawTorch = [&](glm::vec3 p, float h) {
            // palo
            glm::mat4 M(1.0f);
            M = glm::translate(M, p + glm::vec3(0, h * 0.5f, 0));
            M = glm::scale(M, glm::vec3(0.08f, h, 0.08f));
            glUniformMatrix4fv(glGetUniformLocation(gProg, "model"), 1, GL_FALSE, glm::value_ptr(M));
            glUniform1i(glGetUniformLocation(gProg, "uMode"), 1);
            glBindVertexArray(gVAOCube); glDrawArrays(GL_TRIANGLES, 0, gCubeVerts);

            // copa
            M = glm::mat4(1.0f);
            M = glm::translate(M, p + glm::vec3(0, h + 0.04f, 0));
            M = glm::scale(M, glm::vec3(0.16f, 0.06f, 0.16f));
            glUniformMatrix4fv(glGetUniformLocation(gProg, "model"), 1, GL_FALSE, glm::value_ptr(M));
            glUniform1i(glGetUniformLocation(gProg, "uMode"), 1);
            glBindVertexArray(gVAOCube); glDrawArrays(GL_TRIANGLES, 0, gCubeVerts);

            // llama
            glm::mat4 F(1.0f);
            F = glm::translate(F, p + glm::vec3(0, h + 0.06f, 0));
            glUniformMatrix4fv(glGetUniformLocation(gProg, "model"), 1, GL_FALSE, glm::value_ptr(F));
            glUniform1i(glGetUniformLocation(gProg, "uMode"), 0);
            glDepthMask(GL_FALSE);
            glBindVertexArray(gVAOFlame); glDrawArrays(GL_TRIANGLES, 0, gFlameVerts);
            glDepthMask(GL_TRUE);
            };

        // 2 antorchas junto a la fogata
        drawTorch(gCampPos + glm::vec3(+0.9f, 0.0f, +0.6f), 1.2f);
        drawTorch(gCampPos + glm::vec3(-0.9f, 0.0f, +0.6f), 1.2f);

        // 2 antorchas frente a la pirámide
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

void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);

    if (key >= 0 && key < 1024) {
        if (action == GLFW_PRESS)   keys[key] = true;
        else if (action == GLFW_RELEASE) keys[key] = false;
    }
}

void MouseCallback(GLFWwindow* window, double xPos, double yPos) {
    if (firstMouse) {
        lastX = (GLfloat)xPos; lastY = (GLfloat)yPos; firstMouse = false;
    }
    GLfloat xOffset = (GLfloat)xPos - lastX;
    GLfloat yOffset = lastY - (GLfloat)yPos;
    lastX = (GLfloat)xPos; lastY = (GLfloat)yPos;
    camera.ProcessMouseMovement(xOffset, yOffset);
}
