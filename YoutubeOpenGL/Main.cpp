#include<iostream>       
#include<cstdlib>        
#include<cstdio>         
#include<cmath>          
#include<algorithm>      
#include<Windows.h>
#include<tchar.h> 
#include<glad/glad.h>    
#include<GLFW/glfw3.h>   
#include<d3d11.h>
#include<dxgi.h> 

#include"IAMGUI/imgui.h"
#include"IAMGUI/imgui_impl_win32.h"
#include"IAMGUI/imgui_impl_dx11.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

static bool CreateDeviceD3D(HWND hWnd);
static void CleanupDeviceD3D();
static void CreateRenderTarget();
static void CleanupRenderTarget();
static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Código fuente del Vertex Shader (se ejecuta una vez por cada vértice)
const char* vertexShaderSource = "#version 330 core\n"
"layout (location = 0) in vec3 aPos;\n"
"layout (location = 1) in vec3 aColor;\n"
"out vec3 vColor;\n"
"void main()\n"
"{\n"
"   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
"   vColor = aColor;\n"
"}\0";

// Código fuente del Fragment Shader (se ejecuta una vez por cada píxel)
const char* fragmentShaderSource = "#version 330 core\n"
"in vec3 vColor;\n"
"out vec4 FragColor;\n"
"void main()\n"
"{\n"
"   FragColor = vec4(vColor, 1.0);\n"
"}\n\0";

const int GRILLA = 125;   // cantidad de cuadrados por lado
const int TAM = 6;        // tamaño de cada cuadrado en píxeles
const int VENTANA = GRILLA * TAM; // tamaño de la ventana en píxeles
const int MAX_FIGURAS = 100;      // cantidad máxima de figuras que se pueden guardar
const float ANGULO = 45.0f;       // ángulo (en grados) que gira una figura cada vez que se presiona "Rotar"

// Color con el que se pintan las figuras nuevas (lo cambia el selector de color de ImGui)
float colorActual[3] = { 1.0f, 0.3f, 0.0f };   // empieza en naranja, como antes

// CLASE CUADRADOS (encapsulación de la grilla)


// Crea un tipo llamado EstadoCelda para identificar qué tiene cada cuadrado de la grilla:
// VACIO = no tiene nada, LINEA = forma parte de la figura, INICIO = donde comienza la figura.
enum EstadoCelda { VACIO = 0, LINEA = 1, INICIO = 2 };

class Cuadrados
{
public:
    Cuadrados()
    {
        for (int i = 0; i < GRILLA * GRILLA * 3; i++)   // recorre todos los colores guardados
        {
            colores[i] = 0.0f;   // empiezan en 0 para que no tengan basura
        }

        for (int y = 0; y < GRILLA; y++)       // recorre cada fila
        {
            for (int x = 0; x < GRILLA; x++)   // recorre cada columna
            {
                estado[y * GRILLA + x] = VACIO;   // el cuadrado empieza vacío
                escribirCelda(x, y);              // calcula sus 4 vértices y su color
            }
        }

        for (int i = 0; i < GRILLA * GRILLA; i++)   // recorre todos los cuadrados
        {
            GLuint b = i * 4;         // número de la primera esquina de este cuadrado 
            indices[i * 6 + 0] = b;       // primer triángulo del cuadrado
            indices[i * 6 + 1] = b + 1;   // segunda esquina del primer triángulo
            indices[i * 6 + 2] = b + 2;   // tercera esquina del primer triángulo
            indices[i * 6 + 3] = b + 2;   // segundo triángulo del cuadrado
            indices[i * 6 + 4] = b + 3;   // segunda esquina del segundo triángulo
            indices[i * 6 + 5] = b;       // tercera esquina del segundo triángulo
        }

        glGenVertexArrays(1, &VAO);   // crea el VAO, que guarda cómo leer los datos de los vértices
        glGenBuffers(1, &VBO);        // crea el VBO, que guarda los vértices en la GPU
        glGenBuffers(1, &EBO);        // crea el EBO, que guarda los índices de los triángulos

        glBindVertexArray(VAO);               // activa el VAO para configurarlo
        glBindBuffer(GL_ARRAY_BUFFER, VBO);   // activa el VBO como buffer de vértices
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);   // copia todos los vértices a la GPU 

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);   // la posición son 3 floats al inicio de cada vértice de 6 floats
        glEnableVertexAttribArray(0);   // habilita el atributo 0 (posición)
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));   // el color son 3 floats que empiezan después de la posición
        glEnableVertexAttribArray(1);   // habilita el atributo 1 (color)

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);   // activa el EBO; queda guardado dentro del VAO
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);   // copia los índices a la GPU 
        glBindVertexArray(0);   // desactiva el VAO para no modificarlo por accidente
    }

    ~Cuadrados()
    {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
    }

    // Pinta el cuadrado de la columna x y la fila y con el estado indicado.
    // Si el estado es LINEA usa el color c; si no se manda ninguno, usa el color actual del panel.
    void pintar(int x, int y, EstadoCelda e, const float* c = nullptr)
    {
        if (x < 0 || x >= GRILLA || y < 0 || y >= GRILLA) return;   // si la posición está fuera de la grilla, no hace nada
        int i = y * GRILLA + x;   // posición del cuadrado dentro del arreglo de una sola dimensión
        estado[i] = e;            // guarda el nuevo estado del cuadrado
        if (e == LINEA)           // si el cuadrado es parte de una figura
        {
            if (c == nullptr) c = colorActual;   // si no se indicó color, usa el del panel
            colores[i * 3 + 0] = c[0];           // guarda el rojo con el que se pintó
            colores[i * 3 + 1] = c[1];           // guarda el verde con el que se pintó
            colores[i * 3 + 2] = c[2];           // guarda el azul con el que se pintó
        }
        escribirCelda(x, y);      // recalcula su color en el arreglo de vértices
    }

    // Devuelve el estado de un cuadrado
    int obtenerEstado(int x, int y)
    {
        return estado[y * GRILLA + x];   // lee el estado de ese cuadrado
    }

    // Deja todos los cuadrados vacíos
    void limpiar()
    {
        for (int y = 0; y < GRILLA; y++)       // recorre cada fila
        {
            for (int x = 0; x < GRILLA; x++)   // recorre cada columna
            {
                estado[y * GRILLA + x] = VACIO;   // el cuadrado vuelve a estar vacío
                escribirCelda(x, y);              // recalcula su color
            }
        }
    }

    // Manda a la GPU los colores actuales 
    void actualizar()
    {
        glBindBuffer(GL_ARRAY_BUFFER, VBO);   // activa el VBO
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);   // copia otra vez todos los vértices a la GPU
    }

    void dibujar()   // dibuja todos los cuadrados
    {
        glBindVertexArray(VAO);   // activa el VAO con la configuración de los vértices
        glDrawElements(GL_TRIANGLES, GRILLA * GRILLA * 6, GL_UNSIGNED_INT, 0);   // dibuja los triángulos usando los índices (6 por cuadrado)
    }

