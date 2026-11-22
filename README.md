
# SciFi Girl | Exhibición 3D Interactiva

Proyecto desarrollado en **C++ y OpenGL 3.3** que permite visualizar e interactuar con un personaje tridimensional mediante diferentes opciones de cámara, iluminación y movimiento.

La aplicación carga un modelo en formato GLB y lo representa en una escena 3D, utilizando shaders, texturas y transformaciones geométricas. Además, incorpora una cámara automática que recorre una trayectoria construida con curvas de Bézier.

## 1. Características del proyecto

- Visualización de un personaje 3D con sus materiales y texturas.
- Iluminación Phong para mejorar la apariencia del modelo.
- Rotación automática del personaje.
- Cámara manual controlada mediante el teclado.
- Cámara automática con trayectoria de Bézier.
- Luz móvil que cambia de posición alrededor de la escena.
- Visualización de la trayectoria de la cámara.
- Opciones para pausar y activar o desactivar diferentes efectos.

## 2. Herramientas y tecnologías

| Tecnología | Función |
|---|---|
| C++ | Lenguaje utilizado para desarrollar la aplicación. |
| OpenGL 3.3 | Renderizado de los elementos tridimensionales. |
| GLFW | Creación de la ventana y lectura de entradas del teclado. |
| GLAD | Acceso a las funciones de OpenGL. |
| GLM | Operaciones matemáticas y transformaciones 3D. |
| TinyGLTF | Lectura del modelo en formato GLB. |
| stb_image | Procesamiento de imágenes y texturas. |
| Visual Studio | Entorno de desarrollo y compilación. |

## 3. Arquitectura y funcionamiento del programa

El proyecto se desarrolla principalmente en `Main.cpp`, donde se encuentran las funciones necesarias para cargar el modelo, configurar OpenGL y controlar la escena.

El funcionamiento general se organiza de la siguiente manera:

```text
                INICIO DEL PROGRAMA
                        |
                        v
              Carga del modelo GLB
                        |
                        v
          Lectura de vértices y materiales
                        |
                        v
          Construcción de la escena 3D
                        |
                        v
            Inicialización de OpenGL
                        |
                        v
           Configuración de los shaders
                        |
                        v
         Preparación de buffers y texturas
                        |
                        v
                BUCLE PRINCIPAL
                        |
          +-------------+-------------+
          |             |             |
          v             v             v
       Teclado        Cámara      Iluminación
          |             |             |
          +-------------+-------------+
                        |
                        v
               Renderizado 3D
                        |
                        v
             Actualización de ventana
```

Esta organización permite separar las tareas de preparación de datos, configuración gráfica e interacción durante la ejecución.

### 3.1. Lectura y construcción del modelo

El archivo principal que utiliza la aplicación es:

`models/scifi_girl_v.01.glb`

La biblioteca TinyGLTF permite obtener la información del modelo, incluyendo sus mallas, vértices, índices, materiales y texturas.

Para organizar estos datos se utilizan tres estructuras:

- `Vertice`: almacena posición, normal, coordenadas de textura y color.
- `Parte`: identifica un conjunto de índices y su material.
- `Escena`: reúne los vértices, índices y partes del modelo.

Entre las funciones principales se encuentran:

| Función | Descripción |
|---|---|
| `datosAccessor()` | Localiza los datos almacenados en los buffers del GLB. |
| `leerIndice()` | Interpreta los índices utilizados para formar los triángulos. |
| `matrizNodo()` | Obtiene las transformaciones de cada nodo del modelo. |
| `agregarMalla()` | Extrae la geometría y la incorpora a la escena. |
| `visitarNodo()` | Recorre la jerarquía de nodos del archivo GLB. |
| `construirEscena()` | Organiza la geometría para enviarla a OpenGL. |

El programa también calcula normales cuando una malla no las proporciona, lo que permite realizar los cálculos de iluminación.

### 3.2. Preparación de la geometría

Una vez obtenidos los datos del modelo, se transfieren a la GPU mediante los siguientes elementos:

**VAO (Vertex Array Object):**
Almacena la configuración de los atributos de los vértices.

**VBO (Vertex Buffer Object):**
Guarda la información de los vértices, como sus posiciones, normales y coordenadas de textura.

**EBO (Element Buffer Object):**
Contiene los índices utilizados para formar los triángulos.

El dibujo de las diferentes partes del personaje se realiza mediante:

`glDrawElements(GL_TRIANGLES, ...)`

Esto permite representar las mallas del personaje utilizando los materiales correspondientes.

### 3.3. Shaders y renderizado

El proyecto utiliza shaders escritos en GLSL para procesar la geometría y calcular el color de los fragmentos.

**Vertex Shader**

Se encarga de transformar los vértices utilizando las matrices:

- `model`: posición, escala y rotación del personaje.
- `view`: posición y orientación de la cámara.
- `projection`: perspectiva de la escena.

La transformación general se expresa como:

`gl_Position = projection * view * model * vec4(aPos, 1.0);`

**Fragment Shader**

Calcula el color visible de cada fragmento, combinando:

- Textura del modelo.
- Color del material.
- Color del vértice.
- Iluminación de la escena.

También se utilizan shaders adicionales para dibujar la trayectoria de la cámara.

### 3.4. Iluminación Phong

La escena incorpora iluminación con componentes ambientales, difusas y especulares.

- **Ambiental:** mantiene una iluminación básica sobre el personaje.
- **Difusa:** depende de la orientación de la superficie respecto a la luz.
- **Especular:** produce reflejos que ayudan a destacar la forma del modelo.

