// part2.cpp
// Build: g++ part2.cpp glad.c -ldl -lglfw -lGL -I/path/to/glad/include -I/path/to/glm -o part2
// Run: ./part2 bound-bunny_200.smf

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cmath>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
};

struct Material {
    glm::vec4 ambient;
    glm::vec4 diffuse;
    glm::vec4 specular;
    float shininess;
};

struct Light {
    glm::vec4 ambient;
    glm::vec4 diffuse;
    glm::vec4 specular;
    glm::vec3 position; // position in world/object or view space depending on use
    bool inCameraSpace; // if true, position is relative to camera (eye)
};

// SMF loader
bool load_smf(const std::string& filename,
    std::vector<glm::vec3>& out_positions,
    std::vector<glm::uvec3>& out_faces) {
    std::ifstream in(filename);
    if (!in) return false;
    std::string line;
    std::vector<glm::vec3> positions;
    std::vector<glm::uvec3> faces;
    while (std::getline(in, line)) {
        if (line.size() == 0) continue;
        std::istringstream iss(line);
        char t;
        iss >> t;
        if (t == 'v') {
            float x, y, z; iss >> x >> y >> z;
            positions.emplace_back(x, y, z);
        }
        else if (t == 'f') {
            unsigned a, b, c; iss >> a >> b >> c;
            faces.emplace_back(a - 1, b - 1, c - 1);
        }
    }
    out_positions = std::move(positions);
    out_faces = std::move(faces);
    return true;
}

GLuint compile_shader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[1024]; glGetShaderInfoLog(s, 1024, nullptr, buf);
        std::cerr << "Shader compile error: " << buf << "\n";
    }
    return s;
}

GLuint create_program(const char* vsrc, const char* fsrc) {
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vsrc);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fsrc);
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[1024]; glGetProgramInfoLog(p, 1024, nullptr, buf);
        std::cerr << "Program link error: " << buf << "\n";
    }
    glDeleteShader(vs); glDeleteShader(fs);
    return p;
}

// GLSL sources are provided below (gouraud and phong). We'll create two shader programs at runtime and switch.

static const char* gouraud_vert = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

struct Material {
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    float shininess;
};
struct Light {
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    vec3 position; // in same space as calculation
    int inCameraSpace;
};

uniform Material material;
uniform Light light0;
uniform Light light1;
uniform vec3 eyePos; // in world coords

out vec3 vColor;

vec3 calcPhongColor(vec3 pos, vec3 N, Light light) {
    vec3 ambient = vec3(light.ambient * material.ambient);
    vec3 L;
    if (light.inCameraSpace == 1) {
        // light in view space: transform pos to view and compute there (we pass light in view space to shader for simplicity)
        // We'll assume light.position is provided in view space and pos is in view space.
        // But for simplicity we'll convert both to world space externally; here we just compute with world-space positions.
    }
    vec3 lightDir = normalize(light.position - pos);
    float diff = max(dot(N, lightDir), 0.0);
    vec3 diffuse = vec3(light.diffuse * material.diffuse) * diff;
    vec3 V = normalize(eyePos - pos);
    vec3 R = reflect(-lightDir, N);
    float spec = 0.0;
    if (diff>0.0) spec = pow(max(dot(R,V),0.0), material.shininess);
    vec3 specular = vec3(light.specular * material.specular) * spec;
    return ambient + diffuse + specular;
}

void main(){
    mat4 model = uModel;
    vec3 worldPos = vec3(model * vec4(aPos,1.0));
    vec3 worldN = normalize(mat3(transpose(inverse(model))) * aNormal);

    vec3 color = vec3(0.0);
    color += calcPhongColor(worldPos, worldN, light0);
    color += calcPhongColor(worldPos, worldN, light1);
    vColor = color;
    gl_Position = uProj * uView * model * vec4(aPos, 1.0);
}
)";

static const char* gouraud_frag = R"(
#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main(){
    FragColor = vec4(vColor, 1.0);
}
)";

static const char* phong_vert = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec3 FragPos;
out vec3 Normal;

void main(){
    FragPos = vec3(uModel * vec4(aPos,1.0));
    Normal = mat3(transpose(inverse(uModel))) * aNormal;
    gl_Position = uProj * uView * uModel * vec4(aPos,1.0);
}
)";

static const char* phong_frag = R"(
#version 330 core
in vec3 FragPos;
in vec3 Normal;
out vec4 FragColor;

struct Material {
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    float shininess;
};
struct Light {
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    vec3 position;
    int inCameraSpace;
};

uniform Material material;
uniform Light light0;
uniform Light light1;
uniform vec3 eyePos;

