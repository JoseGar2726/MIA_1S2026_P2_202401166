#ifndef RENAME_H
#define RENAME_H

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>
#include "../estructuras/sesion.h"
#include "../estructuras/structures.h"
#include "registrar.h"

class ComandoRename{
public:
    static std::string execute(const std::string& ruta, const std::string& nuevoNombre){
        if(!Sesion::activo){
            return "Error: se requiere tener sesion iniciada para la ejecucion de este comando";
        }

        if(ruta == "/" || ruta.empty()){
            return "Error: no se puede renombrar la raiz";
        }

        if (nuevoNombre.length() > 12) {
            return "Error: el nuevo nombre excede los 12 caracteres permitidos";
        }

        std::string rutaDisco = Sesion::rutaDisco;
        int inicioParticion = Sesion::inicioParticion;

        std::vector<std::string> carpetas;
        std::stringstream ss(ruta);
        std::string token;
        while(std::getline(ss, token, '/')){
            if(!token.empty()) carpetas.push_back(token);
        }

        if(carpetas.empty()) return "Error: ruta invalida";

        std::string nombreActual = carpetas.back();
        carpetas.pop_back();

        std::fstream file(rutaDisco, std::ios::in | std::ios::out | std::ios::binary);
        if(!file.is_open()) return "Error: no se pudo abrir el disco";

        Superblock sb;
        file.seekg(inicioParticion, std::ios::beg);
        file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));

        int inodoPadreId = 0;

        for(const std::string& nombreCarpeta : carpetas){
            inodoPadreId = buscarEnCarpeta(file, sb, inodoPadreId, nombreCarpeta);
            if(inodoPadreId == -1){
                file.close();
                return "Error: la ruta padre no existe";
            }
        }

        Inode inodoPadre;
        file.seekg(sb.s_inode_start + (inodoPadreId * sizeof(Inode)), std::ios::beg);
        file.read(reinterpret_cast<char*>(&inodoPadre), sizeof(Inode));

        if(!tienePermiso(inodoPadre, 2)){
            file.close();
            return "Error: el usuario no tiene permisos para renombrar elementos en este directorio";
        }

        int inodoObjetivoId = buscarEnCarpeta(file, sb, inodoPadreId, nombreActual);
        if(inodoObjetivoId == -1){
            file.close();
            return "Error: el elemento '" + nombreActual + "' no existe";
        }

        Inode inodoObjetivo;
        file.seekg(sb.s_inode_start + (inodoObjetivoId * sizeof(Inode)), std::ios::beg);
        file.read(reinterpret_cast<char*>(&inodoObjetivo), sizeof(Inode));

        if(!tienePermiso(inodoObjetivo, 2)){
            file.close();
            return "Error: el usuario no tiene permisos de escritura sobre '" + nombreActual + "' para renombrarlo";
        }

        if(buscarEnCarpeta(file, sb, inodoPadreId, nombreActual) == -1){
            file.close();
            return "Error: el elemento '" + nombreActual + "' no existe";
        }

        if(buscarEnCarpeta(file, sb, inodoPadreId, nuevoNombre) != -1){
            file.close();
            return "Error: ya existe un elemento con el nombre '" + nuevoNombre + "' en esta ruta";
        }

        if(!actualizarNombreEnCarpeta(file, sb, inodoPadreId, nombreActual, nuevoNombre)){
            file.close();
            return "Error: fallo interno al intentar renombrar";
        }

        file.close();

        Registrar::escribirEnJournal(rutaDisco, inicioParticion, "rename", ruta, nuevoNombre);

        return "Elemento renombrado exitosamente a '" + nuevoNombre + "'";
    }

private:

    static bool tienePermiso(const Inode& inodo, int permisoRequerido) {
        if (Sesion::usuario == "root") return true;
        int permPropietario = inodo.i_perm / 100;
        int permGrupo = (inodo.i_perm / 10) % 10;
        int permOtros = inodo.i_perm % 10;

        if (inodo.i_uid == Sesion::uid) return (permPropietario & permisoRequerido) == permisoRequerido;
        if (inodo.i_gid == Sesion::gid) return (permGrupo & permisoRequerido) == permisoRequerido;
        return (permOtros & permisoRequerido) == permisoRequerido;
    }

    static int buscarEnCarpeta(std::fstream& file, const Superblock& sb, int inodoId, const std::string& nombreBuscado) {
        Inode inodoCarpeta;
        file.seekg(sb.s_inode_start + (inodoId * sizeof(Inode)), std::ios::beg);
        file.read(reinterpret_cast<char*>(&inodoCarpeta), sizeof(Inode));

        for (int i = 0; i < 12; i++) {
            if (inodoCarpeta.i_block[i] != -1) {
                FolderBlock fb;
                file.seekg(sb.s_block_start + (inodoCarpeta.i_block[i] * sizeof(FolderBlock)), std::ios::beg);
                file.read(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));

                for (int j = 0; j < 4; j++) {
                    if (fb.b_content[j].b_inodo != -1) {
                        char temp[13] = {0};
                        std::strncpy(temp, fb.b_content[j].b_name, 12);
                        std::string nombreActual(temp);
                        if (nombreActual == nombreBuscado) {
                            return fb.b_content[j].b_inodo;
                        }
                    }
                }
            }
        }
        return -1;
    }

    static bool actualizarNombreEnCarpeta(std::fstream& file, const Superblock& sb, int inodoPadreId, const std::string& nombreActual, const std::string& nuevoNombre){
        Inode inodoPadre;
        file.seekg(sb.s_inode_start + (inodoPadreId * sizeof(Inode)), std::ios::beg);
        file.read(reinterpret_cast<char*>(&inodoPadre), sizeof(Inode));

        for(int i = 0; i < 12; i++){
            if(inodoPadre.i_block[i] != -1){
                FolderBlock fb;
                file.seekg(sb.s_block_start + (inodoPadre.i_block[i] * sizeof(FolderBlock)), std::ios::beg);
                file.read(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));

                bool modificado = false;
                for(int j = 0; j < 4; j++){
                    if(fb.b_content[j].b_inodo != -1){
                        char temp[13] = {0};
                        std::strncpy(temp, fb.b_content[j].b_name, 12);
                        std::string nombreEnBloque(temp);

                        if(nombreEnBloque == nombreActual){
                            memset(fb.b_content[j].b_name, 0, 12);
                            std::strncpy(fb.b_content[j].b_name, nuevoNombre.c_str(), 12);
                            modificado = true;
                            break;
                        }
                    }
                }

                if(modificado){
                    file.seekp(sb.s_block_start + (inodoPadre.i_block[i] * sizeof(FolderBlock)), std::ios::beg);
                    file.write(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));
                    return true;
                }
            } 
        }
        return false;
    }
};

#endif