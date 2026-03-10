#ifndef SESION_H
#define SESION_H

#include <string>

class Sesion{
public:
    static bool activo;
    static int uid;
    static int gid;
    static std::string usuario;
    static std::string idParticion;
    static std::string fitParticion;
    static std::string rutaDisco;
    static int inicioParticion;

    static void login(int _uid, int _gid, const std::string& _usuario,
        const std::string& _id, const std::string& _ruta,
        int _inicio){
            activo = true;
            uid = _uid;
            gid = _gid;
            usuario = _usuario;
            idParticion = _id;
            rutaDisco = _ruta;
            inicioParticion = _inicio;
        }
    
    static void logout(){
        activo = false;
        uid = -1;
        gid = -1;
        usuario = "";
        idParticion = "";
        rutaDisco = "";
        inicioParticion = -1;
    }
};

bool Sesion::activo = false;
int Sesion::uid = -1;
int Sesion::gid = -1;
std::string Sesion::usuario = "";
std::string Sesion::idParticion = "";
std::string Sesion::fitParticion = "";
std::string Sesion::rutaDisco = "";
int Sesion::inicioParticion = -1;

#endif