vec3 calcLight(Light light, vec3 pos, vec3 N) {
    vec3 ambient = vec3(light.ambient * material.ambient);
    vec3 L = normalize(light.position - pos);
    float diff = max(dot(N,L), 0.0);
    vec3 diffuse = vec3(light.diffuse * material.diffuse) * diff;
    vec3 V = normalize(eyePos - pos);
    vec3 R = reflect(-L, N);
    float spec = 0.0;
    if (diff>0.0) spec = pow(max(dot(R,V),0.0), material.shininess);
    vec3 specular = vec3(light.specular * material.specular) * spec;
    return ambient + diffuse + specular;
}

void main(){
    vec3 N = normalize(Normal);
    vec3 color = vec3(0.0);
    color += calcLight(light0, FragPos, N);
    color += calcLight(light1, FragPos, N);
    FragColor = vec4(color, 1.0);
}
)";

// Globals for camera & light control
float camAngle = 0.0f, camRadius = 2.0f, camHeight = 0.0f;
float lightAngle = 0.0f, lightRadius = 2.0f, lightHeight = 0.0f;
bool perspectiveProj = true;
int shadingMode = 1; // 1=gouraud, 2=phong
int currentMaterial = 0;

void print_controls() {
    std::cout << "Controls:\n"
        << "A/D: camera angle  W/S: radius  Q/E: height\n"
        << "J/L: light angle  I/K: light radius  U/O: light height\n"
        << "1: Gouraud  2: Phong  M: change material  P: toggle projection\n"
        << "Esc: exit\n";
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        if (key == GLFW_KEY_A) camAngle -= 0.05f;
        if (key == GLFW_KEY_D) camAngle += 0.05f;
        if (key == GLFW_KEY_W) camRadius -= 0.05f;
        if (key == GLFW_KEY_S) camRadius += 0.05f;
        if (key == GLFW_KEY_Q) camHeight += 0.05f;
        if (key == GLFW_KEY_E) camHeight -= 0.05f;

        if (key == GLFW_KEY_J) lightAngle -= 0.05f;
        if (key == GLFW_KEY_L) lightAngle += 0.05f;
        if (key == GLFW_KEY_I) lightRadius -= 0.05f;
        if (key == GLFW_KEY_K) lightRadius += 0.05f;
        if (key == GLFW_KEY_U) lightHeight += 0.05f;
        if (key == GLFW_KEY_O) lightHeight -= 0.05f;

        if (key == GLFW_KEY_P) perspectiveProj = !perspectiveProj;
        if (key == GLFW_KEY_1) shadingMode = 1;
        if (key == GLFW_KEY_2) shadingMode = 2;
        if (key == GLFW_KEY_M && action == GLFW_PRESS) currentMaterial = (currentMaterial + 1) % 3;
        if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " model.smf\n"; return -1;
    }
    std::string filename = argv[1];
    std::vector<glm::vec3> positions;
    std::vector<glm::uvec3> faces;
    if (!load_smf(filename, positions, faces)) {
        std::cerr << "Failed to open " << filename << "\n"; return -1;
    }

    // compute per-vertex averaged normals
    std::vector<glm::vec3> faceNormals(faces.size());
    for (size_t i = 0; i < faces.size(); ++i) {
        glm::vec3 p0 = positions[faces[i].x], p1 = positions[faces[i].y], p2 = positions[faces[i].z];
        glm::vec3 n = glm::normalize(glm::cross(p1 - p0, p2 - p0));
        faceNormals[i] = n;
    }
    std::vector<glm::vec3> vertexNormals(positions.size(), glm::vec3(0.0f));
    std::vector<int> counts(positions.size(), 0);
    for (size_t i = 0; i < faces.size(); ++i) {
        auto f = faces[i];
        vertexNormals[f.x] += faceNormals[i];
        vertexNormals[f.y] += faceNormals[i];
        vertexNormals[f.z] += faceNormals[i];
        counts[f.x]++; counts[f.y]++; counts[f.z]++;


    }
    for (size_t i = 0; i < vertexNormals.size(); ++i) {
        if (counts[i] > 0) vertexNormals[i] = glm::normalize(vertexNormals[i] / (float)counts[i]);
    }

    // Build triangle list (positions + normals duplicated per triangle)
    std::vector<Vertex> vertices;
    vertices.reserve(faces.size() * 3);
    for (auto& f : faces) {
        vertices.push_back({ positions[f.x], vertexNormals[f.x] });
        vertices.push_back({ positions[f.y], vertexNormals[f.y] });
        vertices.push_back({ positions[f.z], vertexNormals[f.z] });
    }

    // find centroid & radius
    glm::vec3 centroid(0.0f);
    for (auto& p : positions) centroid += p;
    centroid /= (float)positions.size();
    float maxrad = 0.0f;
    for (auto& p : positions) maxrad = std::max(maxrad, glm::length(p - centroid));
    camRadius = maxrad * 2.5f;
    camAngle = 0.0f;

    // materials (3 distinct)
    std::vector<Material> materials;
    materials.push_back({
        glm::vec4(0.2,0.2,0.2,1.0),
        glm::vec4(0.8,0.2,0.2,1.0),
        glm::vec4(0.5,0.5,0.5,1.0),
        32.0f
        });
    // Provided special material from assignment (bright white specular)
    materials.push_back({
        glm::vec4(0.6,0.2,0.2,1.0),
        glm::vec4(0.9,0.1,0.1,1.0),
        glm::vec4(0.8,0.8,0.8,1.0),
        80.0f
        });
    materials.push_back({
        glm::vec4(0.1,0.1,0.3,1.0),
        glm::vec4(0.1,0.2,0.8,1.0),
        glm::vec4(0.2,0.2,0.9,1.0),
        16.0f
        });

    // lights
    Light light0; // object-space / world-space orbiting light
    light0.ambient = glm::vec4(0.2f);
    light0.diffuse = glm::vec4(0.6f);
    light0.specular = glm::vec4(1.0f);
    light0.inCameraSpace = false;

    Light light1; // camera-space light (near eye)
    light1.ambient = glm::vec4(0.1f);
    light1.diffuse = glm::vec4(0.6f);
    light1.specular = glm::vec4(1.0f);
    light1.inCameraSpace = true;

    // GLFW + GLAD init
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(1024, 768, "Part 2 - Shading", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cerr << "Failed to load GL\n"; return -1; }
    glfwSetKeyCallback(window, key_callback);
    print_controls();

    // Create shader programs
    GLuint progG = create_program(gouraud_vert, gouraud_frag);
    GLuint progP = create_program(phong_vert, phong_frag);

    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);

    // Uniform locations (we will query per program)
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        int w, h; glfwGetFramebufferSize(window, &w, &h);
        float aspect = (float)w / (float)h;
        glViewport(0, 0, w, h);
        glClearColor(0.08f, 0.08f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // model transform (center model at origin)
        glm::mat4 model = glm::translate(glm::mat4(1.0f), -centroid);

        // camera in world coords
        glm::vec3 camPos(camRadius * cos(camAngle), camRadius * sin(camAngle), camHeight);
        glm::mat4 view = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        glm::mat4 proj;
        if (perspectiveProj) proj = glm::perspective(glm::radians(45.0f), aspect, 0.01f, 100.0f);
        else {
            float s = maxrad * 2.0f;
            proj = glm::ortho(-s * aspect, s * aspect, -s, s, -100.0f, 100.0f);
        }

        // compute light0 position in world/object coordinates (orbiting around object centroid)
        glm::vec3 light0pos_world(lightRadius * cos(lightAngle), lightRadius * sin(lightAngle), lightHeight);
        light0.position = glm::vec3(light0pos_world);

        // light1 near eye (camera-space) - we want it in world coords so shaders use world positions
        glm::vec3 light1pos_world = camPos + glm::normalize(glm::vec3(0.0f) - camPos) * 0.1f + glm::vec3(0.0f, 0.0f, 0.1f);
        light1.position = light1pos_world;

        // select program
        GLuint activeProg = (shadingMode == 1) ? progG : progP;
        glUseProgram(activeProg);

        // set common uniforms depending on program
        if (activeProg == progG) {
            GLint loc_uModel = glGetUniformLocation(progG, "uModel");
            GLint loc_uView = glGetUniformLocation(progG, "uView");
            GLint loc_uProj = glGetUniformLocation(progG, "uProj");
            glUniformMatrix4fv(loc_uModel, 1, GL_FALSE, glm::value_ptr(model));
            glUniformMatrix4fv(loc_uView, 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(loc_uProj, 1, GL_FALSE, glm::value_ptr(proj));
            // material uniform
            Material& mat = materials[currentMaterial];
            GLint m_amb = glGetUniformLocation(progG, "material.ambient");
            GLint m_dif = glGetUniformLocation(progG, "material.diffuse");
            GLint m_spec = glGetUniformLocation(progG, "material.specular");
            GLint m_shi = glGetUniformLocation(progG, "material.shininess");
            glUniform4fv(m_amb, 1, glm::value_ptr(mat.ambient));
            glUniform4fv(m_dif, 1, glm::value_ptr(mat.diffuse));
            glUniform4fv(m_spec, 1, glm::value_ptr(mat.specular));
            glUniform1f(m_shi, mat.shininess);

            // light0
            GLint l0a = glGetUniformLocation(progG, "light0.ambient");
            GLint l0d = glGetUniformLocation(progG, "light0.diffuse");
            GLint l0s = glGetUniformLocation(progG, "light0.specular");
            GLint l0p = glGetUniformLocation(progG, "light0.position");
            GLint l0ic = glGetUniformLocation(progG, "light0.inCameraSpace");
            glUniform4fv(l0a, 1, glm::value_ptr(light0.ambient));
            glUniform4fv(l0d, 1, glm::value_ptr(light0.diffuse));
            glUniform4fv(l0s, 1, glm::value_ptr(light0.specular));
            glUniform3fv(l0p, 1, glm::value_ptr(light0.position));
            glUniform1i(l0ic, 0);

            // light1
            GLint l1a = glGetUniformLocation(progG, "light1.ambient");
            GLint l1d = glGetUniformLocation(progG, "light1.diffuse");
            GLint l1s = glGetUniformLocation(progG, "light1.specular");
            GLint l1p = glGetUniformLocation(progG, "light1.position");
            GLint l1ic = glGetUniformLocation(progG, "light1.inCameraSpace");
            glUniform4fv(l1a, 1, glm::value_ptr(light1.ambient));
            glUniform4fv(l1d, 1, glm::value_ptr(light1.diffuse));
            glUniform4fv(l1s, 1, glm::value_ptr(light1.specular));
            glUniform3fv(l1p, 1, glm::value_ptr(light1.position));
            glUniform1i(l1ic, 0); // we pass position in world coords here for simplicity

            GLint eyeLoc = glGetUniformLocation(progG, "eyePos");
            glUniform3fv(eyeLoc, 1, glm::value_ptr(camPos));
        }
        else {
            // Phong program
            GLint loc_uModel = glGetUniformLocation(progP, "uModel");
            GLint loc_uView = glGetUniformLocation(progP, "uView");
            GLint loc_uProj = glGetUniformLocation(progP, "uProj");
            glUniformMatrix4fv(loc_uModel, 1, GL_FALSE, glm::value_ptr(model));
            glUniformMatrix4fv(loc_uView, 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(loc_uProj, 1, GL_FALSE, glm::value_ptr(proj));

            Material& mat = materials[currentMaterial];
            GLint m_amb = glGetUniformLocation(progP, "material.ambient");
            GLint m_dif = glGetUniformLocation(progP, "material.diffuse");
            GLint m_spec = glGetUniformLocation(progP, "material.specular");
            GLint m_shi = glGetUniformLocation(progP, "material.shininess");
            glUniform4fv(m_amb, 1, glm::value_ptr(mat.ambient));
            glUniform4fv(m_dif, 1, glm::value_ptr(mat.diffuse));
            glUniform4fv(m_spec, 1, glm::value_ptr(mat.specular));
            glUniform1f(m_shi, mat.shininess);

            GLint l0a = glGetUniformLocation(progP, "light0.ambient");
            GLint l0d = glGetUniformLocation(progP, "light0.diffuse");
            GLint l0s = glGetUniformLocation(progP, "light0.specular");
            GLint l0p = glGetUniformLocation(progP, "light0.position");
            GLint l0ic = glGetUniformLocation(progP, "light0.inCameraSpace");
            glUniform4fv(l0a, 1, glm::value_ptr(light0.ambient));
            glUniform4fv(l0d, 1, glm::value_ptr(light0.diffuse));
            glUniform4fv(l0s, 1, glm::value_ptr(light0.specular));
            glUniform3fv(l0p, 1, glm::value_ptr(light0.position));
            glUniform1i(l0ic, 0);

            GLint l1a = glGetUniformLocation(progP, "light1.ambient");
            GLint l1d = glGetUniformLocation(progP, "light1.diffuse");
            GLint l1s = glGetUniformLocation(progP, "light1.specular");
            GLint l1p = glGetUniformLocation(progP, "light1.position");
            GLint l1ic = glGetUniformLocation(progP, "light1.inCameraSpace");
            glUniform4fv(l1a, 1, glm::value_ptr(light1.ambient));
            glUniform4fv(l1d, 1, glm::value_ptr(light1.diffuse));
            glUniform4fv(l1s, 1, glm::value_ptr(light1.specular));
            glUniform3fv(l1p, 1, glm::value_ptr(light1.position));
            glUniform1i(l1ic, 0);

            GLint eyeLoc = glGetUniformLocation(progP, "eyePos");
            glUniform3fv(eyeLoc, 1, glm::value_ptr(camPos));
        }

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertices.size());
        glBindVertexArray(0);

        glfwSwapBuffers(window);
    }

    glDeleteProgram(progG); glDeleteProgram(progP);
    glDeleteBuffers(1, &vbo); glDeleteVertexArrays(1, &vao);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
