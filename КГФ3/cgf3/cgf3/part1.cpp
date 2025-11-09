// part1_mod.cpp
// Build: g++ part1_mod.cpp glad.c -ldl -lglfw -lGL -I/path/to/glad/include -I/path/to/glm -o part1_mod
// Run:   ./part1_mod bound-bunny_200.smf

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
    glm::vec3 color;
};

// --- SMF model loading ---
bool loadSMF(const std::string& filename,
    std::vector<glm::vec3>& out_positions,
    std::vector<glm::uvec3>& out_faces)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Cannot open file: " << filename << "\n";
        return false;
    }

    std::vector<glm::vec3> vertices;
    std::vector<glm::uvec3> faces;
    std::string line;

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        char type;
        iss >> type;

        if (type == 'v') {
            float x, y, z;
            iss >> x >> y >> z;
            vertices.emplace_back(x, y, z);
        }
        else if (type == 'f') {
            unsigned int a, b, c;
            iss >> a >> b >> c;
            faces.emplace_back(a - 1, b - 1, c - 1);
        }
    }

    out_positions = std::move(vertices);
    out_faces = std::move(faces);
    return true;
}

// --- Shader utilities ---
GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "Shader compilation failed:\n" << log << "\n";
    }
    return shader;
}

GLuint createProgram(const char* vsrc, const char* fsrc) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vsrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsrc);

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        char info[512];
        glGetProgramInfoLog(program, sizeof(info), nullptr, info);
        std::cerr << "Shader linking failed:\n" << info << "\n";
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

// --- Shaders ---
static const char* vertexShaderSrc = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aColor;

uniform mat4 uMVP;
uniform mat4 uModel;

out vec3 vColor;
out vec3 vNormal;

void main() {
    vColor = aColor;
    vNormal = mat3(transpose(inverse(uModel))) * aNormal;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char* fragmentShaderSrc = R"(
#version 330 core
in vec3 vColor;
out vec4 FragColor;

void main() {
    FragColor = vec4(abs(vColor), 1.0);
}
)";

// --- Camera control ---
float cameraTheta = 0.0f;
float cameraRadius = 2.5f;
float cameraHeight = 0.5f;
bool usePerspective = true;

void onKey(GLFWwindow* window, int key, int, int action, int) {
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        switch (key) {
        case GLFW_KEY_A: cameraTheta -= 0.05f; break;
        case GLFW_KEY_D: cameraTheta += 0.05f; break;
        case GLFW_KEY_W: cameraRadius -= 0.05f; break;
        case GLFW_KEY_S: cameraRadius += 0.05f; break;
        case GLFW_KEY_Q: cameraHeight += 0.05f; break;
        case GLFW_KEY_E: cameraHeight -= 0.05f; break;
        case GLFW_KEY_P: usePerspective = !usePerspective; break;
        case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(window, GLFW_TRUE); break;
        default: break;
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " model.smf\n";
        return -1;
    }

    std::string filename = argv[1];
    std::vector<glm::vec3> positions;
    std::vector<glm::uvec3> faces;

    if (!loadSMF(filename, positions, faces)) return -1;

    // centroid
    glm::vec3 centroid(0.0f);
    for (auto& p : positions) centroid += p;
    centroid /= static_cast<float>(positions.size());

    // vertex data
    std::vector<Vertex> vertices;
    vertices.reserve(faces.size() * 3);

    for (auto& f : faces) {
        glm::vec3 p0 = positions[f.x];
        glm::vec3 p1 = positions[f.y];
        glm::vec3 p2 = positions[f.z];

        glm::vec3 normal = glm::normalize(glm::cross(p1 - p0, p2 - p0));
        glm::vec3 color = glm::abs(normal);

        vertices.push_back({ p0, normal, color });
        vertices.push_back({ p1, normal, color });
        vertices.push_back({ p2, normal, color });
    }

    // init GLFW + GLAD
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1024, 768, "Flat Shading Viewer", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to init GLAD\n";
        return -1;
    }

    glfwSetKeyCallback(window, onKey);
    GLuint program = createProgram(vertexShaderSrc, fragmentShaderSrc);

    // buffers
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);

    // initial camera
    float maxRadius = 0.0f;
    for (auto& p : positions)
        maxRadius = std::max(maxRadius, glm::length(p - centroid));

    cameraRadius = maxRadius * 2.0f;

    GLint uMVP = glGetUniformLocation(program, "uMVP");
    GLint uModel = glGetUniformLocation(program, "uModel");

    glm::mat4 model = glm::translate(glm::mat4(1.0f), -centroid);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        float aspect = (float)width / (float)height;

        glViewport(0, 0, width, height);
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::vec3 camPos(cameraRadius * cos(cameraTheta),
            cameraRadius * sin(cameraTheta),
            cameraHeight);

        glm::mat4 view = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

        glm::mat4 proj;
        if (usePerspective)
            proj = glm::perspective(glm::radians(45.0f), aspect, 0.01f, 100.0f);
        else {
            float s = maxRadius * 2.0f;
            proj = glm::ortho(-s * aspect, s * aspect, -s, s, -100.0f, 100.0f);
        }

        glm::mat4 mvp = proj * view * model;

        glUseProgram(program);
        glUniformMatrix4fv(uMVP, 1, GL_FALSE, glm::value_ptr(mvp));
        glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(model));

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertices.size());
        glBindVertexArray(0);

        glfwSwapBuffers(window);
    }

    // cleanup
    glDeleteProgram(program);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
