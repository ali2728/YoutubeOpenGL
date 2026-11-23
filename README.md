# Grilla Interactiva - Algoritmos de Bresenham

> Aplicación gráfica desarrollada en C++ que permite dibujar figuras geométricas sobre una grilla interactiva utilizando los algoritmos de Bresenham para líneas y círculos. Cuenta con herramientas para cambiar colores, rellenar figuras, rotarlas y eliminarlas mediante una interfaz gráfica.

## Built With

- **C++:** Lenguaje de programación.
- **OpenGL 3.3:** Renderizado de la grilla y las figuras.
- **GLFW:** Creación de ventanas y gestión de eventos del mouse.
- **GLAD:** Carga de funciones de OpenGL.
- **Dear ImGui:** Creación del panel de herramientas.
- **Win32 API:** Gestión de ventanas en Windows.
- **DirectX 11:** Renderizado de la interfaz gráfica.

## Getting Started

Para obtener una copia local del proyecto y ejecutarlo, sigue estos pasos.

### Prerequisites

- Windows.
- Visual Studio con soporte para C++.
- OpenGL 3.3 o superior.
- GLFW.
- GLAD.
- Dear ImGui.
- DirectX 11 SDK / componentes de desarrollo de Windows.
- Editor de código (Visual Studio recomendado).

### Install

1. Clonar el repositorio:

   ```sh
   git clone https://github.com/USUARIO/REPOSITORIO.git
   ```

2. Abrir la solución del proyecto en Visual Studio.

3. Verificar que las librerías GLFW, GLAD e ImGui estén correctamente configuradas.

4. Comprobar que las dependencias de DirectX 11 estén disponibles.

5. Seleccionar la configuración de compilación correspondiente (Debug o Release).

6. Compilar el proyecto desde Visual Studio.

<p align="right">(<a href="#readme-top">back to top</a>)</p>

### Usage

1. Ejecutar el proyecto desde Visual Studio.

2. Seleccionar una herramienta en el panel:
   - Línea.
   - Círculo.
   - Triángulo.
   - Cuadrado.

3. Hacer clic sobre la grilla para establecer los puntos de la figura.

4. Utilizar el selector de color para elegir el color de las nuevas figuras.

5. Seleccionar una figura de la lista para acceder a las siguientes opciones:
   - **Rotar 45 grados:** gira la figura seleccionada.
   - **Rellenar figura:** aplica el color seleccionado al interior de la figura.
   - **Eliminar figura:** elimina la figura seleccionada.

6. Utilizar el botón **Borrar todo** para limpiar la grilla y eliminar todas las figuras.

### Features

- Grilla interactiva de 125 × 125 cuadrados.
- Dibujo de líneas mediante el algoritmo de Bresenham.
- Dibujo de círculos mediante el algoritmo del punto medio.
- Creación de líneas, círculos, triángulos y cuadrados.
- Selector de colores.
- Relleno de figuras mediante Flood Fill.
- Rotación de figuras en intervalos de 45 grados.
- Selección y eliminación individual de figuras.
- Lista de figuras creadas.
- Capacidad máxima de 100 figuras.

## Authors

👤 **Alison Molina**

- GitHub: [@ali2728](https://github.com/ali2728)
- https://youtu.be/rNYdN4u2kmM?si=xP96POcfw-b_Mv4v

## Acknowledgments

- Documentación oficial de [OpenGL](https://www.opengl.org/).
- Documentación de [GLFW](https://www.glfw.org/).
- [Dear ImGui](https://github.com/ocornut/imgui).
- Algoritmos de dibujo de Bresenham.
