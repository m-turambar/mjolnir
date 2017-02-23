#include "elemento_diagrama.h"
#include <windows.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <mutex>
#include <string>
#include <algorithm>

using namespace std;
using namespace cv;

mutex mtx_objetos;
int objeto::sid = 1; //hasta donde sabes debe definirse fuera de la clase, y no en el header
const Point objeto::offset_puntos_clave_(3,3);

//void comparar_por_area(const un)

void ordenar_objetos() //debe ser llamada expl�citamente por el usuario para evitar "sorpresas"
{
  lock_guard<mutex> lck(mtx_objetos);
  if(itr_highlight>=objetos.begin() && itr_highlight!=objetos.end())
    (*itr_highlight)->highlightear(false);
  if(itr_seleccionado>=objetos.begin() && itr_seleccionado!=objetos.end())
    (*itr_seleccionado)->seleccionar(false);
  sort(objetos.begin(),objetos.end(),[&](const unique_ptr<objeto>& a, const unique_ptr<objeto>& b) {return a->area() < b->area();});
  itr_seleccionado=objetos.end();
  itr_highlight=objetos.end();
  b_drag=false; //cuando hacias drag y suprimias terminabas con un dangling ptr
}

void destruir_objeto(int id)
{
  lock_guard<mutex> lck(mtx_objetos);
  auto encontrarItrId = [&]() -> vector<unique_ptr<objeto>>::iterator
  {
    for(auto itr=objetos.begin(); itr!=objetos.end(); ++itr)
      if((*itr)->id() == id) //si el id existe retornamos el iterador que apunta a este
        return itr;
    return objetos.end();
  };
  auto itr = encontrarItrId();
  if(itr != objetos.end())
  {
    objetos.erase(itr);
    itr_seleccionado=objetos.end();
    itr_highlight=objetos.end();
    string paq = "ro" + to_string(id);
    empujar_queue_saliente(paq);
  }
}

void destruir_objeto_seleccionado()
{
  lock_guard<mutex> lck(mtx_objetos);
  if(itr_seleccionado>=objetos.begin() && itr_seleccionado != objetos.end())
  {
    //cout << "\tobjetos.begin()\tobjetos.end()\titr_highlight\titr_seleccion\n";
    //cout << '\t' << &*objetos.begin() << '\t' << &*objetos.end() << '\t' << &*itr_highlight << '\t' << &*itr_seleccionado << '\n';
    string paq = "ro" + to_string((*itr_seleccionado)->id());
    objetos.erase(itr_seleccionado);
    itr_seleccionado=objetos.end();
    itr_highlight=objetos.end();
    empujar_queue_saliente(paq);
    b_drag=false; //cuando hacias drag y suprimias terminabas con un dangling ptr
  }
}
//-----------------------------------------------------------------------------------------------------------------

void objeto::dibujar_nombre(Mat& m) const
{
  Point entiende_esto = centro();
  Point pt = transformar(entiende_esto);
  putText(m, nombre(), pt, FONT_HERSHEY_PLAIN, tamanio_texto, COLOR_NEGRO, ancho_texto, CV_AA);
}

void rectangulo::dibujarse(Mat& m) const
{
  Point inicio, fin;
  inicio = transformar(inicio_); fin = transformar(fin_);
  rectangle(m, Rect(inicio, fin), color_, -2, CV_AA);
  rectangle(m, Rect(inicio, fin), COLOR_NEGRO, 1, CV_AA);
  //rectangle(m, Rect(Point(centro_.x - despl.x - 4, centro_.y - despl.y - 4), Size(8,8)),Scalar(150, 165, 250), 1, CV_AA);
  //fillConvexPoly(matriz, ps.data(), ps.size(), z.color());

  if(b_seleccionado_)
  {
    rectangle(m, Rect(inicio, fin), COLOR_SELECCION, 2, CV_AA); //selecci�n
    for(auto& p : puntos_clave_)
    {
      Point pc = transformar(p);
      rectangle(m, Rect(pc-offset_puntos_clave_, pc+offset_puntos_clave_), COLOR_BLANCO, 1, CV_AA );
    }
  }


  if(b_highlighteado_)
    rectangle(m, Rect(inicio, fin), COLOR_HIGHLIGHT_, 1, CV_AA); //highlight
}

