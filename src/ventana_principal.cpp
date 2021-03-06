#include "ventana_principal.h"
#include "mjolnir.hpp"
#include "../recurso.h"
#include "commctrl.h"
#include "postgres_funciones.h"
#include "ficha_tecnica.h"
#include "commdlg.h"
#include "wingdi.h"
#include "sync.h"
//#include "zonas.hpp"

#include <stdexcept>
#include <utility>

using cv::Mat; using cv::Scalar; using cv::Point;
using namespace std;
using namespace std::placeholders;

RECT ventana::rEscritorio;
const char * ventana::sigh="hmmmmmm";

extern void procesar_queue_cntrl();

static WNDCLASSEX wc;
HINSTANCE ventana::instancia_programa_;


LRESULT CALLBACK ventana::_stWndProc_(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  if (msg == WM_NCCREATE)
    SetWindowLong(hwnd, GWLP_USERDATA, (long long)((LPCREATESTRUCT(lParam))->lpCreateParams));

  ventana* v = ventanaDeHwnd(hwnd);
  return v->WndProc(hwnd, msg, wParam, lParam);
}

void ventana::registrarClase()
{
  wc.cbSize        = sizeof(WNDCLASSEX);
  wc.style         = CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS;
  wc.lpfnWndProc   = ventana::_stWndProc_;
  wc.cbClsExtra    = 0;
  wc.cbWndExtra    = 0;
  wc.hInstance     = ventana::instancia_programa_;
  wc.hIcon         = /*LoadIcon(NULL, IDI_APPLICATION);*/LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MJOLNIR));
  wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
  wc.lpszMenuName  = MAKEINTRESOURCE(IDR_MYMENU);
  wc.lpszClassName = ventana::sigh;
  wc.hIconSm       = /*LoadIcon(NULL, IDI_APPLICATION);*/(HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MJOLNIR), IMAGE_ICON, 16, 16, 0);

  if(!RegisterClassEx(&wc))
      throw std::runtime_error("Error en registrar la clase de la ventana!");
}


void ventana::crearVentana()
{
  hwnd_ = CreateWindowEx(
      WS_EX_CONTEXTHELP,
      ventana::sigh,
      nombre_,
      WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, CW_USEDEFAULT, rEscritorio.right, rEscritorio.bottom,
      NULL, NULL, ventana::instancia_programa_, (void*)this);

  if(hwnd_ == NULL)
  {
    throw std::runtime_error("Error creando ventana");
  }

  cout << "hwnd en crear: " << hwnd_ << '\n';
  mover(0, 0, ventana::rEscritorio.right, ventana::rEscritorio.bottom-50); //demasiada magia
}

void ventana::inicializar_diagrama()
{
  RECT rVentana;                              //es para guardar el tama�o de la ventana principal temporalmente
  GetWindowRect(hwnd_, &rVentana);             //guarda el tama�o de la ventana principal
  auto ancho_region = (rVentana.right - 200);      //hacemos al diagrama ligeramente m�s delgado que la ventana principal
  auto altura_region = (rVentana.bottom);
  mjol_->dxy = Point(ancho_region/2, ancho_region/2);

  mjol_->diagrama_ = Mat(altura_region, ancho_region, CV_8UC4, Scalar(200,200,200));  //instanciamos la matriz del diagrama principal
  mjol_->encabezado_ = Mat(mjol_->diagrama_.colRange(0, mjol_->diagrama_.cols).rowRange(0,30)); //instanciamos la submatriz del header
  mjol_->HEADER_MSG = Point(mjol_->diagrama_.cols-300, mjol_->HEADER0.y);                   //instanciamos el lugar donde ir�n los mensajes de diagrama

  zonas_fabrica(mjol_.get());
  rellenar_zona_telares(mjol_.get());
}

