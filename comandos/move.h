#ifndef MOVE_H
#define MOVE_H

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>
#include "../estructuras/sesion.h"
#include "../estructuras/structures.h"
#include "registrar.h"

class ComandoMove{
public:
    static std::string execute(const std::string& ruta, const std::string& destino){
        if(!Sesion::activo) return "Error: sesion no iniciada";

        if(ruta == "/" || ruta.empty()) return "Error: no se puede mover la raiz";

        if(destino.find(ruta) == 0 && (destino.length() == ruta.length() || destino[ruta.length()] == '/')){
            return "Error: no puedes mover una carpeta dentro de si misma";
        }

        std::string rutaDisco = Sesion::rutaDisco;
        int inicioParticion = Sesion::inicioParticion;

        std::vector<std::string> carpetasOrigen;
        std::stringstream ss(ruta);
        std::string token;
        while(std::getline(ss, token, '/')){
            if(!token.empty()) carpetasOrigen.push_back(token);
        }
        if(carpetasOrigen.empty()) return "Error: ruta origen invalida";
        std::string nombreElementoOrigen = carpetasOrigen.back();
        carpetasOrigen.pop_back();

        std::fstream file(rutaDisco, std::ios::in | std::ios::out | std::ios::binary);
        if(!file.is_open()) return "Error: no se pudo abrir el disco";

        Superblock sb;
        file.seekg(inicioParticion, std::ios::beg);
        file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));

        int inodoPadreOrigenId = 0;
        for(const std::string& nombre : carpetasOrigen){
            inodoPadreOrigenId = buscarEnCarpeta(file, sb, inodoPadreOrigenId, nombre);
            if(inodoPadreOrigenId == -1){
                file.close(); return "Error: la ruta origen no existe";
            }
        }

        int inodoObjetivoId = buscarEnCarpeta(file, sb, inodoPadreOrigenId, nombreElementoOrigen);
        if(inodoObjetivoId == -1){
            file.close();
            return "Error: el elemento a mover no existe";
        }

        int inodoPadreDestinoId = -1;
        std::string nombreNuevoElemento = nombreElementoOrigen;

        int inodoDestinoDirecto = rastrearRuta(file, sb, destino);

        if (inodoDestinoDirecto != -1) {
            Inode tempDest;
            file.seekg(sb.s_inode_start + (inodoDestinoDirecto * sizeof(Inode)), std::ios::beg);
            file.read(reinterpret_cast<char*>(&tempDest), sizeof(Inode));

            if (tempDest.i_type == '0') {
                inodoPadreDestinoId = inodoDestinoDirecto; 
            } else {
                file.close();
                return "Error: la ruta destino ya existe y es un archivo.";
            }
        } else {
            std::vector<std::string> tokensDestino;
            std::stringstream ssD(destino);
            std::string tD;
            while(std::getline(ssD, tD, '/')){
                if(!tD.empty()) tokensDestino.push_back(tD);
            }

            if (tokensDestino.empty()) {
                file.close(); return "Error: ruta destino invalida";
            }

            nombreNuevoElemento = tokensDestino.back();
            tokensDestino.pop_back();

            std::string rutaPadre = "";
            for (const std::string& td : tokensDestino) {
                rutaPadre += "/" + td;
            }
            if (rutaPadre.empty()) rutaPadre = "/";

            inodoPadreDestinoId = rastrearRuta(file, sb, rutaPadre);

            if (inodoPadreDestinoId == -1) {
                file.close();
                return "Error: la carpeta destino '" + rutaPadre + "' no existe.";
            }
        }

        Inode inodoPadreOrigen, inodoPadreDestino;
        file.seekg(sb.s_inode_start + (inodoPadreOrigenId * sizeof(Inode)), std::ios::beg);
        file.read(reinterpret_cast<char*>(&inodoPadreOrigen), sizeof(Inode));
        
        file.seekg(sb.s_inode_start + (inodoPadreDestinoId * sizeof(Inode)), std::ios::beg);
        file.read(reinterpret_cast<char*>(&inodoPadreDestino), sizeof(Inode));
        
        if (!tienePermiso(inodoPadreOrigen, 2)){ 
            file.close(); return "Error: no tienes permiso de escritura en el origen para desenlazar el archivo"; 
        }
        if (!tienePermiso(inodoPadreDestino, 2)){ 
            file.close(); return "Error: no tienes permiso de escritura en el destino para enlazar el archivo"; 
        }

        if(buscarEnCarpeta(file, sb, inodoPadreDestinoId, nombreNuevoElemento) != -1){
            file.close(); return "Error: ya existe un elemento con el nombre '" + nombreNuevoElemento + "' en el destino";
        }

        if(!agregarEntradaCarpeta(file, sb, inodoPadreDestinoId, nombreNuevoElemento, inodoObjetivoId, inicioParticion)){
            file.close();
            return "Error: no se pudo agregar el elemento al destino (Bloques llenos)";
        }

        eliminarEntradaCarpeta(file, sb, inodoPadreOrigenId, nombreElementoOrigen);

        file.seekp(inicioParticion, std::ios::beg);
        file.write(reinterpret_cast<char*>(&sb), sizeof(Superblock));
        file.close();

        Registrar::escribirEnJournal(rutaDisco, inicioParticion, "move", ruta, destino);

        return "Elemento '" + nombreElementoOrigen + "' movido exitosamente como '" + nombreNuevoElemento + "'.";
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

    static int obtenerBloqueLibre(std::fstream& file, Superblock& sb, int inicioParticion){
        if(sb.s_free_blocks_count <= 0) return -1;
        file.seekg(sb.s_bm_block_start, std::ios::beg);
        char bit;
        for(int i = 0; i < sb.s_blocks_count; i++){
            file.read(&bit, 1);
            if(bit == '0'){
                bit = '1';
                file.seekp(sb.s_bm_block_start + i, std::ios::beg);
                file.write(&bit, 1);
                sb.s_free_blocks_count--;
                sb.s_first_blo++;
                return i;
            }
        }
        return -1;
    }

    static bool agregarEntradaCarpeta(std::fstream& file, Superblock& sb, int inodoPadreId, const std::string& nombre, int nuevoInodoId, int inicioParticion) {
        Inode inodoPadre;
        file.seekg(sb.s_inode_start + (inodoPadreId * sizeof(Inode)), std::ios::beg);
        file.read(reinterpret_cast<char*>(&inodoPadre), sizeof(Inode));
        for (int i = 0; i < 12; i++) {
            if (inodoPadre.i_block[i] != -1) {
                FolderBlock fb;
                file.seekg(sb.s_block_start + (inodoPadre.i_block[i] * sizeof(FolderBlock)), std::ios::beg);
                file.read(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));
                for (int j = 0; j < 4; j++) {
                    if (fb.b_content[j].b_inodo == -1) {
                        std::strncpy(fb.b_content[j].b_name, nombre.c_str(), 12);
                        fb.b_content[j].b_inodo = nuevoInodoId;
                        file.seekp(sb.s_block_start + (inodoPadre.i_block[i] * sizeof(FolderBlock)), std::ios::beg);
                        file.write(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));
                        return true;
                    }
                }
            } else {
                int nuevoBloque = obtenerBloqueLibre(file, sb, inicioParticion);
                if (nuevoBloque == -1) return false;
                inodoPadre.i_block[i] = nuevoBloque;
                FolderBlock fb;
                for(int k=0; k<4; k++){ 
                    fb.b_content[k].b_inodo = -1; 
                    memset(fb.b_content[k].b_name, 0, 12); 
                }
                std::strncpy(fb.b_content[0].b_name, nombre.c_str(), 12);
                fb.b_content[0].b_inodo = nuevoInodoId;
                file.seekp(sb.s_block_start + (nuevoBloque * sizeof(FolderBlock)), std::ios::beg);
                file.write(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));
                file.seekp(sb.s_inode_start + (inodoPadreId * sizeof(Inode)), std::ios::beg);
                file.write(reinterpret_cast<char*>(&inodoPadre), sizeof(Inode));
                return true;
            }
        }
        return false;
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
                        if(std::string(temp) == nombreObjetivo){
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