#ifndef LOGIN_H
#define LOGIN_H

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include "../estructuras/sesion.h"
#include "../estructuras/structures.h"
#include "mount.h"

class ComandoLogin{
public:
    static std::string execute(const std::string& usuario, const std::string& password, const std::string& id){
        if(Sesion::activo){
            return "Error: Ya existe una sesion activa con el usuario '" + Sesion::usuario + "' Debe de hacer logout primero";
        }
    

        //LEER APARTIR DE MOUNT
        ComandoMount::ParticionMontada particion;
        if(!ComandoMount::getMountedPartition(id, particion)){
            return "Error: La particion con id '" + id + "' no esta montada";
        }

        std::string ruta = particion.ruta;
        int inicio = particion.inicio;

        std::string contenidoUsuarios = leerTxtUsuarios(ruta, inicio);
        if(contenidoUsuarios.empty()){
            return "Error: NO se pudo leer users.txt";
        }

        std::istringstream iss(contenidoUsuarios);
        std::string linea;
        bool coincidencia = false;

        while (std::getline(iss, linea, '\n')){
            if (linea.empty()) continue;

            std::stringstream ss(linea);
            std::string item;
            std::vector<std::string> tokens;

            while(std::getline(ss, item, ',')){
                size_t primero = item.find_first_not_of(' ');
                size_t ultimo = item.find_last_not_of(' ');

                if(primero != std::string::npos && ultimo != std::string::npos){
                    tokens.push_back(item.substr(primero, ultimo - primero + 1));
                }
            }

            if(tokens.size() == 5 && tokens[1] == "U"){
                if(tokens[3] == usuario && tokens[4] == password){
                    if(tokens[0] == "0"){
                        return "Error: El usuario se encuentra eliminado";
                    }

                    std::string nombreGrupoUsuario = tokens[2];
                    int idGrupoReal = -1;

                    std::istringstream issBusquedaGrupo(contenidoUsuarios);
                    std::string lineaGrupo;
                    while (std::getline(issBusquedaGrupo, lineaGrupo, '\n')) {
                        if (lineaGrupo.empty()) continue;
                        std::stringstream ssG(lineaGrupo);
                        std::string itemG;
                        std::vector<std::string> tokensG;
                        while(std::getline(ssG, itemG, ',')){
                            size_t p = itemG.find_first_not_of(' ');
                            size_t u = itemG.find_last_not_of(' ');
                            if(p != std::string::npos && u != std::string::npos) tokensG.push_back(itemG.substr(p, u - p + 1));
                        }
                        // Si es un grupo, y el nombre coincide, guardamos su ID
                        if (tokensG.size() >= 3 && tokensG[1] == "G" && tokensG[2] == nombreGrupoUsuario) {
                            idGrupoReal = std::stoi(tokensG[0]);
                            break;
                        }
                    }
                
                    Sesion::login(std::stoi(tokens[0]), idGrupoReal, usuario, id, ruta, inicio);
                    
                    return "Login exitoso Bienvenido " + usuario;
                } else if (tokens[3] == usuario){
                    coincidencia = true;
                }
            }
        }

        if (coincidencia){
            return "Error: contrasena incorrecta para el usuario '" + usuario + "'";
        }
        return "Error: el usuario '" + usuario + "' no existe";
    }
private:
    static std::string leerTxtUsuarios(const std::string& ruta, int inicioParticion){
        std::string content = "";
        std::ifstream file(ruta, std::ios::binary);

        if(!file.is_open()) return content;

        Superblock sb;
        file.seekg(inicioParticion, std::ios::beg);
        file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));

        if(sb.s_magic != 0xEF53){
            file.close();
            return content;
        }

        Inode inodoUsuarios;
        int posInodo = sb.s_inode_start + (1 * sizeof(Inode));
        file.seekg(posInodo, std::ios::beg);
        file.read(reinterpret_cast<char*>(&inodoUsuarios), sizeof(Inode));
        if(inodoUsuarios.i_type != '1'){
            file.close();
            return "";
        }

        for (int i = 0; i < 12; i++){
            if(inodoUsuarios.i_block[i] != -1){
                FileBlock fb;
                int posBloque = sb.s_block_start + (inodoUsuarios.i_block[i] * sizeof(FileBlock));
                file.seekg(posBloque, std::ios::beg);
                file.read(reinterpret_cast<char*>(&fb), sizeof(FileBlock));

                size_t len = 0;
                while (len < 64 && fb.b_content[len] != '\0'){
                    len++;
                }
                content.append(fb.b_content, len);
            }
        }

        file.close();
        return content;
    }
};

#endif

