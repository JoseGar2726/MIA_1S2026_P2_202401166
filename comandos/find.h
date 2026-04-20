#ifndef FIND_H
#define FIND_H

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>
#include <regex>
#include "../estructuras/sesion.h"
#include "../estructuras/structures.h"

class ComandoFind{
public:
    static std::string execute(const std::string& ruta, const std::string& name){
        if (!Sesion::activo) return "Error: sesion no iniciada";

        if (ruta.empty() || name.empty()) {
            return "Error: parametros -path y -name son obligatorios";
        }

        std::string rutaDisco = Sesion::rutaDisco;
        int inicioParticion = Sesion::inicioParticion;

        std::fstream file(rutaDisco, std::ios::in | std::ios::out | std::ios::binary);
        if(!file.is_open()) return "Error: no se pudo abrir el disco";

        Superblock sb;
        file.seekg(inicioParticion, std::ios::beg);
        file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));

        int inodoInicioId = rastrearRuta(file, sb, ruta);
        if (inodoInicioId == -1) {
            file.close();
            return "Error: la ruta origen '" + ruta + "' no existe";
        }

        std::string patronRegex = "^";
        for(char c : name){
            if(c == '*') patronRegex += ".+";
            else if(c == '?') patronRegex += ".";
            else if(c == '.') patronRegex += "\\.";
            else patronRegex += c;
        }
        patronRegex += '$';
        std::regex re(patronRegex);

        std::string resultados = "";

        buscarRecursivo(file, sb, inodoInicioId, ruta == "/" ? "" : ruta, re, resultados);

        file.close();

        if(resultados.empty()){
            return "No se encontraron resultados para '" + name + "' en la ruta '" + ruta + "'";
        }

        return "\n=== RESULTADOS DE BÚSQUEDA ===\n" + resultados + "==============================\n";
    }

private:
    static int rastrearRuta(std::fstream& file, const Superblock& sb, const std::string& ruta){
        if(ruta == "/") return 0;
        std::vector<std::string> carpetas;
        std::stringstream ss(ruta);
        std::string token;
        while(std::getline(ss, token, '/')){
            if(!token.empty()) carpetas.push_back(token);
        }
        int inodoActual = 0;
        for(const std::string& nombre : carpetas){
            inodoActual = buscarEnCarpeta(file, sb, inodoActual, nombre);
            if(inodoActual == -1) return -1;
        }
        return inodoActual;
    }

    static int buscarEnCarpeta(std::fstream& file, const Superblock& sb, int inodoId, const std::string& nombreBuscado){
        Inode inodoCarpeta;
        file.seekg(sb.s_inode_start + (inodoId * sizeof(Inode)), std::ios::beg);
        file.read(reinterpret_cast<char*>(&inodoCarpeta), sizeof(Inode));
        if (inodoCarpeta.i_type != '0') return -1;

        for(int i = 0; i < 12; i++){
            if(inodoCarpeta.i_block[i] != -1){
                FolderBlock fb;
                file.seekg(sb.s_block_start + (inodoCarpeta.i_block[i] * sizeof(FolderBlock)), std::ios::beg);
                file.read(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));
                for(int j = 0; j < 4; j++){
                    if(fb.b_content[j].b_inodo != -1){
                        char temp[13] = {0};
                        std::strncpy(temp, fb.b_content[j].b_name, 12);
                        if(std::string(temp) == nombreBuscado) return fb.b_content[j].b_inodo;
                    }
                }
            }
        }
        return -1;
    }

    static bool tienePermiso(const Inode& inodo, int permisoRequerido) {
        if (Sesion::usuario == "root") return true;
        int permPropietario = inodo.i_perm / 100;
        int permGrupo = (inodo.i_perm / 10) % 10;
        int permOtros = inodo.i_perm % 10;
        if (inodo.i_uid == Sesion::uid) return (permPropietario & permisoRequerido) == permisoRequerido;
        if (inodo.i_gid == Sesion::gid) return (permGrupo & permisoRequerido) == permisoRequerido;
        return (permOtros & permisoRequerido) == permisoRequerido;
    }

    static void buscarRecursivo(std::fstream& file, const Superblock& sb, int inodoId, const std::string& rutaActual, const std::regex& re, std::string& resultados){
        Inode inodo;
        file.seekg(sb.s_inode_start + (inodoId * sizeof(Inode)), std::ios::beg);
        file.read(reinterpret_cast<char*>(&inodo), sizeof(Inode));

        if (!tienePermiso(inodo, 4)) return;

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
                                std::string rutaHijo = rutaActual + "/" + nombreHijo;

                                if(std::regex_match(nombreHijo, re)){
                                    resultados += rutaHijo + "\n";
                                }

                                Inode inodoHijo;
                                file.seekg(sb.s_inode_start + (fb.b_content[j].b_inodo * sizeof(Inode)), std::ios::beg);
                                file.read(reinterpret_cast<char*>(&inodoHijo), sizeof(Inode));

                                if(inodoHijo.i_type == '0'){
                                    buscarRecursivo(file, sb, fb.b_content[j].b_inodo, rutaHijo, re, resultados);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

};

#endif