#ifndef REGISTRAR_H
#define REGISTRAR_H

#include <cstring>
#include <ctime>
#include <fstream>
#include "../estructuras/structures.h"

namespace Registrar {

    static void escribirEnJournal(const std::string& rutaDisco, int inicioParticion, const std::string& operacion, const std::string& path, const std::string& contenido) {
        std::fstream file(rutaDisco, std::ios::in | std::ios::out | std::ios::binary);
        if (!file.is_open()) return;

        Superblock sb;
        file.seekg(inicioParticion, std::ios::beg);
        file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));

        if (sb.s_filesystem_type != 3) {
            file.close();
            return; 
        }

        Journal bitacora[50];
        file.seekg(inicioParticion + sizeof(Superblock), std::ios::beg);
        file.read(reinterpret_cast<char*>(&bitacora), 50 * sizeof(Journal));

        int indexVacio = -1;
        for (int i = 0; i < 50; i++) {
            if (bitacora[i].j_count == -1) {
                indexVacio = i;
                break;
            }
        }

        if (indexVacio == -1) {
            for (int i = 0; i < 49; i++) {
                bitacora[i] = bitacora[i + 1];
            }
            indexVacio = 49;
        }

        bitacora[indexVacio].j_count = indexVacio;
        std::strncpy(bitacora[indexVacio].j_content.i_operation, operacion.c_str(), 10);
        std::strncpy(bitacora[indexVacio].j_content.i_path, path.c_str(), 32);
        std::strncpy(bitacora[indexVacio].j_content.i_content, contenido.c_str(), 64);
        bitacora[indexVacio].j_content.i_date = static_cast<float>(time(nullptr));

        file.seekp(inicioParticion + sizeof(Superblock), std::ios::beg);
        file.write(reinterpret_cast<char*>(&bitacora), 50 * sizeof(Journal));
        
        file.close();
    }
}

#endif