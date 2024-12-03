#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>

FT_Library ft;
FT_Face face;
GLuint textVAO, textVBO;
std::map<char, FT_GlyphSlot> glyphCache;

float vibrationIntensity = 0.0f;
float angle = 0.1f;
bool isRadioOn = false;
float lightColor[3] = { 0.5f, 0.5f, 0.5f };
float lightTimer = 0.0f;
bool isLightWhite = true;
std::string currentText = "AM";

GLuint circleVAO, circleVBO;
GLuint rectVAO, rectVBO;
GLuint gridVAO, gridVBO;
GLuint shaderProgram;
GLuint textShaderProgram;

struct Character {
    GLuint TextureID;
    int Width, Height;
    int BearingX, BearingY;
    GLuint Advance;
};

std::map<GLchar, Character> Characters;

const char* vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec2 aPos;
    uniform vec2 position;
    uniform vec2 scale;
    void main() {
        gl_Position = vec4(aPos * scale + position, 0.0, 1.0);
    }
)";

const char* fragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor;
    uniform vec3 color;
    void main() {
        FragColor = vec4(color, 1.0);
    }
)";

const char* textVertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec4 vertex;
    out vec2 TexCoords;
    uniform mat4 projection;
    void main() {
        gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);
        TexCoords = vertex.zw;
    }
)";

const char* textFragmentShaderSource = R"(
    #version 330 core
    in vec2 TexCoords;
    out vec4 color;
    uniform sampler2D text;
    uniform vec3 textColor;
    void main() {
        vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);
        color = vec4(textColor, 1.0) * sampled;
    }
)";

