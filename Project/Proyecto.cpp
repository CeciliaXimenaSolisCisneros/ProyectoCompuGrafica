#include <iostream>
#include <vector>
#include <cmath>

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

// Tiempo
GLfloat deltaTime = 0.0f;
GLfloat lastFrame = 0.0f;

// Posiciones base
glm::vec3 gTablePos = glm::vec3(-1.8f, 0.0f, -6.0f);
glm::vec3 gChairPos = glm::vec3(-1.2f, 0.0f, -6.2f);
glm::vec3 gCampPos = glm::vec3(-3.2f, 0.0f, -9.8f); // fogata lejos de mesa

// Shader embebido único
GLuint gProg = 0;

// VAOs
GLuint gVAOCube = 0, gVBOCube = 0; GLsizei gCubeVerts = 0;
GLuint gVAOSeat = 0, gVBOSeat = 0; GLsizei gSeatVerts = 0;
GLuint gVAOVase = 0, gVBOVase = 0; GLsizei gVaseVerts = 0;
GLuint gVAOFlame = 0, gVBOFlame = 0; GLsizei gFlameVerts = 0;
GLuint gVAOLog = 0, gVBOLog = 0; GLsizei gLogVerts = 0;

// ===== Shaders =====
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
// 0=fuego 1=madera 2=florero 3=pelaje_claro 4=oscuro 5=tejido 6=rosa

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
    if(uMode==1){ FragColor=vec4(wood(vPos),1.0); return; }
    if(uMode==0){
        float h=clamp(vPos.y,0.0,1.0);
        float r=length(vPos.xz);
        float edge=smoothstep(0.35,0.15,r);
        float t=uTime*2.0;
        float flick=noise(vec2(r*6.0,h*8.0+t*3.5))*0.6 + noise(vec2(r*10.0+t*1.7,h*5.0))*0.4;
        vec3 c1=vec3(1.0,0.25,0.02), c2=vec3(1.0,0.55,0.05), c3=vec3(1.0,0.85,0.25), c4=vec3(1.0,0.95,0.75);
        float k=clamp(h*1.2+flick*0.2,0.0,1.0);
        vec3 col=mix(c1,c2,k); col=mix(col,c3,k*k); col=mix(col,c4,pow(k,4.0));
        float a=edge*(0.85+0.15*sin(t*7.0+r*10.0)); a*=(0.35+0.65*h);
        FragColor=vec4(col,clamp(a,0.0,1.0)); return;
    }
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
    if(uMode==2){
        vec3 terracotta=vec3(0.63,0.28,0.20);
        vec3 crema=vec3(0.90,0.82,0.72);
        float b=smoothstep(-0.15,0.15,sin(vPos.y*18.0));
        vec3 col=mix(terracotta,crema,b*0.25);
        if(abs(vPos.y-0.15)<0.06) col=mix(col,vec3(0.14,0.02,0.02),0.85);
        if(abs(vPos.y-0.00)<0.06) col=mix(col,vec3(0.14,0.02,0.02),0.85);
        FragColor=vec4(col,1.0); return;
    }
    if(uMode==3){ FragColor=vec4(0.84,0.72,0.54,1.0); return; } // pelaje claro
    if(uMode==4){ FragColor=vec4(0.10,0.07,0.06,1.0); return; } // oscuro
    if(uMode==6){ FragColor=vec4(0.88,0.42,0.55,1.0); return; } // lengüita
    FragColor=vec4(1,0,1,1);
})";

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

