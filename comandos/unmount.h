#ifndef UNMOUNT_H
#define UNMOUNT_H

#include <string>
#include "mount.h"
#include "../estructuras/sesion.h"

namespace ComandoUnmount{
    static std::string execute(const std::string& id){
        if(id.empty()){
            return "Error: el comando unmount requiere el parametro obligatorio -id";
        }

        if (Sesion::activo && Sesion::idParticion == id) {
            return "Error: No se puede desmontar la particion '" + id + "' porque el usuario '" + 
                   Sesion::usuario + "' tiene una sesion activa en ella. Ejecute logout primero.";
        }

        if (ComandoMount::unmount(id)) {
            return "Exito: Particion con ID '" + id + "' desmontada correctamente del sistema.";
        } else {
            return "Error: La particion con ID '" + id + "' no existe o ya estaba desmontada.";
        }

    }
};

#endif