void compileShaders() {
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void compileTextShaders() {
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &textVertexShaderSource, NULL);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &textFragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    textShaderProgram = glCreateProgram();
    glAttachShader(textShaderProgram, vertexShader);
    glAttachShader(textShaderProgram, fragmentShader);
    glLinkProgram(textShaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void createCircleVAO(float radius, int segments) {
    std::vector<float> vertices;
    vertices.push_back(0.0f);
    vertices.push_back(0.0f);

    for (int i = 0; i <= segments; ++i) {
        float theta = 2.0f * 3.14 * float(i) / float(segments);
        vertices.push_back(radius * cosf(theta));
        vertices.push_back(radius * sinf(theta));
    }

    glGenVertexArrays(1, &circleVAO);
    glGenBuffers(1, &circleVBO);

    glBindVertexArray(circleVAO);

    glBindBuffer(GL_ARRAY_BUFFER, circleVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void createRectVAO() {
    float vertices[] = {
        -0.5f, -0.5f,
         0.5f, -0.5f,
         0.5f,  0.5f,
        -0.5f,  0.5f
    };

    glGenVertexArrays(1, &rectVAO);
    glGenBuffers(1, &rectVBO);

    glBindVertexArray(rectVAO);

    glBindBuffer(GL_ARRAY_BUFFER, rectVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void createGridVAO(int lines) {
    std::vector<float> vertices;

    float maxRadius = 0.2f;

    for (int i = 1; i <= lines; ++i) {
        float normalizedRadius = (float(i) / lines) * maxRadius;
        for (int j = 0; j <= 50; ++j) {
            float theta = 2.0f * 3.14 * float(j) / 50;
            vertices.push_back(normalizedRadius * cosf(theta));
            vertices.push_back(normalizedRadius * sinf(theta));
        }
    }

    for (int i = 0; i < lines; ++i) {
        float theta = 2.0f * 3.14 * float(i) / lines;
        vertices.push_back(0.0f);
        vertices.push_back(0.0f);
        vertices.push_back(maxRadius * cosf(theta));
        vertices.push_back(maxRadius * sinf(theta));
    }

    glGenVertexArrays(1, &gridVAO);
    glGenBuffers(1, &gridVBO);

    glBindVertexArray(gridVAO);

    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void drawCircle(float x, float y, float scale, float r, float g, float b) {
    glUseProgram(shaderProgram);
    glUniform3f(glGetUniformLocation(shaderProgram, "color"), r, g, b);
    glUniform2f(glGetUniformLocation(shaderProgram, "position"), x, y);
    glUniform2f(glGetUniformLocation(shaderProgram, "scale"), scale, scale);

    glBindVertexArray(circleVAO);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 52);
    glBindVertexArray(0);
}

void drawRect(float x, float y, float scaleX, float scaleY, float r, float g, float b) {
    glUseProgram(shaderProgram);
    glUniform3f(glGetUniformLocation(shaderProgram, "color"), r, g, b);
    glUniform2f(glGetUniformLocation(shaderProgram, "position"), x, y);
    glUniform2f(glGetUniformLocation(shaderProgram, "scale"), scaleX, scaleY);

    glBindVertexArray(rectVAO);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindVertexArray(0);
}

void drawGrid(float x, float y, float scale, float r, float g, float b) {
    glUseProgram(shaderProgram);

    glUniform3f(glGetUniformLocation(shaderProgram, "color"), r, g, b);
    glUniform2f(glGetUniformLocation(shaderProgram, "position"), x, y);
    glUniform1f(glGetUniformLocation(shaderProgram, "scale"), 0.0f);

    glBindVertexArray(gridVAO);

    for (int i = 0; i < 10; ++i) {
        glDrawArrays(GL_LINE_LOOP, i * 51, 51);
    }

    glDrawArrays(GL_LINES, 10 * 51, 20);

    glBindVertexArray(0);
}

void updateLightColor() {
    if (isRadioOn) {
        lightTimer += 0.016f;
        if (lightTimer >= 0.3f) {
            lightTimer = 0.0f;
            isLightWhite = !isLightWhite;
        }

        if (isLightWhite) {
            lightColor[0] = 1.0f;
            lightColor[1] = 1.0f;
            lightColor[2] = 1.0f;
        }
        else {
            lightColor[0] = 1.0f;
            lightColor[1] = 0.5f;
            lightColor[2] = 0.0f;
        }
    }
    else {
        lightColor[0] = 0.5f;
        lightColor[1] = 0.5f;
        lightColor[2] = 0.5f;
    }
}

void updatePulsing() {
    if (isRadioOn) {
        angle += 0.7f;
        if (angle > 2.0f * 3.14f) {
            angle -= 2.0f * 3.14f;
        }
    }
    else {
        angle = 0.0f;
    }
}


void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        int width, height;
        glfwGetWindowSize(window, &width, &height);

        float ndcX = (xpos / width) * 2.0f - 1.0f;
        float ndcY = 1.0f - (ypos / height) * 2.0f;

        float buttonLeft = -0.15f;
        float buttonRight = 0.15f;
        float buttonBottom = -0.6f - 0.1f;
        float buttonTop = -0.6f + 0.1f;


        if (ndcX >= buttonLeft && ndcX <= buttonRight && ndcY >= buttonBottom && ndcY <= buttonTop) {
            isRadioOn = !isRadioOn;
        }
    }
}


float sliderValue = 0.0f;
float sliderX = 0.0f;
bool isSliderDragging = false;

float sliderMinX = -0.8f;
float sliderMaxX = -0.4f;
float sliderY = -0.7f;

void drawSlider(float minX, float maxX, float y, float value) {
    drawRect((minX + maxX) / 2.0f, y, (maxX - minX), 0.05f, 0.7f, 0.7f, 0.7f);

    float handleX = minX + value * (maxX - minX);
    drawRect(handleX, y, 0.05f, 0.15f, 0.0f, 0.0f, 0.0f);
}

void handleMouseSlider(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        int width, height;
        glfwGetWindowSize(window, &width, &height);
        float ndcX = (xpos / width) * 2.0f - 1.0f;
        float ndcY = 1.0f - (ypos / height) * 2.0f;

        float handleX = sliderMinX + sliderValue * (sliderMaxX - sliderMinX);
        if (action == GLFW_PRESS &&
            ndcX >= handleX - 0.05f && ndcX <= handleX + 0.05f &&
            ndcY >= sliderY - 0.075f && ndcY <= sliderY + 0.075f) {
            isSliderDragging = true;
        }
        else if (action == GLFW_RELEASE) {
            isSliderDragging = false;
        }
    }
}

void handleSliderDrag(GLFWwindow* window) {
    if (isSliderDragging) {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        int width, height;
        glfwGetWindowSize(window, &width, &height);


        float ndcX = (xpos / width) * 2.0f - 1.0f;


        sliderValue = (ndcX - sliderMinX) / (sliderMaxX - sliderMinX);
        sliderValue = std::fmax(0.0f, std::fmin(1.0f, sliderValue));

        float minValue = 0.0f;
        float maxValue = 1.0f;
        float mappedValue = minValue + sliderValue * (maxValue - minValue);

        vibrationIntensity = mappedValue;
    }
}

void mouseSwitchCallback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);

        int width, height;
        glfwGetWindowSize(window, &width, &height);
        mouseX = (mouseX / width) * 2.0f - 1.0f;
        mouseY = 1.0f - (mouseY / height) * 2.0f;


        float rectLeft = -0.1f;
        float rectRight = 0.1f;
        float rectBottom = 0.3f;
        float rectTop = 0.5f;

        if (mouseX >= rectLeft && mouseX <= rectRight && mouseY >= rectBottom && mouseY <= rectTop) {
            currentText = (currentText == "AM") ? "FM" : "AM";
        }
    }
}

bool initializeFreeType(const std::string& fontPath) {
    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        std::cerr << "Could not initialize FreeType library" << std::endl;
        return false;
    }

    FT_Face face;
    if (FT_New_Face(ft, fontPath.c_str(), 0, &face)) {
        std::cerr << "Failed to load font: " << fontPath << std::endl;
        FT_Done_FreeType(ft);
        return false;
    }

    FT_Set_Pixel_Sizes(face, 0, 48);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (unsigned char c = 0; c < 128; ++c) {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
            std::cerr << "Failed to load glyph: " << c << std::endl;
            continue;
        }

        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RED,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer
        );

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        Character character = {
            texture,
            static_cast<int>(face->glyph->bitmap.width),
            static_cast<int>(face->glyph->bitmap.rows),
            static_cast<int>(face->glyph->bitmap_left),
            static_cast<int>(face->glyph->bitmap_top),
            static_cast<unsigned int>(face->glyph->advance.x)
        };
        Characters.insert(std::make_pair(c, character));

        glBindTexture(GL_TEXTURE_2D, character.TextureID);
    }


    FT_Done_Face(face);
    FT_Done_FreeType(ft);
    return true;
}