private:   // lo que solo se usa dentro de la clase
    float verts[GRILLA * GRILLA * 24];   // array: 4 vértices por cuadrado
    int estado[GRILLA * GRILLA];         // array: estado de cada cuadrado
    float colores[GRILLA * GRILLA * 3];  // array: color (r, g, b) con el que se pintó cada cuadrado
    GLuint indices[GRILLA * GRILLA * 6]; // array: 6 índices por cuadrado (2 triángulos)
    GLuint VAO = 0, VBO = 0, EBO = 0;    // identificadores de los objetos de OpenGL

    // Calcula los 4 vértices (posición + color) de un cuadrado y los guarda en el arreglo verts
    void escribirCelda(int x, int y)   // recibe la columna y la fila del cuadrado
    {
        int i = y * GRILLA + x;   // posición del cuadrado dentro del arreglo de una sola dimensión

        float x0 = -1.0f + x * (2.0f / GRILLA);         // borde izquierdo del cuadrado (de -1 a 1)
        float x1 = -1.0f + (x + 1) * (2.0f / GRILLA);   // borde derecho del cuadrado
        float y0 = -1.0f + y * (2.0f / GRILLA);         // borde inferior del cuadrado
        float y1 = -1.0f + (y + 1) * (2.0f / GRILLA);   // borde superior del cuadrado

        float r, g, b;   // componentes del color del cuadrado
        if (estado[i] == LINEA) { r = colores[i * 3 + 0]; g = colores[i * 3 + 1]; b = colores[i * 3 + 2]; }   // parte de la figura: el color con el que se pintó
        else if (estado[i] == INICIO) { r = 0.2f; g = 0.6f; b = 1.0f; }   // punto inicial: azul
        else if ((x + y) % 2 == 0) { r = 0.78f; g = 0.78f; b = 0.78f; }   // vacío con suma de coordenadas par: gris un poco más oscuro
        else { r = 0.95f; g = 0.95f; b = 0.95f; }   // vacío con suma de coordenadas impar: gris muy claro (efecto tablero)

        float esqX[4] = { x0, x1, x1, x0 };   // la coordenada X de las 4 esquinas
        float esqY[4] = { y0, y0, y1, y1 };   // la coordenada Y de las 4 esquinas
        for (int v = 0; v < 4; v++)   // recorre las 4 esquinas
        {
            int k = i * 24 + v * 6;   // dónde empiezan los datos de esta esquina en el arreglo
            verts[k + 0] = esqX[v];   // coordenada x de la esquina
            verts[k + 1] = esqY[v];   // coordenada y de la esquina
            verts[k + 2] = 0.0f;      // z, siempre 0 porque es 2D
            verts[k + 3] = r;         // componente rojo
            verts[k + 4] = g;         // componente verde
            verts[k + 5] = b;         // componente azul
        }
    }
};

// Una figura guardada: lo que hace falta para volver a dibujarla y para rotarla.
// Los puntos son float para que al rotar varias veces no se vayan deformando.
struct Figura
{
    int tipo;             // 0 = línea, 1 = círculo, 2 = triángulo, 3 = cuadrado
    int cantidad;         // cuántos puntos tiene (línea 2, círculo 1, triángulo 3, cuadrado 4)
    float px[4];          // coordenada X de cada punto (en cuadrados de la grilla)
    float py[4];          // coordenada Y de cada punto
    int radio;            // radio, solo se usa en el círculo
    float color[3];       // color del borde
    bool relleno;         // si la figura está rellena
    float colorRelleno[3];   // color del relleno
};

// VARIABLES GLOBALES 
Cuadrados* cuadrados = nullptr;   // la grilla (se crea en main, cuando ya existe el contexto)

