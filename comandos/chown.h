#ifndef CHOWN_H
#define CHOWN_H

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>
#include "../estructuras/sesion.h"
#include "../estructuras/structures.h"
#include "registrar.h"

class ComandoChown{
public:
    static std::string execute(const std::string& ruta, const std::string& usuario, bool recursivo){
        if(!Sesion::activo) return "Error: sesion no iniciada";



        std::string rutaDisco = Sesion::rutaDisco;
        int inicioParticion = Sesion::inicioParticion;

        std::fstream file(rutaDisco, std::ios::in | std::ios::out | std::ios::binary);
        if(!file.is_open()) return "Error: no se pudo abrir el diso";

        Superblock sb;
        file.seekg(inicioParticion, std::ios::beg);
        file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));

        int nuevoUID = obtenerUIDUsuario(file, sb, usuario);
        if(nuevoUID == -1){
            file.close();
            return "Error: el usuario '" + usuario + "' no existe en el sistema";
        }

        int inodoObjetivoId = rastrearRuta(file, sb, ruta);
        if(inodoObjetivoId == -1){
            file.close();
            return "Error: la ruta objetivo '" + ruta + "' no existe";
        }

        Inode inodoObjetivo;
        file.seekg(sb.s_inode_start + (inodoObjetivoId * sizeof(Inode)), std::ios::beg);
        file.read(reinterpret_cast<char*>(&inodoObjetivo), sizeof(Inode));

        if (Sesion::usuario != "root" && inodoObjetivo.i_uid != Sesion::uid) {
            file.close();
            return "Error: no puedes cambiar el propietario de un archivo que no te pertenece";
        }

        cambiarPropietario(file, sb, inodoObjetivoId, nuevoUID, recursivo);

        file.close();

        Registrar::escribirEnJournal(rutaDisco, inicioParticion, "chown", ruta, usuario);

        return "Propietario cambiado exitosamente a '" + usuario + "' en la ruta '" + ruta + "'";
    }

private:
    static int obtenerUIDUsuario(std::fstream& file, const Superblock& sb, const std::string& nombreBuscado){
        int inodoUsersId = rastrearRuta(file, sb, "/users.txt");
        if(inodoUsersId == -1) {
            std::cout << "-> DEBUG: No se encontro el archivo /users.txt" << std::endl;
            return -1;
        }

        Inode inodoUsers;
        file.seekg(sb.s_inode_start + (inodoUsersId * sizeof(Inode)), std::ios::beg);
        file.read(reinterpret_cast<char*>(&inodoUsers), sizeof(Inode));
        
        std::string contenido = "";
        for(int i = 0; i < 12; i++){
            if(inodoUsers.i_block[i] != -1){
                FileBlock fb;
                file.seekg(sb.s_block_start + (inodoUsers.i_block[i] * sizeof(FileBlock)), std::ios::beg);
                file.read(reinterpret_cast<char*>(&fb), sizeof(FileBlock));
                contenido.append(fb.b_content, 64);
            }
        }
        contenido = contenido.substr(0, inodoUsers.i_size);

        std::stringstream ss(contenido);
        std::string linea;
        while(std::getline(ss, linea, '\n')){
            if(linea.empty()) continue;

            if (!linea.empty() && linea.back() == '\r') linea.pop_back();

            std::stringstream ssLinea(linea);
            std::string idStr, tipo, grupo, usuario, password;

            std::getline(ssLinea, idStr, ',');
            std::getline(ssLinea, tipo, ',');

            if(tipo == "U" || tipo == "u"){
                std::getline(ssLinea, grupo, ',');
                std::getline(ssLinea, usuario, ',');

                usuario.erase(0, usuario.find_first_not_of(" \t\r\n"));
                usuario.erase(usuario.find_last_not_of(" \t\r\n") + 1);

                if(usuario == nombreBuscado && idStr != "0") {
                    return std::stoi(idStr);
                }
            }
        }
        return -1;
    }

    static int rastrearRuta(std::fstream& file, const Superblock& sb, const std::string& ruta){
        if(ruta == "/") return 0;
        std::vector<std::string> carpetas;
        std::stringstream ss(ruta);
        std::string token;
        while (std::getline(ss, token, '/')) {
            if (!token.empty()) carpetas.push_back(token);
        }

        int inodoActual = 0;
        for(const std::string& nombre: carpetas){
            inodoActual = buscarEnCarpeta(file, sb, inodoActual, nombre);  
            if(inodoActual == -1) return -1;
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

    static void cambiarPropietario(std::fstream& file, const Superblock& sb, int inodoId, int nuevoUID, bool recursivo){
        Inode inodo;
        file.seekg(sb.s_inode_start + (inodoId * sizeof(Inode)), std::ios::beg);
        file.read(reinterpret_cast<char*>(&inodo), sizeof(Inode));

        inodo.i_uid = nuevoUID;

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
                                cambiarPropietario(file, sb, fb.b_content[j].b_inodo, nuevoUID, recursivo);
                            }
                        }
                    }
                }
            }
        }
    }
};

#endif