El cálculo se realiza dentro del Fragment Shader.

La aplicación permite activar o desactivar esta iluminación para comparar la apariencia del personaje.

También se implementa una luz móvil cuya posición cambia con el tiempo mediante funciones trigonométricas.

### 3.5. Cámara y trayectoria de Bézier

El proyecto cuenta con dos formas de controlar la cámara.

**Cámara manual**

Permite desplazarse por el entorno y modificar la dirección de observación utilizando el teclado.

La orientación se calcula mediante los ángulos `yaw` y `pitch`, mientras que la matriz de vista se construye con `glm::lookAt()`.

**Cámara automática**

Utiliza curvas de Bézier cúbicas para generar una trayectoria cerrada alrededor del personaje.

La ecuación empleada es:

`B(t) = (1-t)^3 P0 + 3(1-t)^2 t P1 + 3(1-t)t^2 P2 + t^3 P3`

Donde `P0`, `P1`, `P2` y `P3` representan los puntos de control y `t` determina la posición sobre la curva.

Las funciones utilizadas son:

- `bezier()`: calcula un punto de una curva cúbica.
- `trayectoria()`: conecta cuatro tramos de Bézier para formar un recorrido cerrado.

La cámara se desplaza por esta trayectoria mientras mantiene su orientación hacia el personaje.

### 3.6. Interacción y actualización de la escena

El programa utiliza un bucle principal que permanece activo mientras la ventana esté abierta.

Durante cada iteración se realizan las siguientes operaciones:

1. Lectura de eventos y entradas del teclado.
2. Actualización del tiempo de animación.
3. Cálculo de la posición de la cámara.
4. Actualización de la rotación del personaje.
5. Cálculo de la posición de la luz.
6. Configuración de matrices y shaders.
7. Dibujo del modelo y, opcionalmente, de la trayectoria.
8. Actualización de la ventana.

La función `teclaNueva()` permite detectar pulsaciones individuales para cambiar los modos sin repetir continuamente la acción mientras una tecla permanece presionada.

## 4. Controles de la aplicación

| Tecla | Acción |
|---|---|
| `1` | Activar cámara manual. |
| `2` | Activar cámara automática con Bézier. |
| `3` | Activar o desactivar el giro del personaje. |
| `L` | Activar o desactivar iluminación Phong. |
| `F` | Activar o desactivar el movimiento de la luz. |
| `B` | Mostrar u ocultar la trayectoria de la cámara. |
| `Espacio` | Pausar o reanudar la animación. |
| `W / A / S / D` | Desplazar la cámara manualmente. |
| `Q / E` | Bajar o subir la cámara. |
| `Flechas` | Cambiar la dirección de observación. |
| `Shift izquierdo` | Aumentar la velocidad de desplazamiento. |
| `R` | Restablecer la cámara manual. |
| `Esc` | Cerrar la aplicación. |

## 5. Organización de los archivos

```text
OpenGL-3D-SciFi-Girl/
|
|-- Libraries/
|   |-- include/
|   |   |-- GLFW/
|   |   |-- glad/
|   |   |-- glm/
|   |   |-- tinygltf/
|   |-- lib/
|       |-- glfw3.lib
|
|-- models/
|   |-- scifi_girl_v.01.glb
|   |-- Duck.glb
|
|-- Main.cpp
|-- glad.c
|-- stb_image.h
|
|-- textura1.jpg
|-- textura2.jpg
|-- textura3.jpg
|
|-- Tutorial.slnx
|-- Tutorial.vcxproj
|-- Tutorial.vcxproj.filters
|-- .gitignore
```

El archivo `Main.cpp` contiene la implementación principal. La carpeta `Libraries` incluye las dependencias necesarias y `models` almacena los modelos tridimensionales.

El modelo utilizado por defecto es `scifi_girl_v.01.glb`. El repositorio también contiene otros recursos que no forman parte de la escena principal.

## 6. Cómo ejecutar el proyecto

1. Descargar o clonar el repositorio.
2. Abrir `Tutorial.slnx` en Visual Studio.
3. Comprobar que las rutas de las bibliotecas y del enlazador estén configuradas correctamente.
4. Seleccionar la configuración de compilación correspondiente.
5. Verificar que el directorio de trabajo permita encontrar la carpeta `models`.
6. Compilar y ejecutar el proyecto.

El programa requiere un entorno compatible con OpenGL 3.3.

La ruta del modelo está definida en `Main.cpp`:

```cpp
const std::string RUTA_MODELO = "models/scifi_girl_v.01.glb";
```

Por esta razón, el directorio de trabajo debe contener la carpeta `models` con el archivo correspondiente.

## 7. Alcance del proyecto

SciFi Girl permite explorar conceptos de gráficos por computadora mediante la visualización de modelos tridimensionales, la aplicación de materiales, la iluminación y el movimiento de cámaras.

Aunque el programa permite rotar el personaje completo, no implementa animación esquelética ni deformaciones individuales de sus partes.

La iluminación Phong es una implementación propia en GLSL y no reproduce completamente el sistema PBR de los materiales glTF.

## 8. Conclusión

El proyecto reúne diferentes herramientas y conceptos de programación gráfica para desarrollar una escena tridimensional interactiva.

Su implementación permite comprender cómo se procesan los datos de un modelo 3D, cómo se utilizan los shaders para representar superficies y cómo las transformaciones matemáticas permiten controlar el movimiento de objetos y cámaras.

La combinación de iluminación, texturas y trayectorias de Bézier ofrece una forma práctica de estudiar el funcionamiento de OpenGL y la construcción de aplicaciones gráficas en tiempo real.