Figura figuras[MAX_FIGURAS];      // las figuras creadas, en el orden en que se hicieron
int numFiguras = 0;               // cuántas figuras hay
int figuraElegida = -1;           // la figura elegida en la lista (-1 = ninguna)
const char* nombresFigura[4] = { "Linea", "Circulo", "Triangulo", "Cuadrado" };   // nombres para la lista del panel

// Herramientas del panel: 0 = línea, 1 = círculo, 2 = triángulo, 3 = cuadrado
int herramienta = 0;              // herramienta elegida en el panel
int herramientaAnterior = 0;      // la herramienta que había antes, para saber cuándo se cambia
bool tieneInicio = false;         // ya se hizo el primer clic de una figura
bool tieneSegundo = false;        // ya se hizo el segundo clic (solo se usa en el triángulo)
int tipoFigura = 0;               // la herramienta que había en el primer clic (0 línea, 1 círculo, 2 triángulo, 3 cuadrado)
int iniX = 0, iniY = 0;           // primer clic: inicio de la línea / centro del círculo / primer vértice
int segX = 0, segY = 0;           // segundo clic del triángulo: segundo vértice
int finX = 0, finY = 0;           // último clic: final de la línea / punto que define el radio / tercer vértice

// BRESENHAM PARA LINEAS
// Recorre la línea de izquierda a derecha, un cuadrado en X por paso, y una
// variable de decisión D indica cuándo hay que subir (o bajar) un cuadrado en Y.
// Con los ajustes del inicio, funciona en cualquier dirección.
void trazarBresenham(int x0, int y0, int x1, int y1)   // recibe los dos extremos de la línea
{
    bool empinada = std::abs(y1 - y0) > std::abs(x1 - x0);   // la línea es empinada si cambia más en Y que en X
    if (empinada)   // si es empinada
    {
        std::swap(x0, y0);   // intercambia X e Y del primer punto
        std::swap(x1, y1);   // intercambia X e Y del segundo punto
    }

    if (x0 > x1)   // si el primer punto está a la derecha del segundo
    {
        std::swap(x0, x1);   // intercambia las X de los dos puntos
        std::swap(y0, y1);   // intercambia las Y de los dos puntos
    }

    int dx = x1 - x0;                  // cuánto avanza la línea en X
    int dy = std::abs(y1 - y0);        // cuánto cambia la línea en Y, siempre positivo
    int pasoY = (y1 > y0) ? 1 : -1;    // dirección en Y: +1 si la línea sube, -1 si baja

    int D = 2 * dy - dx;   // valor inicial de la variable de decisión
    int y = y0;            // la Y empieza en la Y del primer punto

    for (int x = x0; x <= x1; x++)   // avanza un cuadrado en X hasta llegar al final
    {
        if (empinada) cuadrados->pintar(y, x, LINEA);   // si se intercambiaron los ejes, se pinta con X e Y al revés para deshacerlo
        else          cuadrados->pintar(x, y, LINEA);   // si no, se pinta el cuadrado (x, y) directamente

        if (D > 0)   // si D es positivo, la línea ya se acercó más a la siguiente fila
        {
            y = y + pasoY;     // se mueve un cuadrado en Y
            D = D - 2 * dx;    // se ajusta D por haber cambiado de fila
        }
        D = D + 2 * dy;   // en cada paso D aumenta según la pendiente de la línea
    }
}

// BRESENHAM PARA CIRCULOS (punto medio)
// Un círculo tiene simetría de 8 partes: por cada punto (x, y) calculado en un
// octante se pintan los 8 puntos equivalentes alrededor del centro (xc, yc).
void pintarOctantes(int xc, int yc, int x, int y)   // recibe el centro y un punto del primer octante
{
    cuadrados->pintar(xc + x, yc + y, LINEA);   // octante 1
    cuadrados->pintar(xc - x, yc + y, LINEA);   // octante 8: refleja en X
    cuadrados->pintar(xc + x, yc - y, LINEA);   // octante 4: refleja en Y
    cuadrados->pintar(xc - x, yc - y, LINEA);   // octante 5: refleja en X e Y
    cuadrados->pintar(xc + y, yc + x, LINEA);   // octante 2: intercambia X e Y
    cuadrados->pintar(xc - y, yc + x, LINEA);   // octante 7: intercambia y refleja en X
    cuadrados->pintar(xc + y, yc - x, LINEA);   // octante 3: intercambia y refleja en Y
    cuadrados->pintar(xc - y, yc - x, LINEA);   // octante 6: intercambia y refleja en X e Y
}

// Empieza en la parte de arriba del círculo, (0, R), y avanza hacia la derecha.
// El parámetro p decide, en cada paso, si el siguiente cuadrado queda en la misma
// fila o en la fila de abajo. Se calcula solo un octante y el resto sale por simetría.
void trazarCirculo(int xc, int yc, int r)   // recibe el centro y el radio
{
    int x = 0;        // X inicial, sobre el eje vertical del centro
    int y = r;        // Y inicial, a una distancia r por encima del centro
    int p = 1 - r;    // valor inicial del parámetro de decisión

    while (x <= y)   // repite mientras se esté dentro del primer octante
    {
        pintarOctantes(xc, yc, x, y);   // pinta este punto y sus 7 simétricos

        x++;   // avanza un cuadrado en X
        if (p < 0)   // si p es negativo, el siguiente cuadrado queda en la misma fila
        {
            p = p + 2 * x + 1;   // actualiza p sin cambiar Y
        }
        else   // si no, el siguiente cuadrado baja una fila
        {
            y--;                        // baja un cuadrado en Y
            p = p + 2 * (x - y) + 1;    // actualiza p teniendo en cuenta el cambio de X y de Y
        }
    }
}