// ===== Geometrías =====
static void BuildCube() {
    float v[] = {
        -0.5f,-0.5f,0.5f,  0.5f,-0.5f,0.5f,  0.5f,0.5f,0.5f,
        -0.5f,-0.5f,0.5f,  0.5f,0.5f,0.5f, -0.5f,0.5f,0.5f,
        -0.5f,-0.5f,-0.5f,-0.5f,0.5f,-0.5f, 0.5f,0.5f,-0.5f,
        -0.5f,-0.5f,-0.5f, 0.5f,0.5f,-0.5f, 0.5f,-0.5f,-0.5f,
         0.5f,-0.5f,-0.5f, 0.5f,0.5f,-0.5f, 0.5f,0.5f,0.5f,
         0.5f,-0.5f,-0.5f, 0.5f,0.5f,0.5f, 0.5f,-0.5f,0.5f,
        -0.5f,-0.5f,-0.5f,-0.5f,-0.5f,0.5f,-0.5f,0.5f,0.5f,
        -0.5f,-0.5f,-0.5f,-0.5f,0.5f,0.5f,-0.5f,0.5f,-0.5f,
        -0.5f,0.5f,-0.5f,-0.5f,0.5f,0.5f, 0.5f,0.5f,0.5f,
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
    std::vector<glm::vec3> v;
    std::vector<glm::vec2> prof = { {0.00f,-0.30f},{0.25f,-0.30f},{0.35f,-0.20f},{0.42f,-0.05f},
                                 {0.43f, 0.10f},{0.36f, 0.20f},{0.28f, 0.28f},{0.22f, 0.32f},
                                 {0.18f, 0.34f},{0.16f, 0.35f} };
    int seg = 64;
    for (size_t k = 0;k + 1 < prof.size();++k) {
        float r0 = prof[k].x, y0 = prof[k].y, r1 = prof[k + 1].x, y1 = prof[k + 1].y;
        for (int i = 0;i < seg;i++) {
            float a0 = i * (2.f * 3.14159265f / seg), a1 = (i + 1) * (2.f * 3.14159265f / seg);
            glm::vec3 A(r0 * cosf(a0), y0, r0 * sinf(a0)), B(r0 * cosf(a1), y0, r0 * sinf(a1));
            glm::vec3 C(r1 * cosf(a1), y1, r1 * sinf(a1)), D(r1 * cosf(a0), y1, r1 * sinf(a0));
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

static void BuildCampfire() {
    // tronco = prisma largo
    float L = 1.4f, H = 0.12f, W = 0.12f;
    float x0 = -L * 0.5f, x1 = L * 0.5f, y0 = -H * 0.5f, y1 = H * 0.5f, z0 = -W * 0.5f, z1 = W * 0.5f;
    glm::vec3 p[] = { {x0,y0,z0},{x1,y0,z0},{x1,y1,z0},{x0,y1,z0},{x0,y0,z1},{x1,y0,z1},{x1,y1,z1},{x0,y1,z1} };
    auto quad = [&](int a, int b, int c, int d, std::vector<glm::vec3>& o) {
        o.push_back(p[a]);o.push_back(p[b]);o.push_back(p[c]);
        o.push_back(p[a]);o.push_back(p[c]);o.push_back(p[d]);};
    std::vector<glm::vec3> log;
    quad(0, 1, 2, 3, log); quad(4, 5, 6, 7, log); // caras
    quad(0, 4, 7, 3, log); quad(1, 5, 6, 2, log);
    quad(3, 2, 6, 7, log); quad(0, 1, 5, 4, log);
    gLogVerts = (GLsizei)log.size();
    glGenVertexArrays(1, &gVAOLog); glGenBuffers(1, &gVBOLog);
    glBindVertexArray(gVAOLog); glBindBuffer(GL_ARRAY_BUFFER, gVBOLog);
    glBufferData(GL_ARRAY_BUFFER, log.size() * sizeof(glm::vec3), log.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glBindVertexArray(0);

    // llama = cono doble cara
    const int seg = 48; const float r = 0.38f;
    std::vector<glm::vec3> flame; glm::vec3 tip(0, 1.2f, 0);
    for (int i = 0;i < seg;i++) {
        float a0 = i * (2.f * 3.14159265f / seg), a1 = (i + 1) * (2.f * 3.14159265f / seg);
        glm::vec3 A(0, 0, 0), B(r * cosf(a0), 0, r * sinf(a0)), C(r * cosf(a1), 0, r * sinf(a1));
        flame.push_back(A); flame.push_back(B); flame.push_back(C);
        flame.push_back(B); flame.push_back(C); flame.push_back(tip);
    }
    gFlameVerts = (GLsizei)flame.size();
    glGenVertexArrays(1, &gVAOFlame); glGenBuffers(1, &gVBOFlame);
    glBindVertexArray(gVAOFlame); glBindBuffer(GL_ARRAY_BUFFER, gVBOFlame);
    glBufferData(GL_ARRAY_BUFFER, flame.size() * sizeof(glm::vec3), flame.data(), GL_STATIC_DRAW);
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

    // Modelos del tianguis y pirámide
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
    BuildCube();
    BuildSeatPlane();
    BuildVase();
    BuildCampfire();

    glm::mat4 projection = glm::perspective(camera.GetZoom(), (GLfloat)SCREEN_WIDTH / (GLfloat)SCREEN_HEIGHT, 0.1f, 100.0f);

    // Mesa acostada (largo en Z) + silla
    const float topX = 1.6f, topZ = 3.0f, topY = 0.12f;
    const float legH = 0.90f, legT = 0.09f, railT = 0.06f, railH = 0.32f;
    const float seat = 0.60f, tLeg = 0.06f;
    gChairPos = gTablePos + glm::vec3(topX * 0.5f + 0.45f + seat * 0.5f, 0.0f, -topZ * 0.25f);

    while (!glfwWindowShouldClose(window)) {
        GLfloat currentFrame = (GLfloat)glfwGetTime();
        deltaTime = currentFrame - lastFrame; lastFrame = currentFrame;
        glfwPollEvents(); DoMovement();

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.Use();
        glm::mat4 view = camera.GetViewMatrix();
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "view"), 1, GL_FALSE, glm::value_ptr(view));

        // DIBUJAR MODELOS DEL TIANGUIS
        glm::mat4 model(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model));
        CanastaChiles.Draw(shader);

        //Dibujo Chiles de canasta Tianguis
        glm::mat4 model2(1.0f);
        //model2 = glm::translate(model2, glm::vec3(0.5f, 0.0f, 0.0f));
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model2));
        Chiles.Draw(shader);

        //Dibujo Petates de tianguis
        glm::mat4 model3(1.0f);
        //model3 = glm::translate(model2, glm::vec3(0.5f, 0.0f, 0.0f));
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model3));
        PetatesTianguis.Draw(shader);

        //Dibujo Aguacates Tianguis
        glm::mat4 model4(1.0f);
        //model4 = glm::translate(model2, glm::vec3(0.5f, 0.0f, 0.0f));
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model4));
        Aguacates.Draw(shader);

        //Dibujo Jarrones Tianguis
        glm::mat4 model5(1.0f);
        //model4 = glm::translate(model2, glm::vec3(0.5f, 0.0f, 0.0f));
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model5));
        Jarrones.Draw(shader);

        //Dibujo Tendedero Tianguis
        glm::mat4 model6(1.0f);
        //model4 = glm::translate(model2, glm::vec3(0.5f, 0.0f, 0.0f));
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model6));
        Tendedero.Draw(shader);

        //Dibujo Tendedero Piel Jaguar
        glm::mat4 model7(1.0f);
        //model4 = glm::translate(model2, glm::vec3(0.5f, 0.0f, 0.0f));
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model7));
        PielJaguar.Draw(shader);

        //Dibujo Tendedero Piel 2
        glm::mat4 model8(1.0f);
        //model4 = glm::translate(model2, glm::vec3(0.5f, 0.0f, 0.0f));
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model8));
        Piel2.Draw(shader);

        //Dibujo Piramide
        glm::mat4 model9(1.0f);
        model9 = glm::translate(model9, glm::vec3(0.0f, 0.0f, -100.0f));
        model9 = glm::scale(model9, glm::vec3(0.5f, 0.5f, 0.5f));
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model9));
        Piramide.Draw(shader);

        // ====== Mesa / silla / florero / chihuahua / fogata ======
        glUseProgram(gProg);
        GLint uProj = glGetUniformLocation(gProg, "projection");
        GLint uView = glGetUniformLocation(gProg, "view");
        glUniformMatrix4fv(uProj, 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(uView, 1, GL_FALSE, glm::value_ptr(view));
        glUniform1f(glGetUniformLocation(gProg, "uTime"), currentFrame);

        auto drawCubeAt = [&](glm::vec3 pos, glm::vec3 scl, int mode = 1) {
            glm::mat4 M(1.0f); M = glm::translate(M, pos); M = glm::scale(M, scl);
            glUniformMatrix4fv(glGetUniformLocation(gProg, "model"), 1, GL_FALSE, glm::value_ptr(M));
            glUniform1i(glGetUniformLocation(gProg, "uMode"), mode);
            glBindVertexArray(gVAOCube); glDrawArrays(GL_TRIANGLES, 0, gCubeVerts);
            };

        // Mesa
        float ox = topX * 0.5f - legT * 0.5f, oz = topZ * 0.5f - legT * 0.5f;
        drawCubeAt(gTablePos + glm::vec3(0.0f, legH + topY * 0.5f, 0.0f), glm::vec3(topX, topY, topZ));
        drawCubeAt(gTablePos + glm::vec3(+ox, legH * 0.5f, +oz), glm::vec3(legT, legH, legT));
        drawCubeAt(gTablePos + glm::vec3(-ox, legH * 0.5f, +oz), glm::vec3(legT, legH, legT));
        drawCubeAt(gTablePos + glm::vec3(+ox, legH * 0.5f, -oz), glm::vec3(legT, legH, legT));
        drawCubeAt(gTablePos + glm::vec3(-ox, legH * 0.5f, -oz), glm::vec3(legT, legH, legT));
        drawCubeAt(gTablePos + glm::vec3(0.0f, railH, +oz), glm::vec3(topX - legT * 1.4f, railT, legT));
        drawCubeAt(gTablePos + glm::vec3(0.0f, railH, -oz), glm::vec3(topX - legT * 1.4f, railT, legT));
        drawCubeAt(gTablePos + glm::vec3(+ox, railH, 0.0f), glm::vec3(legT, railT, topZ - legT * 1.4f));
        drawCubeAt(gTablePos + glm::vec3(-ox, railH, 0.0f), glm::vec3(legT, railT, topZ - legT * 1.4f));

        // Silla
        auto chair = [&](glm::vec3 lp, glm::vec3 s) { drawCubeAt(gChairPos + lp, s, 1); };
        const float hLegC = 0.75f, railHC = 0.35f, railTC = 0.05f;
        chair(glm::vec3(+seat * 0.5f - tLeg * 0.5f, hLegC * 0.5f, +seat * 0.5f - tLeg * 0.5f), glm::vec3(tLeg, hLegC, tLeg));
        chair(glm::vec3(-seat * 0.5f + tLeg * 0.5f, hLegC * 0.5f, +seat * 0.5f - tLeg * 0.5f), glm::vec3(tLeg, hLegC, tLeg));
        chair(glm::vec3(+seat * 0.5f - tLeg * 0.5f, hLegC * 0.5f, -seat * 0.5f + tLeg * 0.5f), glm::vec3(tLeg, hLegC, tLeg));
        chair(glm::vec3(-seat * 0.5f + tLeg * 0.5f, hLegC * 0.5f, -seat * 0.5f + tLeg * 0.5f), glm::vec3(tLeg, hLegC, tLeg));
        chair(glm::vec3(0.0f, railHC, +seat * 0.5f - tLeg * 0.5f), glm::vec3(seat - tLeg * 1.2f, railTC, tLeg));
        chair(glm::vec3(0.0f, railHC, -seat * 0.5f + tLeg * 0.5f), glm::vec3(seat - tLeg * 1.2f, railTC, tLeg));
        chair(glm::vec3(+seat * 0.5f - tLeg * 0.5f, railHC, 0.0f), glm::vec3(tLeg, railTC, seat - tLeg * 1.2f));
        chair(glm::vec3(-seat * 0.5f + tLeg * 0.5f, railHC, 0.0f), glm::vec3(tLeg, railTC, seat - tLeg * 1.2f));

        {   // asiento tejido
            glm::mat4 MS(1.0f);
            MS = glm::translate(MS, gChairPos + glm::vec3(0.0f, hLegC, 0.0f));
            MS = glm::scale(MS, glm::vec3(seat, 1.0f, seat));
            glUniformMatrix4fv(glGetUniformLocation(gProg, "model"), 1, GL_FALSE, glm::value_ptr(MS));
            glUniform1i(glGetUniformLocation(gProg, "uMode"), 5);
            glBindVertexArray(gVAOSeat); glDrawArrays(GL_TRIANGLES, 0, gSeatVerts); glBindVertexArray(0);
        }

        // Florero centrado
        {
            float vaseH = 0.60f;
            glm::mat4 MV(1.0f);
            MV = glm::translate(MV, gTablePos + glm::vec3(0.0f, legH + topY + vaseH * 0.5f, 0.0f));
            MV = glm::scale(MV, glm::vec3(vaseH));
            glUniformMatrix4fv(glGetUniformLocation(gProg, "model"), 1, GL_FALSE, glm::value_ptr(MV));
            glUniform1i(glGetUniformLocation(gProg, "uMode"), 2);
            glBindVertexArray(gVAOVase); glDrawArrays(GL_TRIANGLES, 0, gVaseVerts); glBindVertexArray(0);
        }

        // ===== ÚNICO CHIHUAHUA JUNTO A LA MESA =====
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
            // cuello y cabeza
            drawPart(base, yaw, glm::vec3(+0.30f, 0.28f, 0.0f), glm::vec3(0.10f * s, 0.16f * s, 0.16f * s), 3);
            drawPart(base, yaw, glm::vec3(+0.43f, 0.34f, 0.0f), glm::vec3(0.18f * s, 0.16f * s, 0.18f * s), 3);
            // hocico/nariz/lengua
            drawPart(base, yaw, glm::vec3(+0.55f, 0.30f, 0.0f), glm::vec3(0.12f * s, 0.10f * s, 0.12f * s), 3);
            drawPart(base, yaw, glm::vec3(+0.62f, 0.30f, 0.0f), glm::vec3(0.06f * s, 0.06f * s, 0.06f * s), 4);
            drawPart(base, yaw, glm::vec3(+0.58f, 0.22f, 0.0f), glm::vec3(0.06f * s, 0.02f * s, 0.04f * s), 6);
            // orejas y ojos
            drawPart(base, yaw, glm::vec3(+0.46f, 0.48f, +0.07f), glm::vec3(0.06f * s, 0.14f * s, 0.06f * s), 4);
            drawPart(base, yaw, glm::vec3(+0.46f, 0.48f, -0.07f), glm::vec3(0.06f * s, 0.14f * s, 0.06f * s), 4);
            drawPart(base, yaw, glm::vec3(+0.52f, 0.36f, +0.08f), glm::vec3(0.03f * s, 0.03f * s, 0.03f * s), 4);
            drawPart(base, yaw, glm::vec3(+0.52f, 0.36f, -0.08f), glm::vec3(0.03f * s, 0.03f * s, 0.03f * s), 4);
            // patas y cola
            drawPart(base, yaw, glm::vec3(+0.20f, 0.09f, +0.09f), glm::vec3(0.07f * s, 0.18f * s, 0.07f * s), 3);
            drawPart(base, yaw, glm::vec3(+0.20f, 0.09f, -0.09f), glm::vec3(0.07f * s, 0.18f * s, 0.07f * s), 3);
            drawPart(base, yaw, glm::vec3(-0.22f, 0.09f, +0.09f), glm::vec3(0.07f * s, 0.18f * s, 0.07f * s), 3);
            drawPart(base, yaw, glm::vec3(-0.22f, 0.09f, -0.09f), glm::vec3(0.07f * s, 0.18f * s, 0.07f * s), 3);
            drawPart(base, yaw, glm::vec3(-0.32f, 0.32f, 0.0f), glm::vec3(0.05f * s, 0.16f * s, 0.05f * s), 3);
            };

        // Posición única junto a la mesa (lado +Z), sin estorbar
        float zDog = gTablePos.z + topZ * 0.5f + 0.55f;
        float xDog = gTablePos.x - 0.30f;
        drawDog(glm::vec3(xDog, 0.0f, zDog), 10.0f, 1.0f);

        // ===== Fogata: 2 troncos cruzados + llama =====
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

        // tronco 1
        place(gCampPos + glm::vec3(0.0f, 0.06f, 0.0f), glm::vec3(0, 20, 0), glm::vec3(1.0f), 1);
        glBindVertexArray(gVAOLog); glDrawArrays(GL_TRIANGLES, 0, gLogVerts);
        // tronco 2
        place(gCampPos + glm::vec3(0.0f, 0.06f, 0.0f), glm::vec3(0, 110, 0), glm::vec3(1.0f), 1);
        glBindVertexArray(gVAOLog); glDrawArrays(GL_TRIANGLES, 0, gLogVerts);

        // llama
        place(gCampPos + glm::vec3(0.0f, 0.06f, 0.0f), glm::vec3(0, 0, 0), glm::vec3(1.0f), 0);
        glDepthMask(GL_FALSE);
        glBindVertexArray(gVAOFlame); glDrawArrays(GL_TRIANGLES, 0, gFlameVerts);
        glBindVertexArray(0);
        glDepthMask(GL_TRUE);

        glUseProgram(0);
        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}

// -------- input ----------
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
