#include <windows.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <mutex>
#include <string>

#include "elemento_diagrama.h"
#include "utilidades.hpp"

extern void empujar_queue_saliente(std::string s); /*No quiero incluir redes.h*/

using namespace std;

std::mutex mtx_objetos;

int objeto::sid = 1; //hasta donde sabes debe definirse fuera de la clase, y no en el header
int relacion::sid = 1;

void crear_objeto(cv::Point& p1, cv::Point& p2)
{
  std::lock_guard<mutex> lck(mtx_objetos);
  char k = 0xfe;
  string paq = k + to_string(p1.x);
  glb::objetos.emplace(objeto::sid - 1, objeto(p1, p2)); //porque -1?
  empujar_queue_saliente(paq); //dentro de una funci�n lockeada llamas a otra que usa locks. aguas
}

void destruir_objeto(int id)
{
  std::lock_guard<mutex> lck(mtx_objetos);
  glb::objetos.erase(glb::llave_objeto_seleccionado);
  glb::llave_objeto_seleccionado = -1;
  glb::llave_objeto_highlight = -1;
  string paq = "mo" + to_string(id);
  empujar_queue_saliente(paq);
}

flecha::flecha(int llave_origen, int llave_destino, std::map<int, objeto>& contenedor)
{
    inicio_ = contenedor.at(llave_origen).centro_;
    fin_ = contenedor.at(llave_destino).centro_;
    centro_ = cv::Point(inicio_.x + (fin_.x - inicio_.x)/2, inicio_.y + (fin_.y - inicio_.y)/2);
    b_seleccionado_ = false;
}


void flecha::dibujarse(cv::Mat& m) //deber�amos inlinear?
{
    arrowedLine(m, transformar(inicio_), transformar(fin_), cv::Scalar(205,155,25), 2, CV_AA);
}

//-----------------------------------------------------------------------------------------------------------------

void objeto::dibujarse(cv::Mat& m)
{
  cv::Point iniciodespl, findespl;
  iniciodespl = transformar(inicio_); findespl = transformar(fin_);
  cv::rectangle(m, cv::Rect(iniciodespl, findespl), color_, 2, CV_AA);
  //cv::rectangle(m, cv::Rect(cv::Point(centro_.x - despl.x - 4, centro_.y - despl.y - 4), cv::Size(8,8)),cv::Scalar(150, 165, 250), 1, CV_AA);


  //fillConvexPoly(matriz, ps.data(), ps.size(), z.color());

  if(b_seleccionado_)
    cv::rectangle(m, cv::Rect(iniciodespl, findespl), COLOR_SELECCION, 3, CV_AA); //selecci�n

  if(b_highlighteado_)
    cv::rectangle(m, cv::Rect(iniciodespl, findespl), COLOR_HIGHLIGHT_, 1, CV_AA); //highlight
}

bool objeto::pertenece_a_area(const cv::Point pt) //pt debe ser absoluto, obtenido mediante p = g(p')
{
    return !((pt.x > inicio_.x && pt.x > fin_.x) || (pt.x < inicio_.x && pt.x < fin_.x) ||
        (pt.y > inicio_.y && pt.y > fin_.y) || (pt.y < inicio_.y && pt.y < fin_.y));
}

bool objeto::es_esquina(const cv::Point pt)
{
  return (pt.x <= fin_.x && pt.x > fin_.x-5 && pt.y <= fin_.y && pt.y > fin_.y-5); //XXXXXXXXXXXXXXXXXXXXX -5
}

void objeto::imprimir_datos()
{
    /*...*/
}

//se llama continuamente cuando haces drag, no s�lo una vez
void objeto::arrastrar(const cv::Point pt) //no es realmente un punto, sino una diferencia entre dos puntos. Debe ser absoluto
{
    fin_ += pt;
    inicio_ += pt;
    centro_ += pt;
    notificar(Notificacion::Movimiento);
}

void objeto::resizear(const cv::Point pt)
{
  fin_ += pt;
  centro_ = cv::Point(inicio_.x + (fin_.x - inicio_.x)/2, inicio_.y + (fin_.y - inicio_.y)/2);
  notificar(Notificacion::Movimiento);
}

