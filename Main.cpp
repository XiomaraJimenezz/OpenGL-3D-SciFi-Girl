// PROYECTO: Exhibicion 3D - SciFi Girl
// OpenGL 3.3 | C++ | GLFW | GLAD | GLM | TinyGLTF
//
// El programa carga un modelo GLB y permite observarlo con
// iluminacion Phong, rotacion y una camara que sigue una curva
// de Bezier. Los controles permiten cambiar entre modos.
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <glad/glad.h>
#include "Shader.h"
#include <GLFW/glfw3.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

// STB se implementa una sola vez. TinyGLTF reutiliza esa implementacion.
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tinygltf/tiny_gltf.h>

const std::string RUTA_MODELO = "models/scifi_girl_v.01.glb";
const int ANCHO = 900, ALTO = 750;
const float PI = 3.14159265358979323846f; // Constante de referencia.

// 1. DATOS DEL MODELO
// Guardamos los vertices, los indices y el material de cada parte.
struct Vertice { glm::vec3 posicion, normal; glm::vec2 uv; glm::vec4 color = glm::vec4(1.0f); };
struct Parte { unsigned int inicio, cantidad; int material; };
struct Escena { std::vector<Vertice> vertices; std::vector<unsigned int> indices; std::vector<Parte> partes; };

// Lectura segura para buffers intercalados y accessors con offset.
// 2. LECTURA DEL ARCHIVO GLB
// Se leen los datos de los buffers y se reconstruyen las mallas.
const unsigned char* datosAccessor(const tinygltf::Model& m, const tinygltf::Accessor& a, size_t& paso) {
    if (a.bufferView < 0 || a.bufferView >= (int)m.bufferViews.size()) return nullptr;
    const auto& v = m.bufferViews[a.bufferView];
    if (v.buffer < 0 || v.buffer >= (int)m.buffers.size()) return nullptr;
    paso = a.ByteStride(v);
    const auto& b = m.buffers[v.buffer];
    if (v.byteOffset + a.byteOffset >= b.data.size()) return nullptr;
    return b.data.data() + v.byteOffset + a.byteOffset;
}

unsigned int leerIndice(const unsigned char* p, int tipo) {
    if (tipo == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) { uint8_t x; std::memcpy(&x, p, 1); return x; }
    if (tipo == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) { uint16_t x; std::memcpy(&x, p, 2); return x; }
    if (tipo == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) { uint32_t x; std::memcpy(&x, p, 4); return x; }
    return 0;
}

// Transforma las mallas segun la jerarquia de nodos del GLB.
// No aplica skinning ni animaciones esqueleticas.
glm::mat4 matrizNodo(const tinygltf::Node& n) {
    if (n.matrix.size() == 16) {
        glm::mat4 M(1.0f);
        for (int c = 0;c < 4;c++) for (int r = 0;r < 4;r++) M[c][r] = (float)n.matrix[c * 4 + r];
        return M;
    }
    glm::mat4 M(1.0f);
    if (n.translation.size() == 3) M = glm::translate(M, glm::vec3(n.translation[0], n.translation[1], n.translation[2]));
    if (n.rotation.size() == 4) {
        glm::quat q((float)n.rotation[3], (float)n.rotation[0], (float)n.rotation[1], (float)n.rotation[2]);
        M *= glm::mat4_cast(q);
    }
    if (n.scale.size() == 3) M = glm::scale(M, glm::vec3(n.scale[0], n.scale[1], n.scale[2]));
    return M;
}

void agregarMalla(const tinygltf::Model& m, int meshID, const glm::mat4& M, Escena& e) {
    if (meshID < 0 || meshID >= (int)m.meshes.size()) return;
    for (const auto& p : m.meshes[meshID].primitives) {
        if (p.mode != -1 && p.mode != TINYGLTF_MODE_TRIANGLES) continue;
        auto it = p.attributes.find("POSITION");
        if (it == p.attributes.end()) continue;
        const auto& a = m.accessors[it->second];
        if (a.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || a.type != TINYGLTF_TYPE_VEC3) continue;
        size_t stride = 0; const unsigned char* pos = datosAccessor(m, a, stride);
        if (!pos) continue;
        const tinygltf::Accessor* na = nullptr; const unsigned char* normals = nullptr; size_t ns = 0;
        auto ni = p.attributes.find("NORMAL");
        if (ni != p.attributes.end()) { na = &m.accessors[ni->second]; if (na->componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && na->type == TINYGLTF_TYPE_VEC3) normals = datosAccessor(m, *na, ns); }
        const tinygltf::Accessor* ta = nullptr; const unsigned char* tex = nullptr; size_t ts = 0;
        auto ti = p.attributes.find("TEXCOORD_0");
        if (ti != p.attributes.end()) { ta = &m.accessors[ti->second]; if (ta->componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && ta->type == TINYGLTF_TYPE_VEC2) tex = datosAccessor(m, *ta, ts); }
        const tinygltf::Accessor* ca = nullptr; const unsigned char* colores = nullptr; size_t cs = 0;
        auto ci = p.attributes.find("COLOR_0");
        if (ci != p.attributes.end()) {
            ca = &m.accessors[ci->second];
            if ((ca->type == TINYGLTF_TYPE_VEC3 || ca->type == TINYGLTF_TYPE_VEC4) &&
                (ca->componentType == TINYGLTF_COMPONENT_TYPE_FLOAT ||
                    ca->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE ||
                    ca->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT))
                colores = datosAccessor(m, *ca, cs);
        }
        unsigned int base = (unsigned int)e.vertices.size();
        glm::mat3 normalM = glm::transpose(glm::inverse(glm::mat3(M)));
        for (size_t i = 0;i < a.count;i++) {
            float xyz[3]; std::memcpy(xyz, pos + i * stride, sizeof(xyz));
            Vertice v{}; v.posicion = glm::vec3(M * glm::vec4(xyz[0], xyz[1], xyz[2], 1));
            v.normal = glm::vec3(0);
            if (normals && i < na->count) { float n[3]; std::memcpy(n, normals + i * ns, sizeof(n)); v.normal = glm::normalize(normalM * glm::vec3(n[0], n[1], n[2])); }
            if (tex && i < ta->count) { float uv[2]; std::memcpy(uv, tex + i * ts, sizeof(uv)); v.uv = glm::vec2(uv[0], uv[1]); }
            if (colores && i < ca->count) {
                const unsigned char* cp = colores + i * cs;
                int componentes = ca->type == TINYGLTF_TYPE_VEC4 ? 4 : 3;
                for (int c = 0; c < componentes; ++c) {
                    float valor = 1.0f;
                    if (ca->componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) std::memcpy(&valor, cp + c * 4, 4);
                    else if (ca->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) valor = cp[c] / 255.0f;
                    else { uint16_t n; std::memcpy(&n, cp + c * 2, 2); valor = n / 65535.0f; }
                    v.color[c] = valor;
                }
            }
            e.vertices.push_back(v);
        }
        unsigned int start = (unsigned int)e.indices.size();
        if (p.indices >= 0) {
            const auto& ia = m.accessors[p.indices]; size_t is = 0;
            const unsigned char* ids = datosAccessor(m, ia, is);
            if (ids) for (size_t i = 0;i < ia.count;i++) e.indices.push_back(base + leerIndice(ids + i * is, ia.componentType));
        }
        else for (size_t i = 0;i < a.count;i++) e.indices.push_back(base + (unsigned int)i);
        // Calcular normales de respaldo si no estaban presentes.
        if (!normals) for (size_t j = start;j + 2 < e.indices.size();j += 3) {
            unsigned int i0 = e.indices[j], i1 = e.indices[j + 1], i2 = e.indices[j + 2];
            glm::vec3 n = glm::cross(e.vertices[i1].posicion - e.vertices[i0].posicion, e.vertices[i2].posicion - e.vertices[i0].posicion);
            if (glm::length(n) > 1e-8f) { n = glm::normalize(n); e.vertices[i0].normal += n; e.vertices[i1].normal += n; e.vertices[i2].normal += n; }
        }
        e.partes.push_back({ start,(unsigned int)e.indices.size() - start,p.material });
    }
}
void visitarNodo(const tinygltf::Model& m, int id, const glm::mat4& padre, Escena& e, int profundidad = 0) {
    if (id < 0 || id >= (int)m.nodes.size() || profundidad>100) return;
    const auto& n = m.nodes[id]; glm::mat4 mundo = padre * matrizNodo(n);
    if (n.mesh >= 0) agregarMalla(m, n.mesh, mundo, e);
    for (int hijo : n.children) visitarNodo(m, hijo, mundo, e, profundidad + 1);
}
Escena construirEscena(const tinygltf::Model& m) {
    Escena e;
    int s = m.defaultScene >= 0 ? m.defaultScene : (!m.scenes.empty() ? 0 : -1);
    if (s >= 0) for (int n : m.scenes[s].nodes) visitarNodo(m, n, glm::mat4(1), e);
    else for (size_t i = 0;i < m.meshes.size();i++) agregarMalla(m, (int)i, glm::mat4(1), e);
    for (auto& v : e.vertices) if (glm::length(v.normal) > 1e-6f) v.normal = glm::normalize(v.normal); else v.normal = glm::vec3(0, 1, 0);
    return e;
}

