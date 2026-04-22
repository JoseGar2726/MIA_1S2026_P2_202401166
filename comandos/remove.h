#ifndef REMOVE_H
#define REMOVE_H

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>
#include "../estructuras/sesion.h"
#include "../estructuras/structures.h"
#include "registrar.h"

class ComandoRemove{
public:
    static std::string execute(const std::string& ruta){
        if(!Sesion::activo){
            return "Error: se requiere tener sesion iniciada para la ejecucion de este comando";
        }

        if(ruta == "/" || ruta.empty()){
            return "Error: no se puede eliminar el directorio raiz";
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

        std::string nombreObjetivo = carpetas.back();
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
                return "Error: la ruta no existe";
            }
        }

        Inode inodoPadre;
        file.seekg(sb.s_inode_start + (inodoPadreId * sizeof(Inode)), std::ios::beg);
        file.read(reinterpret_cast<char*>(&inodoPadre), sizeof(Inode));

        if(!tienePermiso(inodoPadre, 2)){
            file.close();
            return "Error: el usuario no tiene permisos para eliminar elementos en este directorio";
        }

        int inodoObjetivoId = buscarEnCarpeta(file, sb, inodoPadreId, nombreObjetivo);
        if(inodoObjetivoId == -1){
            file.close();
            return "Error: el archivo o carpeta '" + nombreObjetivo + "' no existe";
        }

        if (!verificarPermisosRecursivos(file, sb, inodoObjetivoId)) {
            file.close();
            return "Error: operacion abortada. Existen elementos internos sin permisos de escritura.";
        }

        liberarInodo(file, sb, inodoObjetivoId, inicioParticion);

        eliminarEntradaCarpeta(file, sb, inodoPadreId, nombreObjetivo);

        file.seekp(inicioParticion, std::ios::beg);
        file.write(reinterpret_cast<char*>(&sb), sizeof(Superblock));

        file.close();

        Registrar::escribirEnJournal(rutaDisco, inicioParticion, "remove", ruta, "Archivo Eliminado");

        return "Elemento '" + ruta + "' eliminado exitosamente.";
    }

private:
    static bool tienePermiso(const Inode& inodo, int permisoRequerido){
        if(Sesion::usuario == "root") return true;

        int permPropietario = inodo.i_perm / 100;
        int permGrupo = (inodo.i_perm / 10) % 10;
        int permOtros = inodo.i_perm % 10;

        if(inodo.i_uid == Sesion::uid) return (permPropietario & permisoRequerido) == permisoRequerido;
        if(inodo.i_gid == Sesion::gid) return (permGrupo & permisoRequerido) == permisoRequerido;
        return (permOtros & permisoRequerido) == permisoRequerido;
    }

    static  bool verificarPermisosRecursivos(std::fstream& file, const Superblock& sb, int inodoId){
        Inode inodo;
        file.seekg(sb.s_inode_start + (inodoId * sizeof(Inode)), std::ios::beg);
        file.read(reinterpret_cast<char*>(&inodo), sizeof(Inode));

        if(!tienePermiso(inodo, 2)) return false;

        if(inodo.i_type == '0'){
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
                                if(!verificarPermisosRecursivos(file, sb, fb.b_content[j].b_inodo)){
                                    return false;
                                }
                            }
                        }
                    }
                }
            }
        }
        return true;
    }

    static int buscarEnCarpeta(std::fstream& file, const Superblock& sb, int inodoId, const std::string& nombreBuscado){
        Inode inodoCarpeta;
        file.seekg(sb.s_inode_start + (inodoId * sizeof(Inode)), std::ios::beg);
        file.read(reinterpret_cast<char*>(&inodoCarpeta), sizeof(Inode));

        for(int i = 0; i < 12; i++){
            if(inodoCarpeta.i_block[i] != -1){
                FolderBlock fb;
                file.seekg(sb.s_block_start + (inodoCarpeta.i_block[i] * sizeof(FolderBlock)), std::ios::beg);
                file.read(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));

                for(int j = 0; j < 4; j++){
                    if(fb.b_content[j].b_inodo != -1){
                        char temp[13] = {0};
                        std::strncpy(temp, fb.b_content[j].b_name, 12);
                        std::string nombreActual(temp);
                        if(nombreActual == nombreBuscado){
                            return fb.b_content[j].b_inodo;
                        }
                    }
                }
            }
        }
        return -1;
    }

    static void liberarInodo(std::fstream& file, Superblock& sb, int inodoId, int inicioParticion){
        Inode inodo;
        file.seekg(sb.s_inode_start + (inodoId * sizeof(Inode)), std::ios::beg);
        file.read(reinterpret_cast<char*>(&inodo), sizeof(Inode));

        for(int i = 0; i < 12; i++){
            if(inodo.i_block[i] != -1){
                if(inodo.i_type == '0'){
                    FolderBlock fb;
                    file.seekg(sb.s_block_start + (inodo.i_block[i] * sizeof(FolderBlock)), std::ios::beg);
                    file.read(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));

                    for(int j = 0; j < 4; j++){
                        if(fb.b_content[j].b_inodo != -1){
                            char temp[13] = {0};
                            std::strncpy(temp, fb.b_content[j].b_name, 12);
                            std::string nombreHijo(temp);

                            if(nombreHijo != "." && nombreHijo != ".."){
                                liberarInodo(file, sb, fb.b_content[j].b_inodo, inicioParticion);
                            }
                        }
                    }
                }
                char cero = '0';
                file.seekp(sb.s_bm_block_start + inodo.i_block[i], std::ios::beg);
                file.write(&cero, 1);
                sb.s_free_blocks_count++;
            }
        }
        char cero = '0';
        file.seekp(sb.s_bm_inode_start + inodoId, std::ios::beg);
        file.write(&cero, 1);
        sb.s_free_inodes_count++;
    }

    static void eliminarEntradaCarpeta(std::fstream& file, const Superblock& sb, int inodoPadreId, const std::string& nombreObjetivo){
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
                        std::string nombreActual(temp);

                        if(nombreActual == nombreObjetivo){
                            fb.b_content[j].b_inodo = -1;
                            memset(fb.b_content[j].b_name, 0, 12);
                            modificado = true;
                            break;
                        }
                    }
                }

                if(modificado){
                    file.seekp(sb.s_block_start + (inodoPadre.i_block[i] * sizeof(FolderBlock)), std::ios::beg);
                    file.write(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));
                    return;
                }
            }
        }
    }
};

#endif