// RELLENO: pinta todos los cuadrados vacíos conectados al cuadrado (x, y) hasta toparse con una figura.
// Usa una pila de cuadrados pendientes en lugar de recursión, porque la grilla es grande.
// La figura debe estar cerrada: si tiene un hueco, el relleno se sale y llena todo lo que alcance.
void rellenar(int x, int y)   // recibe un cuadrado de adentro de la figura
{
    if (x < 0 || x >= GRILLA || y < 0 || y >= GRILLA) return;   // si está fuera de la grilla, no hace nada
    if (cuadrados->obtenerEstado(x, y) != VACIO) return;        // si ya tiene algo pintado, no hace nada

    static int pila[GRILLA * GRILLA];   // cuadrados pendientes por revisar (cada cuadrado entra una sola vez)
    int tope = 0;                       // cuántos hay en la pila

    cuadrados->pintar(x, y, LINEA);     // pinta el cuadrado de partida
    pila[tope] = y * GRILLA + x;        // lo guarda en la pila
    tope++;                             // la pila crece en uno

    int vecinoX[4] = { 1, -1, 0, 0 };   // desplazamientos en X de los 4 vecinos: derecha, izquierda, arriba, abajo
    int vecinoY[4] = { 0, 0, 1, -1 };   // desplazamientos en Y de los 4 vecinos

    while (tope > 0)   // repite mientras queden cuadrados por revisar
    {
        tope--;                          // saca un cuadrado de la pila
        int cx = pila[tope] % GRILLA;    // su columna
        int cy = pila[tope] / GRILLA;    // su fila

        for (int k = 0; k < 4; k++)   // revisa sus 4 vecinos
        {
            int nx = cx + vecinoX[k];   // columna del vecino
            int ny = cy + vecinoY[k];   // fila del vecino
            if (nx < 0 || nx >= GRILLA || ny < 0 || ny >= GRILLA) continue;   // si está fuera de la grilla, lo salta
            if (cuadrados->obtenerEstado(nx, ny) != VACIO) continue;          // si ya tiene algo pintado, lo salta
            cuadrados->pintar(nx, ny, LINEA);   // pinta el vecino
            pila[tope] = ny * GRILLA + nx;      // lo guarda para revisar sus vecinos después
            tope++;                             // la pila crece en uno
        }
    }
}

// FIGURAS GUARDADAS
// La grilla siempre se puede volver a dibujar desde la lista de figuras. Así cada figura se puede rotar sola.

// Redondea un número al cuadrado más cercano
int redondear(float v)
{
    return (int)std::floor(v + 0.5f);   // suma 0.5 y se queda con la parte entera hacia abajo
}

// Calcula el centro de una figura: el promedio de sus puntos
void centroFigura(const Figura& f, float& cx, float& cy)
{
    cx = 0.0f;   // suma de las X
    cy = 0.0f;   // suma de las Y
    for (int k = 0; k < f.cantidad; k++)   // recorre los puntos de la figura
    {
        cx = cx + f.px[k];   // suma la X del punto
        cy = cy + f.py[k];   // suma la Y del punto
    }
    cx = cx / f.cantidad;   // promedio de las X
    cy = cy / f.cantidad;   // promedio de las Y
}

// Dibuja el borde de una figura usando el Bresenham de líneas o el de círculos
void dibujarFigura(const Figura& f)
{
    if (f.tipo == 1)   // si es un círculo
    {
        trazarCirculo(redondear(f.px[0]), redondear(f.py[0]), f.radio);   // traza el círculo con su centro y su radio
    }
    else   // si es una línea, un triángulo o un cuadrado: son lados entre puntos
    {
        int lados = (f.cantidad == 2) ? 1 : f.cantidad;   // la línea tiene un solo lado; las demás tienen uno por punto
        for (int k = 0; k < lados; k++)   // recorre los lados
        {
            int s = (k + 1) % f.cantidad;   // el punto siguiente (el último se une con el primero)
            trazarBresenham(redondear(f.px[k]), redondear(f.py[k]), redondear(f.px[s]), redondear(f.py[s]));   // traza el lado entre los dos puntos
        }
    }
}

// Deja la grilla vacía y vuelve a dibujar todas las figuras de la lista
void redibujarTodo()
{
    float guardado[3] = { colorActual[0], colorActual[1], colorActual[2] };   // guarda el color que hay en el panel, para devolverlo al final

    cuadrados->limpiar();   // deja la grilla vacía
    for (int k = 0; k < numFiguras; k++)   // recorre las figuras en el orden en que se crearon
    {
        colorActual[0] = figuras[k].color[0];   // usa el color del borde de esta figura
        colorActual[1] = figuras[k].color[1];
        colorActual[2] = figuras[k].color[2];
        dibujarFigura(figuras[k]);              // dibuja su borde

        if (figuras[k].relleno)   // si la figura está rellena
        {
            colorActual[0] = figuras[k].colorRelleno[0];   // usa el color del relleno
            colorActual[1] = figuras[k].colorRelleno[1];
            colorActual[2] = figuras[k].colorRelleno[2];
            float cx, cy;                            // centro de la figura
            centroFigura(figuras[k], cx, cy);        // lo calcula
            rellenar(redondear(cx), redondear(cy));  // rellena desde el centro, que está adentro de la figura
        }
    }

    colorActual[0] = guardado[0];   // devuelve el color del panel
    colorActual[1] = guardado[1];
    colorActual[2] = guardado[2];
    cuadrados->actualizar();   // manda el cambio a la GPU
}

