#include <iostream>
#include <chrono> //para profiles
#include <ctime> //para profiles
#include <fstream>
#include <algorithm>
#include <bitset> //borrame si no debuggeas
#include <future>
#include <thread>
#include <atomic>
#include <mutex>

#include <opencv2/opencv.hpp>

#include "elemento_diagrama.h" /**mjolnir no incluye su propio header?*/
#include "redes.h"
#include "zonas.hpp"

#include "gui.h"
#include "cuenta_nueva.h"

extern int paleta_colores();

using namespace std;
using namespace cv;

/* No intentar tus ideas es la forma m�s triste de no verlas tener �xito*/

const Scalar BLANCO(255,255,255);
const Scalar CAFE(0,51,102);
const Scalar GRIS(200,200,200);
const Scalar AZUL_PALIDO(240,200,200);
const Scalar Bckgnd(46,169,230);

int ancho_region; //w = 2dx
int altura_region; //h = 2dy

class zona;
extern vector<zona> zonas;
extern vector<zona> superzonas;
extern void rellenar_zona_telares();

Mat region;
Mat mat_panel;
Mat mat_header;

const Point HEADER0(5,20);
const Point HEADER1(100,20);
const Point HEADER2(300,20);
Point HEADER_MSG;

bool botonMouseIzquierdoAbajo=false; //flechas, drag, drag n drop
bool botonMouseDerechoAbajo=false; //panning
Point puntoClickMouseDerecho(0,0); //panning
Point puntoInicioDesplazamiento(0,0); //panning

bool b_dibujando_flecha; //flechas temporales
Point puntoInicioFlecha(0,0); //flechas temporales
Point puntoTerminoFlecha(0,0); //flechas temporales

bool b_dibujando_circulo;           //completar esta interfaz
Point puntoOrigenCirculo(0,0);

bool b_dibujando_objeto; //objeto temporal
Point puntoOrigenobjeto(0,0); //objeto temporal
Point puntoFinobjeto(0,0); //objeto temporal

Point puntoActualMouse(0,0); //se actualiza en cada evento del mouse

Point despl(10000,10000); //originalmente glb::desplazamientoOrigen

Point dxy; // (w/2, h/2)
int zoom(32);

std::string mensaje_derecho_superior;
std::mutex mtx_mensaje_derecho_superior;

std::string obtener_mensaje() /*Mensaje informativo en esquina superior derecha del diagrama*/
{
  lock_guard<mutex> lck(mtx_mensaje_derecho_superior);
  string s = mensaje_derecho_superior;
  return s;
}

void establecer_mensaje(std::string m) /*Mensaje informativo en esquina superior derecha del diagrama*/
{
  lock_guard<mutex> lck(mtx_mensaje_derecho_superior);
  mensaje_derecho_superior = m;
}

/**
La manera en la que se renderiza el diagrama es tomando los puntos absolutos (coordenadas) de los objetos o elementos a dibujar,
y aplic�ndoles una transformaci�n para poder hacer zoom out. Esta transformaci�n es dependiente del ancho del diagrama w, pues
al zoomoutear, la mitad del diagrama permanece en el centro.

p' = f(p) = dx + (p - dx - d)/z
dx = w/2, es decir el ancho de diagrama entre dos.
d = desplazamiento del origen
z es el factor zoom */

/**transforma los puntos absolutos a puntos relativos renderizables
p' = f(p) = dx + (p - dx - d)/z*/
cv::Point transformar(cv::Point& p)
{
  Point pp = dxy + (p - dxy - despl)/zoom; //dxy es la mitad del tama�o del diagrama
  return pp;
}

/**transforma un punto relativo a un punto absoluto
p = g(p') = z*(p' - dx) + dx + d */
cv::Point transformacion_inversa(cv::Point& pp)
{
  Point p = zoom*(pp - dxy) + dxy + despl;
  return p;
}