// 3. TEXTURAS
// Convertimos las imagenes del GLB en texturas para OpenGL.
GLuint texturaImagen(const tinygltf::Image& img) {
    if (img.image.empty() || img.width <= 0 || img.height <= 0 || img.bits != 8 || img.component < 1 || img.component>4) return 0;
    GLenum formato = img.component == 1 ? GL_RED : img.component == 2 ? GL_RG : img.component == 3 ? GL_RGB : GL_RGBA;
    GLuint t = 0; glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    // sRGB para las texturas de color; mantiene mejor sus tonos originales.
    GLenum interno = formato == GL_RGB ? GL_SRGB8 : formato == GL_RGBA ? GL_SRGB8_ALPHA8 : formato;
    glTexImage2D(GL_TEXTURE_2D, 0, interno, img.width, img.height, 0, formato, GL_UNSIGNED_BYTE, img.image.data());
    glGenerateMipmap(GL_TEXTURE_2D); return t;
}


// 4. SHADERS
// Vertex shader: transforma vertices y envia normales y UV.
// Fragment shader: combina textura, color e iluminacion Phong.
// Los shaders LINE dibujan la trayectoria de la camara.
const char* VS = R"GLSL(#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
layout(location=3) in vec4 aColor;
uniform mat4 model,view,projection;
out vec3 FragPos,Normal;
out vec2 UV;
out vec4 ColorVertice;
void main(){vec4 world=model*vec4(aPos,1.0);FragPos=world.xyz;Normal=mat3(transpose(inverse(model)))*aNormal;UV=aUV;ColorVertice=aColor;gl_Position=projection*view*world;}
)GLSL";
const char* FS = R"GLSL(#version 330 core
in vec3 FragPos,Normal;in vec2 UV;in vec4 ColorVertice;out vec4 FragColor;
uniform sampler2D tex;
uniform vec4 baseColor;
uniform vec3 lightPos,viewPos,lightColor;
uniform bool usarPhong;
void main(){
 vec4 base=texture(tex,UV)*baseColor*ColorVertice;
 if(base.a<0.08) discard;
 if(!usarPhong){FragColor=base;return;}
 vec3 n=normalize(Normal),l=normalize(lightPos-FragPos),v=normalize(viewPos-FragPos);
 float dif=max(dot(n,l),0.0);
 vec3 h=normalize(l+v);
 float esp=pow(max(dot(n,h),0.0),24.0)*step(0.001,dif);
 vec3 color=(0.34+0.68*dif)*base.rgb*lightColor+0.16*esp*lightColor;
 FragColor=vec4(color,base.a);
}
)GLSL";
const char* LINE_VS = R"GLSL(#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 view,projection;
void main(){gl_Position=projection*view*vec4(aPos,1.0);}
)GLSL";
const char* LINE_FS = R"GLSL(#version 330 core
out vec4 FragColor;uniform vec3 color;
void main(){FragColor=vec4(color,1.0);}
)GLSL";