// Cancela la figura que estuviera a medias
void cancelarFigura()
{
    bool habiaFigura = tieneInicio;   // recuerda si había una figura a medias
    tieneInicio = false;              // ya no hay punto inicial
    tieneSegundo = false;             // tampoco segundo punto
    if (habiaFigura) redibujarTodo(); // vuelve a dibujar solo las figuras guardadas: quita el punto azul y el lado de prueba del triángulo
}

// Guarda en la lista la figura que se acaba de terminar con los clics
void crearFigura()
{
    if (numFiguras >= MAX_FIGURAS) return;   // si la lista está llena, no guarda más

    Figura f;                 // la figura nueva
    f.tipo = tipoFigura;      // qué tipo de figura es
    f.radio = 0;              // el radio solo se usa en el círculo
    f.relleno = false;        // empieza sin relleno
    f.color[0] = colorActual[0];   // su color es el que hay en el panel
    f.color[1] = colorActual[1];
    f.color[2] = colorActual[2];
    f.colorRelleno[0] = 0.0f;      // color de relleno sin usar todavía
    f.colorRelleno[1] = 0.0f;
    f.colorRelleno[2] = 0.0f;

    if (tipoFigura == 0)   // si la figura es una línea
    {
        f.cantidad = 2;                 // dos puntos
        f.px[0] = (float)iniX;  f.py[0] = (float)iniY;   // el primer clic
        f.px[1] = (float)finX;  f.py[1] = (float)finY;   // el segundo clic
    }
    else if (tipoFigura == 1)   // si la figura es un círculo
    {
        f.cantidad = 1;                 // un punto: el centro
        f.px[0] = (float)iniX;  f.py[0] = (float)iniY;   // el centro es el primer clic
        // El radio es la distancia (en cuadrados) entre el centro y el segundo clic, calculada solo con enteros
        int dx = finX - iniX;   // diferencia en X entre el centro y el segundo clic
        int dy = finY - iniY;   // diferencia en Y entre el centro y el segundo clic
        int d2 = dx * dx + dy * dy;             // distancia al cuadrado, sin usar raíz
        int r = 0;                              // radio, empieza en 0
        while ((r + 1) * (r + 1) <= d2) r++;    // sube r mientras su cuadrado no pase esa distancia
        f.radio = r;                            // guarda el radio
    }
    else if (tipoFigura == 2)   // si la figura es un triángulo
    {
        f.cantidad = 3;                 // tres puntos
        f.px[0] = (float)iniX;  f.py[0] = (float)iniY;   // primer vértice
        f.px[1] = (float)segX;  f.py[1] = (float)segY;   // segundo vértice
        f.px[2] = (float)finX;  f.py[2] = (float)finY;   // tercer vértice
    }
    else   // si la figura es un cuadrado
    {
        int dx = finX - iniX;   // diferencia en X entre la esquina inicial y el segundo clic
        int dy = finY - iniY;   // diferencia en Y entre la esquina inicial y el segundo clic
        int lado = (std::abs(dx) > std::abs(dy)) ? std::abs(dx) : std::abs(dy);   // el lado es la mayor de las dos diferencias
        int signoX = (dx >= 0) ? 1 : -1;   // hacia qué lado crece el cuadrado en X
        int signoY = (dy >= 0) ? 1 : -1;   // hacia qué lado crece el cuadrado en Y
        int ex = iniX + signoX * lado;     // X de la esquina opuesta
        int ey = iniY + signoY * lado;     // Y de la esquina opuesta
        f.cantidad = 4;                 // cuatro esquinas
        f.px[0] = (float)iniX;  f.py[0] = (float)iniY;   // esquina inicial
        f.px[1] = (float)ex;    f.py[1] = (float)iniY;   // esquina a su lado en X
        f.px[2] = (float)ex;    f.py[2] = (float)ey;     // esquina opuesta
        f.px[3] = (float)iniX;  f.py[3] = (float)ey;     // esquina a su lado en Y
    }

    figuras[numFiguras] = f;         // guarda la figura en la lista
    figuraElegida = numFiguras;      // la deja elegida en el panel
    numFiguras++;                    // ahora hay una figura más
}

// ROTACION
// Gira una figura los grados indicados (positivo = sentido contrario a las agujas del reloj)
// alrededor de su propio centro: gira sus puntos y vuelve a dibujar todo.
void rotarFigura(int k, float grados)
{
    cancelarFigura();   // si había una figura a medias, la cancela

    Figura& f = figuras[k];   // la figura a rotar (se modifica directamente en la lista)
    float cx, cy;             // centro de la figura
    centroFigura(f, cx, cy);  // lo calcula

    float a = grados * 3.14159265f / 180.0f;   // pasa los grados a radianes
    float c = std::cos(a);                     // coseno del ángulo
    float s = std::sin(a);                     // seno del ángulo

    for (int p = 0; p < f.cantidad; p++)   // recorre los puntos de la figura
    {
        float dx = f.px[p] - cx;   // distancia en X al centro
        float dy = f.py[p] - cy;   // distancia en Y al centro
        f.px[p] = cx + dx * c - dy * s;   // nueva X del punto, girado
        f.py[p] = cy + dx * s + dy * c;   // nueva Y del punto, girado
    }

    redibujarTodo();   // dibuja de nuevo todas las figuras, con esta ya girada
}