void efecto_cuadricula(cv::Mat& matriz)
{
  static vector<chrono::duration<double>> tiempos(100);
  static int cnt;
  int h = matriz.cols;
  int v = matriz.rows;
  Point pt1, pt2; //probablemente esto ayude a que la cach� est� caliente con este valor durante estos ciclos
  chrono::time_point<chrono::system_clock> t_inicial, t_final; //empezamos a medir tiempo
  t_inicial = chrono::system_clock::now();


  if(zoom<=32)
  {
    const int szlado = 800/zoom;
    //cout << "szlado == " << szlado << '\n';
    for(int i=szlado-(despl.x/zoom)%szlado; i<h; i+=szlado) //"generamos" un efecto de desplazamiento de la cuadr�cula
    {
      pt1.x=i, pt1.y=0; //Esto lo hacemos con la intenci�n de que no se creen Puntos temporales en cada iteraci�n
      pt2.x=i, pt2.y=v; //y que de esta manera las llamadas a malloc(si es que existen) desaparezcan
      line(matriz, pt1, pt2, BLANCO, 1, 4, 0);
      //line(matriz, Point(i,0), Point(i,v), BLANCO, 1, 4, 0); //cuadr�cula, vertical
    }

    for(int i=szlado-(despl.y/zoom)%szlado; i<v; i+=szlado) //Mat::cols es int, no uint
    {
      pt1.x=0, pt1.y=i;
      pt2.x=h, pt2.y=i;
      line(matriz, pt1, pt2, BLANCO, 1, 4, 0);
      //line(matriz, Point(0,i), Point(h,i), BLANCO, 1, 4, 0); //cuadr�cula, horizontal
    }
  }
  t_final = std::chrono::system_clock::now();
  chrono::duration<double> t_renderizar = t_final - t_inicial;
  tiempos[cnt] = t_renderizar;
  cnt++;
  if(cnt==100)
  {
    cnt--;
    chrono::duration<double> promedio;
    for(int i=0; i<cnt; ++i)
      promedio+=tiempos[i];
    promedio = promedio/cnt;
    cnt=0;
    //cout << "Promedio cuadricula: " << promedio.count() << "s\n";
  }

}

void establecer_resolucion(int& horizontal, int& vertical)
{
   RECT escritorio;
   const HWND hEscritorio = GetDesktopWindow();// obt�n un handle a la ventana del escritorio

   GetWindowRect(hEscritorio, &escritorio);// guarda el tama�o de la pantalla a la variable escritorio
   //la esquina superior izquierda tendr� las coordenadas (0,0)
   //La esquina inferior derecha tendr� coordenadas (horizontal,vertical)

   horizontal = (escritorio.right - 10);
   vertical = (escritorio.bottom - 60);
   dxy = cv::Point(horizontal/2, vertical/2);
}

void inicializar_diagrama()
{
  establecer_resolucion(ancho_region, altura_region);
  if(ancho_region < 10 || ancho_region > 10000 || altura_region < 10 || altura_region > 10000)
  {
    ancho_region = 1000; altura_region = 600;
  }
  region = Mat(altura_region, ancho_region, CV_8UC3, cv::Scalar(200,200,200));
  const int margen = region.cols - 200;
  mat_panel = Mat(region.colRange(margen, region.cols)); //mn
  mat_header = Mat(region.colRange(0, margen).rowRange(0,30)); //mn
  HEADER_MSG = Point(margen-200, HEADER0.y);

  rellenar_zona_telares();
}