void initializeTextRendering() {
    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &textVBO);

    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}


void renderText(const std::string& text, float x, float y, float scale, float r, float g, float b) {
    glUseProgram(textShaderProgram);
    glUniform3f(glGetUniformLocation(textShaderProgram, "textColor"), r, g, b);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(glGetUniformLocation(textShaderProgram, "text"), 0);

    glBindVertexArray(textVAO);

    for (char c : text) {
        Character ch = Characters[c];

        float xpos = x + ch.BearingX * scale;
        float ypos = y - (ch.Height - ch.BearingY) * scale;
        float w = ch.Width * scale;
        float h = ch.Height * scale;

        float vertices[6][4] = {
            { xpos,     ypos + h, 0.0f, 0.0f },
            { xpos,     ypos,     0.0f, 1.0f },
            { xpos + w, ypos,     1.0f, 1.0f },

            { xpos,     ypos + h, 0.0f, 0.0f },
            { xpos + w, ypos,     1.0f, 1.0f },
            { xpos + w, ypos + h, 1.0f, 0.0f }
        };

        glBindTexture(GL_TEXTURE_2D, ch.TextureID);
        glBindBuffer(GL_ARRAY_BUFFER, textVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        x += (ch.Advance >> 6) * scale;
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}


void renderAMFM(const std::string& currentText) {

    drawRect(0.0f, 0.4f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f);


    glUseProgram(textShaderProgram);
    glm::mat4 projection = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f);
    glUniformMatrix4fv(glGetUniformLocation(textShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    renderText(currentText, -0.05f, 0.42f, 0.0015f, 1.0f, 1.0f, 1.0f);
}

#include <chrono>
#include <thread>


const int TARGET_FPS = 60;
const double FRAME_TIME = 1.0 / TARGET_FPS;

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    GLFWwindow* window = glfwCreateWindow(800, 800, "Radio Interface", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    compileShaders();
    compileTextShaders();
    initializeTextRendering();
    createCircleVAO(0.2f, 50);
    createRectVAO();
    createGridVAO(10);

    if (!initializeFreeType("C:\\Users\\Aleksa\\Desktop\\grafika\\RadioProject\\LiberationSans-Regular.ttf")) {
        return -1;
    }

    glm::mat4 projection = glm::ortho(0.0f, 800.0f, 0.0f, 600.0f);
    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    glfwSetMouseButtonCallback(window, [](GLFWwindow* win, int btn, int action, int mods) {
        mouseButtonCallback(win, btn, action, mods);
        handleMouseSlider(win, btn, action, mods);
        mouseSwitchCallback(win, btn, action, mods);
        });

    while (!glfwWindowShouldClose(window)) {
        auto startTime = std::chrono::high_resolution_clock::now();

        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        updatePulsing();
        updateLightColor();
        handleSliderDrag(window);

        float pulsingScale = isRadioOn ? 1.0f + sin(angle) * vibrationIntensity : 1.0f;

        drawRect(0.0f, 0.0f, 1.8f, 1.6f, 0.0f, 0.0f, 1.0f);
        drawSlider(sliderMinX, sliderMaxX, sliderY, sliderValue);
        drawRect(-0.6f, 0.0f, 0.45f, 0.55f, 0.3f, 0.3f, 0.3f);
        drawCircle(-0.6f, 0.0f, 0.8f * pulsingScale, 0.0f, 0.0f, 0.0f);
        drawGrid(-0.6f, 0.0f, 0.0, 0.5f, 0.5f, 0.5f);
        drawRect(0.6f, 0.0f, 0.45f, 0.55f, 0.3f, 0.3f, 0.3f);
        drawCircle(0.6f, 0.0f, 0.8f * pulsingScale, 0.0f, 0.0f, 0.0f);
        drawGrid(0.6f, 0.0f, 0.1, 0.5f, 0.5f, 0.5f);
        drawCircle(0.0f, 0.6f, 0.17f, lightColor[0], lightColor[1], lightColor[2]);
        renderAMFM(currentText);

        if (!isRadioOn) {
            drawRect(0.0f, -0.6f, 0.3f, 0.2f, 0.0f, 0.8f, 0.0f);
        }
        else {
            drawRect(0.0f, -0.6f, 0.3f, 0.2f, 0.8f, 0.0f, 0.0f);
        }

        renderText(u8"Aleksa Janjic SV58/2021", -0.9f, 0.83f, 0.002f, 1.0f, 1.0f, 1.0f);
        
        glfwSwapBuffers(window);
        glfwPollEvents();

        auto endTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = endTime - startTime;

        double sleepTime = FRAME_TIME - elapsed.count();
        if (sleepTime > 0) {
            std::this_thread::sleep_for(std::chrono::duration<double>(sleepTime));
        }
    }

    glfwTerminate();
    return 0;
}