// Convierte la posición del cursor a la columna y fila de la grilla.
// Devuelve false si el cursor está fuera de la grilla.
bool cursorACelda(GLFWwindow* window, int& i, int& j)
{
    double cx, cy;                        // posición del cursor dentro de la ventana
    glfwGetCursorPos(window, &cx, &cy);   // pide la posición actual del cursor
    int px = (int)cx;                     // X del cursor en píxeles, como entero
    int py = VENTANA - 1 - (int)cy;       // Y invertida porque en la ventana crece hacia abajo y en la grilla hacia arriba
    if (px < 0 || px >= VENTANA || py < 0 || py >= VENTANA) return false;   // si el cursor quedó fuera de la grilla, avisa con false
    i = px / TAM;   // columna del cuadrado bajo el cursor
    j = py / TAM;   // fila del cuadrado bajo el cursor
    return true;    // el cursor sí está dentro de la grilla
}

// Las figuras ya NO se borran al empezar otra: se quedan en la lista hasta que uses "Eliminar figura" o "Borrar todo".
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)   // GLFW la llama al presionar o soltar un botón del mouse
{
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;   // solo se usa el botón izquierdo
    if (action != GLFW_PRESS) return;               // solo importa el momento de presionar, no el de soltar

    int i, j;                                      // columna y fila donde se hizo clic
    if (!cursorACelda(window, i, j)) return;       // si el clic quedó fuera de la grilla, no hace nada

    if (!tieneInicio)   // si es el primer clic de una figura
    {
        tipoFigura = herramienta;   // guarda qué figura se va a hacer (0 línea, 1 círculo, 2 triángulo, 3 cuadrado)
        iniX = i;   // guarda la X del primer punto
        iniY = j;   // guarda la Y del primer punto
        tieneInicio = true;                     // ahora ya hay un punto inicial
        cuadrados->pintar(i, j, INICIO);        // marca el punto inicial en azul
    }
    else if (tipoFigura == 2 && !tieneSegundo)   // si es el segundo clic de un triángulo
    {
        segX = i;   // guarda la X del segundo vértice
        segY = j;   // guarda la Y del segundo vértice
        tieneSegundo = true;    // ahora ya hay dos vértices
        trazarBresenham(iniX, iniY, segX, segY);   // traza el primer lado del triángulo, de prueba mientras se hace el tercer clic
    }
    else   // si es el último clic de la figura
    {
        finX = i;   // guarda la X del último punto
        finY = j;   // guarda la Y del último punto
        tieneInicio = false;    // termina el proceso de clics
        tieneSegundo = false;   // el triángulo termina
        crearFigura();          // guarda la figura en la lista
        redibujarTodo();        // dibuja todas las figuras (quita también el punto azul)
    }
    cuadrados->actualizar();   // manda los colores nuevos a la GPU para que se vean
}