//demasiados magic numbers
void renderizarDiagrama(Mat& matriz) //No hay pedo si tratamos de dibujar una regi�n que no pertenece a matriz
{
  chrono::time_point<chrono::system_clock> t_inicial, t_final; //empezamos a medir tiempo
  t_inicial = chrono::system_clock::now();

  Point pabs = transformacion_inversa(puntoActualMouse);

  matriz = Bckgnd;

  for(auto& zz : superzonas) //relleno de superzonas
  {
    vector<Point> ps = zz.puntos_desplazados();
    fillConvexPoly(matriz, ps.data(), ps.size(), zz.color());
    //polylines(matriz, ps.data(), ps.size(), 1, true, BLANCO);
    for(auto itr = ps.begin(); itr!=ps.end(); ++itr)
    {
      if(itr!=ps.begin())
        line(matriz,*itr,*(itr-1),CAFE);
      else
        line(matriz,*itr,*(ps.end()-1),CAFE);
    }
  }

  efecto_cuadricula(matriz);

  for(auto& z : zonas) //relleno de zonas
  {
    vector<Point> ps = z.puntos_desplazados();
    fillConvexPoly(matriz, ps.data(), ps.size(), z.color());
  }

  if(zoom<=64)
  {
    int sz = 4/zoom;
    if(sz==0)
      sz=1;
    int ancho=4-zoom;
    if(ancho<0)
      ancho = 1;
    for(auto it=zonas.begin(); it!=zonas.end(); ++it)
    {
      Point pc = it->centro();
      Point pt = transformar(pc);
      putText(matriz, it->nombre(), pt, FONT_HERSHEY_PLAIN, sz, Scalar(0,0,0), ancho, CV_AA);
    }
    for(auto& zz : superzonas)
    {
      auto aaa = zz.puntos_desplazados();
      putText(matriz, zz.nombre(), aaa[0], FONT_HERSHEY_PLAIN, sz, Scalar(0,0,0), ancho, CV_AA);
    }
  }


  if(b_dibujando_flecha) //dibujamos una flecha temporal
    arrowedLine(matriz, transformar(puntoInicioFlecha),
                transformar(puntoTerminoFlecha),
                COLOR_FLECHA_DIBUJANDO, 2, CV_AA);

  if(b_dibujando_objeto) //dibujamos un rect�ngulo temporal
  {
    rectangle(matriz, Rect(transformar(puntoOrigenobjeto), transformar(puntoFinobjeto)),
              COLOR_RECT_DIBUJANDO, 2, CV_AA);
  }

  //dibujamos todos los objetos
  for(auto& rec : glb::objetos)
    rec.second.dibujarse(matriz);
  //dibujamos todas las relaciones
  for(auto& rel : glb::relaciones)
    rel.second.dibujarse(matriz);

  mat_panel = AZUL_PALIDO;
  mat_header = BLANCO;

  if(glb::llave_objeto_seleccionado > 0)
    putText(matriz, std::string("Seleccionado: " + std::to_string(glb::llave_objeto_seleccionado)),
            HEADER2, FONT_HERSHEY_PLAIN, 1, Scalar(230,100,0), 1, CV_AA);

  string spabs = '(' + to_string(pabs.x) + ',' + to_string(pabs.y) + ')';
  putText(matriz, spabs, HEADER0, FONT_HERSHEY_PLAIN, 1, Scalar(230,100,0), 1, CV_AA); //PLAIN es m�s peque�a que SIMPLEX

  putText(matriz, obtener_mensaje(), HEADER_MSG, FONT_HERSHEY_PLAIN, 1, Scalar(100,0,255), 1, CV_AA);

  //string sprueba = "kanban urdido";
  //putText(matriz, sprueba, HEADER1, FONT_HERSHEY_PLAIN, 1, Scalar(230,100,0), 1, CV_AA);


  // medimos tiempo
  t_final = std::chrono::system_clock::now();
  chrono::duration<double> t_renderizar = t_final - t_inicial;
  //cout << t_renderizar.count() << "s\n";

  imshow("Mjolnir", matriz); //actualizamos el diagrama
}