// 5. TRAYECTORIA DE BEZIER
// Cada tramo usa cuatro puntos de control. Los cuatro tramos
// forman una trayectoria cerrada para mover la camara.
// Curva Bezier cubica. Cuatro tramos conectados forman un circuito cerrado.
glm::vec3 bezier(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d, float t) {
    float s = 1 - t;return s * s * s * a + 3 * s * s * t * b + 3 * s * t * t * c + t * t * t * d;
}
glm::vec3 trayectoria(float t) {
    t = t - std::floor(t);float x = t * 4;int tramo = std::min(3, (int)x);float u = x - tramo;
    // Aproximacion Bezier de una orbita eliptica en el plano XZ.
    const float k = 0.55228475f, rx = 3.2f, rz = 2.5f, y = 1.15f;
    glm::vec3 a, b, c, d;
    if (tramo == 0) { a = { rx,y,0 };b = { rx,y,k * rz };c = { k * rx,y,rz };d = { 0,y,rz }; }
    else if (tramo == 1) { a = { 0,y,rz };b = { -k * rx,y,rz };c = { -rx,y,k * rz };d = { -rx,y,0 }; }
    else if (tramo == 2) { a = { -rx,y,0 };b = { -rx,y,-k * rz };c = { -k * rx,y,-rz };d = { 0,y,-rz }; }
    else { a = { 0,y,-rz };b = { k * rx,y,-rz };c = { rx,y,-k * rz };d = { rx,y,0 }; }
    return bezier(a, b, c, d, u);
}

// 6. ENTRADAS DEL TECLADO
// Detectamos cuando una tecla acaba de pulsarse para evitar
// que un modo se active y desactive varias veces seguidas.
// Controles por pulsacion, no por repeticion de fotogramas.
// Controles por pulsacion, no por repeticion de fotogramas.
bool teclaNueva(GLFWwindow* w, int key, bool& anterior) { bool actual = glfwGetKey(w, key) == GLFW_PRESS;bool nuevo = actual && !anterior;anterior = actual;return nuevo; }

