#ifndef CHMOD_H
#define CHMOD_H

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>
#include "../estructuras/sesion.h"
#include "../estructuras/structures.h"
#include "registrar.h"

class ComandoChmod {
public:
    static std::string execute(const std::string& ruta, const std::string& ugoStr, bool recursivo){
        if(!Sesion::activo) return "Error: sesion no iniciada";

        if(ugoStr.length() != 3){
            return "Error: el parametro de permisos(ugo) debe tener exactamente 3 digitos";
        }

        int nuevoPermiso = 0;
        try{
            nuevoPermiso = std::stoi(ugoStr);
        } catch(...){
            return "Error: el parametro de permisos debe ser numerico";
        }

        std::string rutaDisco = Sesion::rutaDisco;
        int inicioParticion = Sesion::inicioParticion;

        std::fstream file(rutaDisco, std::ios::in | std::ios::out | std::ios::binary);
        if(!file.is_open()) return "Error: no se pudo abrir el disco";

        Superblock sb;
        file.seekg(inicioParticion, std::ios::beg);
        file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));

        int inodoObjetivoId = rastrearRuta(file, sb, ruta);
        if(inodoObjetivoId == -1){
            file.close();
            return "Error: la ruta objetivo '" + ruta + "' no existe";
        }

        Inode inodoObjetivo;
        file.seekg(sb.s_inode_start + (inodoObjetivoId * sizeof(Inode)), std::ios::beg);
        file.read(reinterpret_cast<char*>(&inodoObjetivo), sizeof(Inode));

        if(Sesion::usuario != "root" && inodoObjetivo.i_uid != Sesion::uid){
            file.close();
            return "Error: no puedes cambiar los permisos de un archivo que no te pertenece";
        }

        cambiarPermisos(file, sb, inodoObjetivoId, nuevoPermiso, recursivo);

        file.close();
        Registrar::escribirEnJournal(rutaDisco, inicioParticion, "chmod", ruta, ugoStr);

        return "Permisos cambiados exitosamente a '" + ugoStr + "' en la ruta '" + ruta + "'";
    }

private:
    static int rastrearRuta(std::fstream& file, const Superblock& sb, const std::string& ruta) {
        if (ruta == "/") return 0;
        std::vector<std::string> carpetas;
        std::stringstream ss(ruta);
        std::string token;
        while (std::getline(ss, token, '/')) {
            if (!token.empty()) carpetas.push_back(token);
        }

        int inodoActual = 0;
        for (const std::string& nombre : carpetas) {
            inodoActual = buscarEnCarpeta(file, sb, inodoActual, nombre);
            if (inodoActual == -1) return -1;
        }
        return inodoActual;
    }

    static int buscarEnCarpeta(std::fstream& file, const Superblock& sb, int inodoId, const std::string& nombreBuscado) {
        Inode inodoCarpeta;
        file.seekg(sb.s_inode_start + (inodoId * sizeof(Inode)), std::ios::beg);
        file.read(reinterpret_cast<char*>(&inodoCarpeta), sizeof(Inode));

        if (inodoCarpeta.i_type != '0') return -1;

        for (int i = 0; i < 12; i++) {
            if (inodoCarpeta.i_block[i] != -1) {
                FolderBlock fb;
                file.seekg(sb.s_block_start + (inodoCarpeta.i_block[i] * sizeof(FolderBlock)), std::ios::beg);
                file.read(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));

                for (int j = 0; j < 4; j++) {
                    if (fb.b_content[j].b_inodo != -1) {
                        char temp[13] = {0};
                        std::strncpy(temp, fb.b_content[j].b_name, 12);
                        if (std::string(temp) == nombreBuscado) {
                            return fb.b_content[j].b_inodo;
                        }
                    }
                }
            }
        }
        return -1;
    }

    static void cambiarPermisos(std::fstream& file, const Superblock& sb, int inodoId, int nuevoPermiso, bool recursivo){
        Inode inodo;
        file.seekg(sb.s_inode_start + (inodoId * sizeof(Inode)), std::ios::beg);
        file.read(reinterpret_cast<char*>(&inodo), sizeof(Inode));

        inodo.i_perm = nuevoPermiso;

        file.seekp(sb.s_inode_start + (inodoId * sizeof(Inode)), std::ios::beg);
        file.write(reinterpret_cast<char*>(&inodo), sizeof(Inode));
        
        if(recursivo && inodo.i_type == '0'){
            for(int i = 0; i < 12; i++){
                if(inodo.i_block[i] != -1){
                    FolderBlock fb;
                    file.seekg(sb.s_block_start + (inodo.i_block[i] * sizeof(FolderBlock)), std::ios::beg);
                    file.read(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));

                    for(int j = 0; j < 4; j++){
                        if(fb.b_content[j].b_inodo != -1){
                            char temp[13] = {0};
                            std::strncpy(temp, fb.b_content[j].b_name, 12);
                            std::string nombreHijo(temp);
                            if(nombreHijo != "." && nombreHijo != ".."){
                                cambiarPermisos(file, sb, fb.b_content[j].b_inodo, nuevoPermiso, recursivo);
                            }
                        }
                    }
                }
            }
        }
    }

};

#endif