void manejarInputTeclado(Mat& matriz, int k) //k no incluye ni ctrl, ni shift, ni alt por mencionar algunas
{
  //cout << k << "!\n"; //borrame si no debugeas, o com�ntame mejor!

  constexpr int DESPLAZAMIENTO = 1500;
  constexpr int TECLADO_FLECHA_ARRIBA = 2490368;
  constexpr int TECLADO_FLECHA_ABAJO = 2621440;
  constexpr int TECLADO_FLECHA_IZQUIERDA = 2424832;
  constexpr int TECLADO_FLECHA_DERECHA = 2555904;

  switch (k) {
  case TECLADO_FLECHA_ARRIBA:
    despl.y -= DESPLAZAMIENTO;
    break;
  case TECLADO_FLECHA_ABAJO:
    despl.y += DESPLAZAMIENTO;
    break;
  case TECLADO_FLECHA_DERECHA:
    despl.x += DESPLAZAMIENTO;
    break;
  case TECLADO_FLECHA_IZQUIERDA:
    despl.x -= DESPLAZAMIENTO;
    break;

  case 13: //tecla enter
    if(b_dibujando_objeto)
    {
      b_dibujando_objeto = false;
      crear_objeto(puntoOrigenobjeto, puntoFinobjeto); //deben ser p y no p'
    }
    break;

  case 43: //+ zoom in
    if(zoom!=1)
      zoom = zoom/2;
    break;
  case 45: //- zoom out
    if(zoom!=1024) //64 es razonable
      zoom = zoom*2;
    break;

  case 48:
    //empujar_queue_cntrl("reboot");
  case 50: //debug
    cout << "valor global: " << glb::llave_objeto_highlight << endl;
    push_funptr(&foo_gui);
    //push_funptr(&ventana_cuenta_nueva);
    break;
  case 51:
    establecer_mensaje("Cincuenta y uno");
    break;
  case 52:
    establecer_mensaje("Arbitrario");
    break;
  case 53:
    establecer_mensaje("");
    break;

  case 100: //d de debug
    cout << "obj sel: " << glb::llave_objeto_seleccionado << " obj hgl: " << glb::llave_objeto_highlight << endl;
    for(auto& ob : glb::objetos)
      cout << ob.first << "," << ob.second.id() << " | " << endl;
    for(auto& rel : glb::relaciones)
      cout << rel.first << "," << rel.second.id() << " | " << endl;
    cout << "\ntodas las relaciones:" << endl;
    for(auto& rel : glb::relaciones)
      cout << "relacion " << rel.second.id() << ": "
           << rel.second.get_objetos().first << ',' << rel.second.get_objetos().second << endl;
    if(glb::llave_objeto_seleccionado > 0)
    {
      cout << "relaciones del objeto " << glb::llave_objeto_seleccionado << ":" << endl;
      for(auto id : glb::objetos.at(glb::llave_objeto_seleccionado).get_relaciones())
        cout << id << endl;
    }
    break;

  case 103: //g de guardar
    guardar_todo();
    break;
  case 108: //l de load(cargar)
    cargar_todo();
    break;
  case 110: //m - cerrar redes
    iosvc.stop();
    break;
  case 112: //p - paleta de colores
    push_funptr(&paleta_colores);
    break;

  case 114: //r (ojo, R tiene su propia clave
    puntoOrigenobjeto = transformacion_inversa(puntoActualMouse); //convertimos p' en p
    puntoFinobjeto = puntoOrigenobjeto;
    b_dibujando_objeto = true;
    break;

  case 3014656: //suprimir, borrar objeto
    if(glb::llave_objeto_seleccionado > 0)
    {
      destruir_objeto(glb::llave_objeto_seleccionado);
      ubicacion::determinar_propiedades_ubicacion(puntoActualMouse); //para actualizar highlight
    }
    break;

  }
    //cout << glb::desplazamientoOrigen << endl;
}

/**Este callback se invoca cada vez que hay un evento de mouse en la ventana a la cual se attache� el callback.
  Podr�as tener varias ventanas con diferentes funciones que manejen el mouse
  La l�gica de este callback debe estar encapsulada, y debe ser legible*/