void ventana::configuramos_parametros_diagrama()
{
  cv::namedWindow(nombre_, CV_WINDOW_AUTOSIZE);
  HWND hWnd2 = (HWND) cvGetWindowHandle(nombre_);
  HWND hDiagrama = ::GetParent(hWnd2);
  ::SetParent(hDiagrama, hwnd_);
  //EnableMenuItem(GetSystemMenu(hDiagrama, FALSE), SC_CLOSE, MF_BYCOMMAND | MF_DISABLED);
  /***************/
  LONG lStyle = GetWindowLong(hDiagrama, GWL_STYLE);
  lStyle &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZE | WS_MAXIMIZE | WS_SYSMENU);
  SetWindowLong(hDiagrama, GWL_STYLE, lStyle);
  /***************/
  RECT sz_dia;
  GetWindowRect(hDiagrama, &sz_dia);// guarda el tama�o de la pantalla a la variable escritorio
  SetWindowPos(hDiagrama, 0, 0, 0, sz_dia.right, sz_dia.bottom, SWP_NOSIZE);
  //mover( 0, -20, sz_dia.right, sz_dia.bottom);
  SendMessage(hDiagrama, WM_SETICON, ICON_BIG, IDI_MJOLNIR);
}

LRESULT CALLBACK ventana::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch(msg)
  {
  case WM_CREATE:
    cout << "creando?";
    break;

  case WM_COMMAND:
    switch(LOWORD(wParam))
    {
    case ID_FILE_EXIT:
      DestroyWindow(hwnd);
      break;

    case ID_PB1:
    {
      //LPWSTR buffer[256];
      //SendMessage(hEdit, WM_GETTEXT, sizeof(buffer)/sizeof(buffer[0]), reinterpret_cast<LPARAM>(buffer));
      MessageBox(NULL, (LPCSTR)"dise�ame", "Click boton", MB_ICONINFORMATION);
      break;
    }
    case ID_ACCION1:
    {
      //LPWSTR buffer[256];
      //SendMessage(hEdit, WM_GETTEXT, sizeof(buffer)/sizeof(buffer[0]), reinterpret_cast<LPARAM>(buffer));
      MessageBox(NULL, (LPCSTR)/*buffer*/"accion1", "Informacion!", MB_ICONINFORMATION);
      break;
    }
    case IDM_CREAR_FICHA:
    {
      //dialogo_ficha_tecnica(hwnd);
      break;
    }

    case IDM_CFG_COLORES:
    {
      COLORREF ccref[16];
      COLORREF csel=0x000000;
      CHOOSECOLOR c; ZeroMemory(&c, sizeof(CHOOSECOLOR)); c.lStructSize = sizeof(CHOOSECOLOR);
      c.Flags=CC_FULLOPEN|CC_RGBINIT; c.hwndOwner=hwnd; c.lpCustColors=ccref; c.rgbResult=csel;
      if(ChooseColor(&c))
        mjol_->bckgnd_ = Scalar(GetBValue(c.rgbResult), GetGValue(c.rgbResult), GetRValue(c.rgbResult) );
      break;
    }

    }
	break;

  case WM_TIMER:
    switch (wParam)
    {
      case ID_T30:
        if(!mjol_->b_cache_valida)
        {
          mjol_->renderizarDiagrama(); //actualizamos el contenido de la matriz
          mjol_->b_cache_valida = true;
        }
        procesar_queue_cntrl();
        db::checar_input_db();
        break;

      /*case ID_T1000:
        if(sync::b_sync_cambio)
        {
          for(sync* p : sync::set_modificados)
            p->actualizar_db();
          sync::set_modificados.clear();
          sync::b_sync_cambio = false;
        }
        break;*/
    }
    break;

  case WM_CLOSE:
    DestroyWindow(hwnd);
    break;

  case WM_DESTROY:
    PostQuitMessage(0);
    break;
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

void alerta_cierre_programa(string msg)
{
  MessageBox(NULL, msg.c_str(), "Cerrando programa", MB_OK);
  exit(0);
}