int main()   // punto de entrada del programa
{
    HINSTANCE hInstance = GetModuleHandle(nullptr);   // obtiene el identificador del programa, que antes llegaba como parámetro
    glfwInit();   // inicializa GLFW
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);   // pide OpenGL versión 3.x
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);   // pide OpenGL versión x.3, o sea 3.3
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // Usa el perfil Core de OpenGL 
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);      // la ventana no se puede cambiar de tamaño

    GLFWwindow* window = glfwCreateWindow(VENTANA, VENTANA, "Grilla Bresenham", NULL, NULL);   // crea una ventana cuadrada con ese título
    if (window == NULL)   // si no se pudo crear la ventana
    {
        std::cout << "Failed to create GLFW window" << std::endl;   // muestra un mensaje de error
        glfwTerminate();   // cierra GLFW
        return -1;         // termina el programa con error
    }
    glfwMakeContextCurrent(window);   // hace que esta ventana sea donde OpenGL dibuja
    glfwSwapInterval(0);              // sin sincronización aquí: el ritmo lo marca la ventana del panel (Present con vsync)
    gladLoadGL();                     // carga las funciones de OpenGL

    int anchoFb, altoFb;                                  // tamaño real del área de dibujo en píxeles
    glfwGetFramebufferSize(window, &anchoFb, &altoFb);    // pide ese tamaño a GLFW
    glViewport(0, 0, anchoFb, altoFb); // Define el área de renderizado dentro de la ventana 

    //shaders
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);   // crea el objeto del vertex shader
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);   // le asigna su código fuente
    glCompileShader(vertexShader);   // lo compila

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);   // crea el objeto del fragment shader
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);   // le asigna su código fuente
    glCompileShader(fragmentShader);   // lo compila

    // Crea el programa shader que enlazará ambos shaders
    GLuint shaderProgram = glCreateProgram();   // crea el programa que une los dos shaders
    glAttachShader(shaderProgram, vertexShader);     // agrega el vertex shader al programa
    glAttachShader(shaderProgram, fragmentShader);   // agrega el fragment shader al programa
    glLinkProgram(shaderProgram);                    // enlaza los shaders en un solo programa

    glDeleteShader(vertexShader);     // borra el vertex shader, ya está dentro del programa
    glDeleteShader(fragmentShader);   // borra el fragment shader, ya está dentro del programa

    cuadrados = new Cuadrados();   // crea la grilla; se hace aquí porque necesita el contexto de OpenGL

    glfwSetMouseButtonCallback(window, mouseButtonCallback);   // avisa a GLFW qué función llamar con los clics

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);   // color de fondo: gris muy oscuro

    // VENTANA DEL PANEL (Win32 + DX11): aquí se dibuja ImGui
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, hInstance, nullptr, nullptr, nullptr, nullptr, _T("PanelImGui"), nullptr };   // describe cómo es la ventana del panel
    RegisterClassEx(&wc);   // registra esa descripción en Windows

    int ventanaX, ventanaY;                         // posición de la ventana de la grilla en la pantalla
    glfwGetWindowPos(window, &ventanaX, &ventanaY); // la pide a GLFW para poner el panel a su derecha

    HWND hwnd = CreateWindow(wc.lpszClassName, _T("Herramientas"), WS_OVERLAPPEDWINDOW,
        ventanaX + VENTANA + 20, ventanaY, 320, VENTANA, nullptr, nullptr, hInstance, nullptr);   // crea la ventana del panel al lado de la grilla

    if (!CreateDeviceD3D(hwnd))   // si no se pudo crear DirectX 11
    {
        CleanupDeviceD3D();                  // libera lo que se haya creado
        UnregisterClass(wc.lpszClassName, wc.hInstance);   // quita el registro de la ventana
        glfwTerminate();                     // cierra GLFW
        return 1;                            // termina el programa con error
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);   // muestra la ventana del panel
    UpdateWindow(hwnd);                 // la actualiza una vez

    // IMGUI: inicio
    IMGUI_CHECKVERSION();                                          // revisa que los archivos de ImGui sean de la misma versión
    ImGui::CreateContext();                                        // crea el contexto de ImGui
    ImGui::StyleColorsDark();                                      // estilo oscuro para el panel
    ImGui_ImplWin32_Init(hwnd);                                    // conecta ImGui con la ventana de Windows
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);        // conecta ImGui con DirectX 11

    bool corriendo = true;   // pasa a false cuando se cierra alguna de las dos ventanas

    while (!glfwWindowShouldClose(window) && corriendo)   // repite hasta que se cierre una de las dos ventanas
    {
        glfwPollEvents();   // procesa los eventos del mouse y de la ventana de la grilla

        MSG msg;   // mensaje de Windows
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))   // procesa los mensajes pendientes de la ventana del panel
        {
            TranslateMessage(&msg);   // traduce el mensaje
            DispatchMessage(&msg);    // lo manda a WndProc
            if (msg.message == WM_QUIT) corriendo = false;   // si se cerró el panel, termina el programa
        }

        // IMGUI: empieza un cuadro nuevo
        ImGui_ImplDX11_NewFrame();    // prepara el backend de DirectX 11
        ImGui_ImplWin32_NewFrame();   // prepara el backend de Windows (lee el mouse)
        ImGui::NewFrame();            // empieza a construir la interfaz

        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));              // el panel empieza en la esquina de su ventana
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);     // y ocupa toda la ventana
        ImGui::Begin("Herramientas", nullptr,
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);   // sin mover, sin cambiar tamaño, sin colapsar y sin barra de título

        ImGui::Text("Herramienta");                          // título de la sección
        ImGui::RadioButton("Linea", &herramienta, 0);        // opción línea
        ImGui::RadioButton("Circulo", &herramienta, 1);      // opción círculo
        ImGui::RadioButton("Triangulo", &herramienta, 2);    // opción triángulo
        ImGui::RadioButton("Cuadrado", &herramienta, 3);     // opción cuadrado

        if (herramienta != herramientaAnterior)   // si se cambió de herramienta
        {
            cancelarFigura();                   // cancela la figura que estuviera a medias
            herramientaAnterior = herramienta;  // recuerda la nueva herramienta
        }

        ImGui::Separator();                        // línea separadora
        ImGui::Text("Color");   // título del selector
        ImGui::SameLine();      // el recuadro va en la misma línea
        if (ImGui::ColorButton("##color", ImVec4(colorActual[0], colorActual[1], colorActual[2], 1.0f), ImGuiColorEditFlags_NoTooltip, ImVec2(40.0f, 20.0f)))   // recuadro con el color actual, sin texto al pasar el mouse
        {
            ImGui::OpenPopup("selector_color");   // al hacer clic se abre el selector
        }
        if (ImGui::BeginPopup("selector_color"))   // el selector de color
        {
            ImGui::ColorPicker3("##selector", colorActual, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoTooltip);   // solo el cuadro de color y la barra, sin números, sin código y sin vista previa
            ImGui::EndPopup();   // cierra el selector
        }



        ImGui::Separator();                        // línea separadora
        if (ImGui::Button("Borrar todo"))          // si se presiona el botón
        {
            numFiguras = 0;        // la lista queda vacía
            figuraElegida = -1;    // ya no hay figura elegida
            cancelarFigura();      // cancela la figura que estuviera a medias
            redibujarTodo();       // deja la grilla vacía
        }

        ImGui::Separator();                        // línea separadora
        ImGui::Text("Figuras creadas");            // título de la lista
        ImGui::BeginChild("lista", ImVec2(0.0f, 170.0f), true);   // cuadro con scroll para la lista
        for (int k = 0; k < numFiguras; k++)   // recorre las figuras guardadas
        {
            char etiqueta[40];   // texto que se muestra en la lista
            snprintf(etiqueta, sizeof(etiqueta), "%d. %s", k + 1, nombresFigura[figuras[k].tipo]);   // por ejemplo "2. Circulo"
            if (ImGui::Selectable(etiqueta, figuraElegida == k))   // si se hace clic en esta figura
            {
                cancelarFigura();   // cancela la figura que estuviera a medias
                figuraElegida = k;  // la deja elegida
            }
        }
        ImGui::EndChild();   // cierra el cuadro de la lista

        if (figuraElegida >= 0)   // si hay una figura elegida
        {
            ImGui::Text("Figura elegida: %d", figuraElegida + 1);       // muestra cuál es
            if (ImGui::Button("Rotar 45 grados")) rotarFigura(figuraElegida, ANGULO);   // gira solo esta figura 45 grados
            if (figuras[figuraElegida].tipo == 1)   // si es un círculo
                ImGui::TextWrapped("Un circulo no cambia al rotarlo.");   // aviso: girar un círculo sobre su centro no se nota

            if (ImGui::Button("Eliminar figura"))   // si se presiona el botón
            {
                cancelarFigura();   // cancela la figura que estuviera a medias
                for (int k = figuraElegida; k < numFiguras - 1; k++)   // recorre las figuras que están después de la eliminada
                {
                    figuras[k] = figuras[k + 1];   // cada una ocupa el lugar de la anterior
                }
                numFiguras--;         // hay una figura menos
                figuraElegida = -1;   // ya no hay figura elegida
                redibujarTodo();      // dibuja de nuevo las figuras que quedan
            }
        }
        else   // si no hay ninguna figura elegida
        {
            ImGui::TextWrapped("Elige una figura de la lista para rotarla o eliminarla.");   // explica qué hacer
        }

        // BOTON RELLENAR FIGURA
        if (figuras[figuraElegida].tipo != 0)
        {
            if (ImGui::Button("Rellenar figura"))
            {
                figuras[figuraElegida].relleno = true;

                figuras[figuraElegida].colorRelleno[0] = colorActual[0];
                figuras[figuraElegida].colorRelleno[1] = colorActual[1];
                figuras[figuraElegida].colorRelleno[2] = colorActual[2];

                redibujarTodo();
            }
        }
        else
        {
            ImGui::TextWrapped("Las lineas no se pueden rellenar.");
        }

        ImGui::Separator();     // línea separadora
        if (tieneInicio && tipoFigura == 2 && !tieneSegundo)   // si es un triángulo y falta el segundo vértice
            ImGui::TextWrapped("Haz clic en el segundo vertice.");   // pide el segundo clic
        else if (tieneInicio && tipoFigura == 2)   // si es un triángulo y falta el tercer vértice
            ImGui::TextWrapped("Haz clic en el tercer vertice.");   // pide el tercer clic
        else if (tieneInicio)   // si ya se hizo el primer clic
            ImGui::TextWrapped("Haz clic donde termina la figura.");   // pide el segundo clic
        else                    // si todavía no se hace ningún clic
            ImGui::TextWrapped("Haz clic donde empieza la figura.");   // pide el primer clic

        ImGui::End();      // cierra el panel
        ImGui::Render();   // ImGui prepara lo que va a dibujar

        // DIBUJO DEL PANEL (DirectX 11)
        const float colorFondoPanel[4] = { 0.1f, 0.1f, 0.1f, 1.0f };   // color de fondo del panel
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);       // elige dónde se dibuja: la ventana del panel
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, colorFondoPanel); // borra el panel con el color de fondo
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());   // dibuja ImGui en el panel
        g_pSwapChain->Present(1, 0);                           // muestra el panel (con vsync)

        // DIBUJO DE LA GRILLA (OpenGL)
        glClear(GL_COLOR_BUFFER_BIT);   // borra la pantalla con el color de fondo
        glUseProgram(shaderProgram);    // usa el programa de shaders

        // dibujar toda la grilla (las figuras ya están pintadas dentro de los cuadrados)
        cuadrados->dibujar();   // dibuja todos los cuadrados

        glfwSwapBuffers(window);   // muestra en pantalla lo que se acaba de dibujar
    }

    // IMGUI: cierre
    ImGui_ImplDX11_Shutdown();     // cierra el backend de DirectX 11
    ImGui_ImplWin32_Shutdown();    // cierra el backend de Windows
    ImGui::DestroyContext();       // destruye el contexto de ImGui

    CleanupDeviceD3D();                                // libera DirectX 11
    DestroyWindow(hwnd);                               // cierra la ventana del panel
    UnregisterClass(wc.lpszClassName, wc.hInstance);   // quita el registro de la ventana

    delete cuadrados; // libera VAO/VBO/EBO antes de cerrar el contexto
    glDeleteProgram(shaderProgram);   // borra el programa de shaders

    glfwDestroyWindow(window);   // cierra la ventana
    glfwTerminate();             // libera los recursos de GLFW
    return 0;                    // termina el programa sin errores
}

static bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = {
        D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0
    };

    HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
        featureLevelArray, 2, D3D11_SDK_VERSION,
        &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext
    );

    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

static void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

static void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer)
    {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

static void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;

    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;

    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}