void objeto::notificar(Notificacion noti)
{
  using namespace std;

  if(!relaciones_.empty())
    for(auto& id_rel : relaciones_) //para toda relaci�n del objeto...
    {
      try
      {
        //std::cout << " | obj " << id_  << " trato de notificar a rel " << id_rel << std::endl;
        glb::relaciones.at(id_rel).recibir_notificacion(id_, noti); //les env�a una notificaci�n
      }
      catch(std::out_of_range e) {cout << "Fuera de rango"<< endl;}
    }
}

void objeto::recibir_notificacion(int id_rel, Notificacion noti)
{
  if(Notificacion::Destruccion == noti) //borramos el indice de esa notificacion
  {
    auto itr = find(objeto::relaciones_.begin(), objeto::relaciones_.end(), id_rel);
    objeto::relaciones_.erase(itr);
  }
}

objeto::~objeto() //luego lo optimizas
{
    std::cout << "soy el destructor de objetos[" << id_ << "]\n";
    notificar(Notificacion::Destruccion); //notifica a todas sus relaciones que ser� destruido
    for(auto& id : relaciones_)
        glb::relaciones.erase(id); //y luego las borra
}

std::ostream& operator<<(std::ostream& os, objeto& o)
{
  auto pts = o.pts();
  return os << 'o' << o.id() << " " << pts.first.x << " " << pts.first.y << " " << pts.second.x << " " << pts.second.y;
}

//oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo

void relacion::notificar(int id, Notificacion noti)
{
    glb::objetos.at(id).recibir_notificacion(id_, noti);
}

void relacion::recibir_notificacion(int id, Notificacion noti)
{
    //std::cout << " | rel " << id_  << " recibi notificacion " << " de obj " << id << std::endl;
    if(Notificacion::Movimiento == noti)
    {
        if(id == ids_.first)
            pt1_ = glb::objetos.at(id).centro();
        else if(id == ids_.second)
            pt2_ = glb::objetos.at(id).centro();
    }
    else if(Notificacion::Destruccion == noti)
    {
        //El stack debe desenvolverse por lo que el que notific� de destrucci�n debe ser el que se haga responsable de la relacion
        if(id == ids_.first)
            notificar(ids_.second, Notificacion::Destruccion); //el objeto que reciba esto simplemente borra el id de su vector
        else if(id == ids_.second)
            notificar(ids_.first, Notificacion::Destruccion);
    }
}

void relacion::dibujarse(cv::Mat& m)
{
    line(m, transformar(pt1_), transformar(pt2_), cv::Scalar(205,155,125), 2, CV_AA);
}

std::ostream& operator<<(std::ostream& os, relacion& r)
{
  auto ids = r.get_objetos();
  return os << 'r' << r.id() << " " << ids.first << " " << ids.second;
}

//rrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr

//BORRAR - probablemente
void guardar_todo()
{
  using namespace std;
  ofstream ofs("objetos.txt");
  cout << "guardando:" << endl;
  for(auto& o : glb::objetos)
  {
    ofs << o.second << endl;
    cout << o.second << endl;
  }

  for(auto& r : glb::relaciones)
  {
    ofs << r.second << endl;
    cout << r.second << endl;
  }
}

//BORRAR - probablemente
void cargar_todo()
{
  using namespace std;
  ifstream ifs("objetos.txt");
  string s;
  while(getline(ifs, s))
  {
    stringstream ss(s);
    string tipo;
    ss >> tipo;
    if(tipo[0] == 'o')
    {
      int x1, x2, y1, y2;
      ss >> x1; ss >> y1; ss >> x2; ss >> y2; //se siente muy ineficiente
      glb::objetos.emplace(objeto::sid - 1, objeto(cv::Point(x1,y1), cv::Point(x2,y2)));
    }
    else if(tipo[0] == 'r')
    {
      int id1, id2;
      ss >> id1; ss >> id2;
      glb::relaciones.emplace(relacion::sid - 1, relacion(id1, id2));
    }
  }
}