//se llama continuamente cuando haces drag, no s�lo una vez
void rectangulo::arrastrar(const Point pt) //no es realmente un punto, sino una diferencia entre dos puntos. Debe ser absoluto
{
    fin_ += pt;
    inicio_ += pt;
    centro_ += pt;
    for(auto& p : puntos_clave_)
      p+=pt;
}

bool rectangulo::pertenece_a_area(const Point pt) const //pt debe ser absoluto, obtenido mediante p = g(p')
{
    return !((pt.x > inicio_.x && pt.x > fin_.x) || (pt.x < inicio_.x && pt.x < fin_.x) ||
        (pt.y > inicio_.y && pt.y > fin_.y) || (pt.y < inicio_.y && pt.y < fin_.y));
}

void rectangulo::imprimir_datos() const
{
  cout << nombre() << " : " << id() << '\t';
  cout << inicio_ << ", " << fin_ << '\n';
}

void circulo::dibujarse(Mat& m) const
{
  Point centro = transformar(centro_);
  int radio = transformar_escalar(radio_);
  cv::circle(m, centro, radio, color_, -2, CV_AA);
  cv::circle(m, centro, radio, COLOR_NEGRO, 1, CV_AA);

  if(b_seleccionado_)
    cv::circle(m, centro, radio, COLOR_SELECCION, 2, CV_AA);

  if(b_highlighteado_)
    cv::circle(m, centro, radio, COLOR_HIGHLIGHT_, 1, CV_AA);
}

void circulo::arrastrar(const Point pt)
{
  centro_ += pt;
}

bool circulo::pertenece_a_area(const Point pt) const //pt debe ser absoluto
{
  double x2 = double(pt.x - centro_.x)*double(pt.x - centro_.x);
  double y2 = double(pt.y - centro_.y)*double(pt.y - centro_.y);
  double r2 = double(radio_)*double(radio_);
  return (x2 + y2 < r2); //esto se hizo porque estaban overfloweando los valores de int
}

void circulo::imprimir_datos() const
{
  cout << nombre() << " : " << id() << '\t';
  cout << centro_ << ", " << radio_ << '\n';
}

void linea::dibujarse(Mat& m) const
{
  cv::line(m, transformar(inicio_), transformar(fin_), color_, 1, CV_AA);
}

void linea::arrastrar(const Point pt)
{
  inicio_ +=pt;
  fin_    +=pt;
}

bool linea::pertenece_a_area(const Point pt) const
{
  //no mames jaja, no est� tan f�cil
  return false;
}

void linea::imprimir_datos() const
{
  cout << nombre() << " : " << id() << '\t';
  cout << inicio_ << ", " << fin_ << '\n';
}

void cuadrado_isometrico::dibujarse(cv::Mat& m) const
{
  vector<Point> ps = puntos_desplazados();
  fillConvexPoly(m, ps.data(), ps.size(), color_);
  polylines(m, ps, true, COLOR_NEGRO, 1, CV_AA);
  if(b_highlighteado_)
    polylines(m, ps, true, COLOR_HIGHLIGHT_, 2, CV_AA);
  if(b_seleccionado_)
    polylines(m, ps, true, COLOR_SELECCION, 2, CV_AA);
}

void cuadrado_isometrico::arrastrar(const cv::Point pt)
{
  inicio_+=pt;
  fin_+=pt;
  for(auto& v : vertices_)
    v+=pt;
}

bool cuadrado_isometrico::pertenece_a_area(const cv::Point pt) const
{
  if(pointPolygonTest(vertices_, pt, false) > 0)
    return true;
  return false;
}

void cuadrado_isometrico::imprimir_datos() const
{
  cout << nombre() << " : " << id() << '\t';
  cout << inicio_ << ", " << fin_ << '\n';
}

/*bool objeto::es_esquina(const Point pt)
{
  return (pt.x <= fin_.x && pt.x > fin_.x-5 && pt.y <= fin_.y && pt.y > fin_.y-5); //XXXXXXXXXXXXXXXXXXXXX -5
}*/

/*void objeto::resizear(const Point pt)
{
  fin_ += pt;
  centro_ = Point(inicio_.x + (fin_.x - inicio_.x)/2, inicio_.y + (fin_.y - inicio_.y)/2);
}*/

objeto::~objeto()
{
    //cout << "soy el destructor de objetos[" << id_ << "]\n";
}

ostream& operator<<(ostream& os, objeto& o)
{
  auto pts = o.pts();
  return os << 'o' << o.id() << " " << pts.first.x << " " << pts.first.y << " " << pts.second.x << " " << pts.second.y;
}

//oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo
