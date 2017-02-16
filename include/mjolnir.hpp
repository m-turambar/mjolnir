#ifndef MJOLNIRHPP
#define MJOLNIRHPP

#include <vector>
#include <memory>

#include <opencv2/opencv.hpp>

class objeto;

extern cv::Mat region;
extern cv::Mat mat_header;

extern int ancho_region;  //w = 2dx
extern int altura_region; //h = 2dy
extern cv::Point dxy;     // (w/2, h/2)

/*se ven feos*/
const cv::Point HEADER0(5,20);
const cv::Point HEADER1(100,20);
const cv::Point HEADER2(300,20);
extern cv::Point HEADER_MSG;

/**apuntador a un elemento de un vector de apuntadores. OK. SWAG.*/
using Apuntador = std::vector<std::unique_ptr<objeto>>::iterator;

extern Apuntador itr_seleccionado;
extern Apuntador itr_highlight;
extern bool b_drag;
extern bool b_resize;
extern cv::Point ptInicioArrastre;
extern cv::Point ptFinArrastre;
extern std::vector<std::unique_ptr<objeto>> objetos;

enum class Flags {Vacia, Objeto, SinCambios}; //no me convence
Apuntador determinar_propiedades_ubicacion(cv::Point);

void renderizarDiagrama(cv::Mat& matriz);
void manejarInputTeclado(int k);
void manejarInputMouse(int event, int x, int y, int flags, void*);

extern cv::Point operator/(cv::Point p, const int d);

/**transforma los puntos absolutos a puntos relativos renderizables
p' = f(p) = dx + (p - dx - d)/z*/
extern cv::Point transformar(const cv::Point p);

/**transforma un punto relativo a un punto absoluto
p = g(p') = z*(p' - dx) + dx + d */
extern cv::Point transformacion_inversa(const cv::Point pp);

extern void rellenar_zona_telares(); //zonas
extern void anexar_zonas();

extern const char* nombreDiagrama;

#endif // MJOLNIRHPP