// 7. PROGRAMA PRINCIPAL
// Cargamos el GLB, preparamos OpenGL y ejecutamos la escena.
int main() {
    // Cargar el archivo GLB y comprobar que tenga geometria.
    tinygltf::TinyGLTF loader;tinygltf::Model gltf;std::string err, warn;
    if (!loader.LoadBinaryFromFile(&gltf, &err, &warn, RUTA_MODELO)) {
        std::cerr << "No se pudo abrir " << RUTA_MODELO << "\n" << err << "\n";
        std::cerr << "Revisa el nombre exacto y el directorio de trabajo de Visual Studio.\n";return -1;
    }
    if (!warn.empty())std::cout << warn << '\n';
    Escena e = construirEscena(gltf);
    if (e.vertices.empty() || e.indices.empty()) { std::cerr << "GLB sin triangulos compatibles.\n";return -1; }
    std::cout << "SciFi: " << e.vertices.size() << " vertices, " << e.partes.size() << " partes\n";
    glm::vec3 minP = e.vertices[0].posicion, maxP = minP;
    for (const auto& v : e.vertices) { minP = glm::min(minP, v.posicion);maxP = glm::max(maxP, v.posicion); }
    float escala = 2.6f / std::max(0.001f, std::max(maxP.x - minP.x, std::max(maxP.y - minP.y, maxP.z - minP.z)));
    glm::vec3 centro = (minP + maxP) * 0.5f;
    // Colocar los pies aproximadamente sobre y=0.
    glm::mat4 normalizar = glm::scale(glm::mat4(1), glm::vec3(escala)) * glm::translate(glm::mat4(1), glm::vec3(-centro.x, -minP.y, -centro.z));

    // Crear la ventana e inicializar OpenGL.
    if (!glfwInit()) { std::cerr << "Error GLFW\n";return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* w = glfwCreateWindow(ANCHO, ALTO, "SciFi Girl - Phong y Camara Bezier", nullptr, nullptr);
    if (!w) { glfwTerminate();return -1; }
    glfwMakeContextCurrent(w);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cerr << "Error GLAD\n";glfwDestroyWindow(w);glfwTerminate();return -1; }
    glfwSwapInterval(1);glEnable(GL_DEPTH_TEST);
    { // Los objetos Shader deben destruirse antes de cerrar el contexto OpenGL.
        Shader shaderModelo(VS, FS);
        Shader shaderCurva(LINE_VS, LINE_FS);
        if (!shaderModelo.valido() || !shaderCurva.valido()) {
            std::cerr << "No se pudieron crear los shaders.\n";
            return -1;
        }
        // Enviar vertices e indices a la GPU mediante VAO, VBO y EBO.
        GLuint vao, vbo, ebo;glGenVertexArrays(1, &vao);glGenBuffers(1, &vbo);glGenBuffers(1, &ebo);
        glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, e.vertices.size() * sizeof(Vertice), e.vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, e.indices.size() * sizeof(unsigned int), e.indices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertice), (void*)offsetof(Vertice, posicion));glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertice), (void*)offsetof(Vertice, normal));glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertice), (void*)offsetof(Vertice, uv));glEnableVertexAttribArray(2);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertice), (void*)offsetof(Vertice, color));glEnableVertexAttribArray(3);
        glBindVertexArray(0);

        // Textura blanca de respaldo para materiales sin imagen.
        GLuint blanco;glGenTextures(1, &blanco);glBindTexture(GL_TEXTURE_2D, blanco);
        const unsigned char pix[] = { 255,255,255,255 };glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pix);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        std::cout << "GLB: " << gltf.images.size() << " imagenes, " << gltf.textures.size()
            << " texturas, " << gltf.materials.size() << " materiales.\n";
        for (size_t i = 0; i < gltf.images.size(); ++i) {
            const auto& im = gltf.images[i];
            std::cout << "Imagen " << i << ": " << im.width << "x" << im.height
                << " canales=" << im.component << " bits=" << im.bits
                << " bytes=" << im.image.size() << "\n";
        }
        for (size_t i = 0; i < gltf.materials.size(); ++i) {
            const auto& mat = gltf.materials[i];
            std::cout << "Material " << i << " [" << mat.name << "] baseColorTexture="
                << mat.pbrMetallicRoughness.baseColorTexture.index;
            for (const auto& ext : mat.extensions) std::cout << " extension=" << ext.first;
            std::cout << "\n";
        }
        // Crear las texturas del modelo usando las imagenes del GLB.
        std::vector<GLuint> imagenes(gltf.images.size(), 0);
        for (size_t i = 0;i < gltf.images.size();i++) {
            imagenes[i] = texturaImagen(gltf.images[i]);
            if (!imagenes[i]) std::cout << "AVISO: imagen " << i << " no se pudo subir a OpenGL.\n";
        }

        // Preparar los puntos que dibujan el recorrido de la camara.
        std::vector<glm::vec3> curva;
        for (int i = 0;i <= 240;i++)curva.push_back(trayectoria(i / 240.0f));
        GLuint lineVAO, lineVBO;glGenVertexArrays(1, &lineVAO);glGenBuffers(1, &lineVBO);
        glBindVertexArray(lineVAO);glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
        glBufferData(GL_ARRAY_BUFFER, curva.size() * sizeof(glm::vec3), curva.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);glEnableVertexAttribArray(0);glBindVertexArray(0);

        // Estado de los modos, posicion de camara y tiempo de animacion.
        bool autoCam = false, girar = true, phong = true, luzMovil = true, mostrarCurva = false, pausa = false;
        bool prev1 = false, prev2 = false, prev3 = false, prevL = false, prevF = false, prevB = false, prevSpace = false;
        glm::vec3 cam(0, 1.3f, 4.8f);float yaw = -90, pitch = -7;
        float reloj = 0, anguloModelo = 0, ultimo = (float)glfwGetTime();
        std::cout << "\nCONTROLES\n1 camara manual | 2 camara Bezier | 3 giro personaje\n"
            << "L Phong | F luz movil | B trayectoria | ESPACIO pausa\n"
            << "WASD mover | Q/E bajar/subir | Flechas mirar | Shift rapido | R reiniciar | ESC salir\n";
        // Bucle principal: se repite mientras la ventana siga abierta.
        while (!glfwWindowShouldClose(w)) {
            float ahora = (float)glfwGetTime(), dt = std::min(0.05f, ahora - ultimo);ultimo = ahora;
            glfwPollEvents();
            if (glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS)glfwSetWindowShouldClose(w, true);
            if (teclaNueva(w, GLFW_KEY_1, prev1))autoCam = false;
            if (teclaNueva(w, GLFW_KEY_2, prev2))autoCam = true;
            if (teclaNueva(w, GLFW_KEY_3, prev3))girar = !girar;
            if (teclaNueva(w, GLFW_KEY_L, prevL))phong = !phong;
            if (teclaNueva(w, GLFW_KEY_F, prevF))luzMovil = !luzMovil;
            if (teclaNueva(w, GLFW_KEY_B, prevB))mostrarCurva = !mostrarCurva;
            if (teclaNueva(w, GLFW_KEY_SPACE, prevSpace))pausa = !pausa;
            // Actualizar el tiempo y el giro del personaje si no hay pausa.
            if (!pausa) { reloj += dt;if (girar)anguloModelo += dt * glm::radians(14.0f); }
            // Camara automatica con Bezier o camara manual con teclado.
            glm::vec3 frente;
            if (autoCam) { cam = trayectoria(reloj / 18.0f);frente = glm::normalize(glm::vec3(0, 1.35f, 0) - cam); }
            else {
                if (glfwGetKey(w, GLFW_KEY_LEFT) == GLFW_PRESS)yaw -= 75 * dt;
                if (glfwGetKey(w, GLFW_KEY_RIGHT) == GLFW_PRESS)yaw += 75 * dt;
                if (glfwGetKey(w, GLFW_KEY_UP) == GLFW_PRESS)pitch += 75 * dt;
                if (glfwGetKey(w, GLFW_KEY_DOWN) == GLFW_PRESS)pitch -= 75 * dt;
                pitch = glm::clamp(pitch, -89.0f, 89.0f);
                frente = glm::normalize(glm::vec3(cos(glm::radians(yaw)) * cos(glm::radians(pitch)), sin(glm::radians(pitch)), sin(glm::radians(yaw)) * cos(glm::radians(pitch))));
                glm::vec3 der = glm::normalize(glm::cross(frente, glm::vec3(0, 1, 0)));
                float paso = 2.5f * dt * (glfwGetKey(w, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 2.0f : 1.0f);
                if (glfwGetKey(w, GLFW_KEY_W) == GLFW_PRESS)cam += frente * paso;
                if (glfwGetKey(w, GLFW_KEY_S) == GLFW_PRESS)cam -= frente * paso;
                if (glfwGetKey(w, GLFW_KEY_A) == GLFW_PRESS)cam -= der * paso;
                if (glfwGetKey(w, GLFW_KEY_D) == GLFW_PRESS)cam += der * paso;
                if (glfwGetKey(w, GLFW_KEY_Q) == GLFW_PRESS)cam.y -= paso;
                if (glfwGetKey(w, GLFW_KEY_E) == GLFW_PRESS)cam.y += paso;
                if (glfwGetKey(w, GLFW_KEY_R) == GLFW_PRESS) { cam = { 0,1.3f,4.8f };yaw = -90;pitch = -7; }
            }
            // Preparar la ventana, la perspectiva y la vista de la camara.
            int ancho, alto;glfwGetFramebufferSize(w, &ancho, &alto);if (alto == 0)continue;
            glViewport(0, 0, ancho, alto);glClearColor(0.025f, 0.045f, 0.11f, 1);glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glm::mat4 proj = glm::perspective(glm::radians(55.0f), (float)ancho / alto, 0.05f, 100.0f);
            glm::mat4 view = glm::lookAt(cam, cam + frente, glm::vec3(0, 1, 0));
            // Matriz del modelo: rotacion del personaje y ajuste de escala.
            glm::mat4 giro = glm::rotate(glm::mat4(1), anguloModelo, glm::vec3(0, 1, 0));
            glm::mat4 modelo = giro * normalizar;
            // Posicion de la luz: fija o en movimiento segun la tecla F.
            glm::vec3 luz = luzMovil ? glm::vec3(3 * cos(reloj * 0.6f), 3.4f, 3 * sin(reloj * 0.6f)) : glm::vec3(2, 3, 3);
            // Dibujar cada parte del modelo con su material y textura.
            shaderModelo.usar();shaderModelo.setMat4("model", modelo);shaderModelo.setMat4("view", view);shaderModelo.setMat4("projection", proj);
            shaderModelo.setVec3("lightPos", luz);shaderModelo.setVec3("viewPos", cam);shaderModelo.setVec3("lightColor", glm::vec3(1.0f, 0.94f, 0.86f));
            shaderModelo.setBool("usarPhong", phong);
            shaderModelo.setInt("tex", 0);
            glActiveTexture(GL_TEXTURE0);glBindVertexArray(vao);
            for (const Parte& parte : e.partes) {
                GLuint t = blanco;glm::vec4 color(1);
                if (parte.material >= 0 && parte.material < (int)gltf.materials.size()) {
                    const auto& mat = gltf.materials[parte.material];
                    const auto& pbr = mat.pbrMetallicRoughness;
                    if (pbr.baseColorFactor.size() == 4)color = glm::vec4((float)pbr.baseColorFactor[0], (float)pbr.baseColorFactor[1], (float)pbr.baseColorFactor[2], (float)pbr.baseColorFactor[3]);
                    int texID = pbr.baseColorTexture.index;
                    // Algunos GLB antiguos utilizan KHR_materials_pbrSpecularGlossiness.
                    auto ext = mat.extensions.find("KHR_materials_pbrSpecularGlossiness");
                    if (ext != mat.extensions.end() && ext->second.IsObject()) {
                        const auto& ex = ext->second;
                        if (ex.Has("diffuseFactor") && ex.Get("diffuseFactor").IsArray()) {
                            const auto& arr = ex.Get("diffuseFactor").Get<tinygltf::Value::Array>();
                            if (arr.size() == 4)
                                for (int k = 0; k < 4; ++k) color[k] = (float)arr[k].GetNumberAsDouble();
                        }
                        if (ex.Has("diffuseTexture") && ex.Get("diffuseTexture").IsObject()) {
                            const auto& dt = ex.Get("diffuseTexture");
                            if (dt.Has("index") && dt.Get("index").IsInt()) texID = dt.Get("index").Get<int>();
                        }
                    }
                    if (texID >= 0 && texID < (int)gltf.textures.size()) {
                        int imagenID = gltf.textures[texID].source;
                        if (imagenID >= 0 && imagenID < (int)imagenes.size() && imagenes[imagenID])t = imagenes[imagenID];
                    }
                    // Mantener ambas caras visibles; el programa no activa culling.
                    (void)mat.doubleSided;
                }
                shaderModelo.setVec4("baseColor", color);
                glBindTexture(GL_TEXTURE_2D, t);
                glDrawElements(GL_TRIANGLES, (GLsizei)parte.cantidad, GL_UNSIGNED_INT, (void*)(uintptr_t)(parte.inicio * sizeof(unsigned int)));
            }
            glBindVertexArray(0);
            // Mostrar la curva en color cian cuando se active la tecla B.
            if (mostrarCurva) {
                shaderCurva.usar();shaderCurva.setMat4("view", view);shaderCurva.setMat4("projection", proj);
                shaderCurva.setVec3("color", glm::vec3(0.12f, 0.86f, 1.0f));
                glBindVertexArray(lineVAO);glLineWidth(2.0f);glDrawArrays(GL_LINE_STRIP, 0, (GLsizei)curva.size());glBindVertexArray(0);
            }
            glfwSwapBuffers(w);
        }
        // Liberar los recursos de OpenGL antes de cerrar la ventana.
        for (GLuint t : imagenes)if (t)glDeleteTextures(1, &t);
        glDeleteTextures(1, &blanco);glDeleteBuffers(1, &lineVBO);glDeleteVertexArrays(1, &lineVAO);
        glDeleteBuffers(1, &ebo);glDeleteBuffers(1, &vbo);glDeleteVertexArrays(1, &vao);
    } // Destructores de Shader: glDeleteProgram con contexto vigente.
    glfwDestroyWindow(w);glfwTerminate();return 0;
}