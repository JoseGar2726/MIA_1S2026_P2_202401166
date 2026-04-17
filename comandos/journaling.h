#ifndef JORNALING_H
#define JORNALING_H

#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>
#include "../estructuras/structures.h"
#include "mount.h"

namespace ComandoJournaling{

    inline std::string formatearFecha(time_t fecha){
        std::string date = std::ctime(&fecha);
        date.erase(std::remove(date.begin(), date.end(), '\n'), date.end());
        return date;
    }

    inline std::string execute(const std::string& id){
        ComandoMount::ParticionMontada particion;
        if(!ComandoMount::getMountedPartition(id, particion)){
            return "Error: la particion con id '" + id + "' no esta montada";
        }

        std::fstream file(particion.ruta, std::ios::in | std::ios::binary);
        if(!file.is_open()) return "Error: No se pudo abrir el disco";

        Superblock sb;
        file.seekg(particion.inicio, std::ios::beg);
        file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));

        if(sb.s_filesystem_type != 3){
            file.close();
            return " Error: La particion no es EXT3. El Journaling solo esta disponible en sistemas EXT3.";
        }

        Journal bitacora[50];
        file.seekg(particion.inicio + sizeof(Superblock), std::ios::beg);
        file.read(reinterpret_cast<char*>(&bitacora), 50 * sizeof(Journal));
        file.close();

        std::ostringstream salida;
        salida << "\n=================================================================================\n";
        salida << "                           REPORTE DE JOURNALING (EXT3)                          \n";
        salida << "=================================================================================\n";
        salida << std::left << std::setw(12) << "OPERACION"
               << std::setw(30) << "PATH"
               << std::setw(20) << "CONTENIDO"
               << "FECHA\n";
        salida << "---------------------------------------------------------------------------------\n";

        bool hayDatos = false;
        for (int i = 0; i < 50; i++){
            if (bitacora[i].j_count != -1){
                hayDatos = true;
                std::string op = bitacora[i].j_content.i_operation;
                std::string pth = bitacora[i].j_content.i_path;
                std::string cont = bitacora[i].j_content.i_content;

                if (pth.length() > 28) pth = pth.substr(0, 25) + "...";
                if (cont.length() > 18) cont = cont.substr(0, 15) + "...";

                time_t fechaT = static_cast<time_t>(bitacora[i].j_content.i_date);

                salida << std::left << std::setw(12) << op
                       << std::setw(30) << pth
                       << std::setw(20) << cont
                       << formatearFecha(fechaT) << "\n";
            }
        }
        if (!hayDatos) {
            salida << "               No hay transacciones registradas en el Journaling.                \n";
        }
        salida << "=================================================================================\n";

        return salida.str();
    }
}

#endif