void manejarInputMouse(int event, int x, int y, int flags, void*)
{
  puntoActualMouse = cv::Point(x,y); //esta variable siempre lleva el rastro de d�nde est� el mouse
  //cout << (flags & CV_EVENT_FLAG_CTRLKEY) << endl;
  //cout << event << " " << flags << endl;

  if(event == CV_EVENT_RBUTTONDOWN) //panning
  {
    botonMouseDerechoAbajo = true;
    puntoClickMouseDerecho = puntoActualMouse;
    puntoInicioDesplazamiento = despl; //XXXXXXXXXXXXXXXX????????????
  }

  if(event == CV_EVENT_RBUTTONUP)
    botonMouseDerechoAbajo = false;

  if(event == CV_EVENT_LBUTTONDOWN)
  {
    /*Si est�bamos dibujando un objeto y dimos click, lo insertamos y no hacemos nada m�s*/
    if(b_dibujando_objeto)
    {
      b_dibujando_objeto = false;
      crear_objeto(puntoOrigenobjeto, puntoFinobjeto);

      /**solicitamos las propiedades del nuevo objeto a crear*/
      /***/
    }

    /*no est�bamos dibujando un objeto, evaluamos el punto y establecemos condiciones para la selecci�n y el arrastre*/
    else
    {
      botonMouseIzquierdoAbajo = true;
      auto props = ubicacion::determinar_propiedades_ubicacion(transformacion_inversa(puntoActualMouse));

      if(props.first > 0 ) //props.first es el id
      {
        if(glb::llave_objeto_seleccionado > 0) //si hab�a otro brother seleccionado antes...
          glb::objetos.at(glb::llave_objeto_seleccionado).seleccionar(false); //des-seleccionamos al anterior

        glb::objetos.at(props.first).seleccionar(true); //seleccionamos al brother
        glb::llave_objeto_seleccionado = props.first; //actualizamos al seleccionado

        if(flags & CV_EVENT_FLAG_CTRLKEY) //vamos a dibujar flecha, no a arrastrar
        {
          b_dibujando_flecha = true; //a�adir condici�n
          puntoInicioFlecha = transformacion_inversa(puntoActualMouse); //a�adir condici�n
          puntoTerminoFlecha = puntoInicioFlecha; //"reseteamos" la flecha;
        }
        else //de lo contrario, arrastramos o resizeamos. Usamos las mismas variables
        {
          glb::ptInicioArrastre = transformacion_inversa(puntoActualMouse);
          glb::ptFinArrastre = glb::ptInicioArrastre;
          b_dibujando_flecha = false;
          if(glb::objetos.at(props.first).es_esquina(transformacion_inversa(puntoActualMouse)))
          {
            glb::b_resize = true;
          }
          else
          {
            glb::b_drag = true;
          }
        }
        //hay espacio para alt y shift. Afortunadamente drag y dibujar flecha son mutuamente excluyentes
      }

      else if(glb::llave_objeto_seleccionado > 0) //no caimos en nadie, pero hab�a un brother seleccionado
      {
        glb::objetos.at(glb::llave_objeto_seleccionado).seleccionar(false); //lo des-seleccionamos
        glb::llave_objeto_seleccionado=-1; //y reseteamos el id de selecci�n
      }
    }
  }

  if(event == CV_EVENT_LBUTTONUP)
  {
    botonMouseIzquierdoAbajo = false;
    glb::b_drag = false; //terminan las condiciones de arrastre y resize
    glb::b_resize = false;

    if(b_dibujando_flecha) //esto se va a revampear
    {
      //flechas.push_back(flecha(puntoInicioFlecha, cv::Point(x,y) + despl));
      auto props = ubicacion::determinar_propiedades_ubicacion(transformacion_inversa(puntoActualMouse));
      if(props.first > 0 && props.first != glb::llave_objeto_seleccionado)
        glb::relaciones.emplace(relacion::sid - 1, relacion(glb::llave_objeto_seleccionado, props.first));

      b_dibujando_flecha = false;
    }
  }

  if(event == CV_EVENT_MOUSEMOVE)
  {
    if(botonMouseDerechoAbajo) //Panning. Movi�ndonos con click derecho apretado
    {
      despl = puntoInicioDesplazamiento + zoom*(puntoClickMouseDerecho - puntoActualMouse);
    }

    if(botonMouseIzquierdoAbajo) //Flechas. Dragging. Resizing. Moviendo el cursor con click izquierdo apretado.
    {
      //propiedades ubicacion, highlightear destino de flecha, posible drag n drop
      if(glb::b_drag)
      {
        Point pt = transformacion_inversa(puntoActualMouse);
        Point dif = pt - glb::ptInicioArrastre;
        glb::objetos.at(glb::llave_objeto_seleccionado).arrastrar(dif);
        glb::ptInicioArrastre = pt;
      }
      else if(glb::b_resize)
      {
        Point pt = transformacion_inversa(puntoActualMouse);
        Point dif = pt - glb::ptInicioArrastre;
        glb::objetos.at(glb::llave_objeto_seleccionado).resizear(dif);
        glb::ptInicioArrastre = pt;
      }
      //...

      if(b_dibujando_flecha)  //dibujando flecha temporal
      {
        /*props se va a usar despu�s para tener feedback con el highlight*/
        /*auto props = */ubicacion::determinar_propiedades_ubicacion(transformacion_inversa(puntoActualMouse)); //para highlightear el destino
        puntoTerminoFlecha = transformacion_inversa(puntoActualMouse); //la flecha es temporal, no se a�ade sino hasta que LBUTTONUP
      }
    }

    if(!botonMouseDerechoAbajo && !botonMouseIzquierdoAbajo)
      ubicacion::determinar_propiedades_ubicacion(transformacion_inversa(puntoActualMouse));

    if(b_dibujando_objeto)
      puntoFinobjeto = transformacion_inversa(puntoActualMouse);

  } //CV_EVENT_MOUSEMOVE
  if(event==CV_EVENT_LBUTTONDBLCLK)
  {
    cout << "DBL CLICK\n";
  }

} //manejarInputMouse
