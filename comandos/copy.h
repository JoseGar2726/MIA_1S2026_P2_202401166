#ifndef COPY_H
#define COPY_H

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>
#include "../estructuras/sesion.h"
#include "../estructuras/structures.h"
#include "registrar.h"

class ComandoCopy{
public:
    static std::string execute(const std::string& ruta, const std::string& destino){
        if (!Sesion::activo) return "Error: sesion no iniciada";

        if(ruta == "/" || ruta.empty()){
            return "Error: no se puede copiar el directorio raiz";
        }

        std::string rutaDisco = Sesion::rutaDisco;
        int inicioParticion = Sesion::inicioParticion;

        std::fstream file(rutaDisco, std::ios::in | std::ios::out | std::ios::binary);
        if(!file.is_open()) return "Error: no se pudo abrir el archivo";

        Superblock sb;
        file.seekg(inicioParticion, std::ios::beg);
        file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));

        int inodoOrigenId = rastrearRuta(file, sb, ruta);
        if(inodoOrigenId == -1){
            file.close();
            return "Error: la ruta origen '" + ruta + "' no existe";
        }

        std::vector<std::string> carpetasOrigen;
        std::stringstream ssO(ruta);
        std::string tokenO;
        while(std::getline(ssO, tokenO, '/')){
            if(!tokenO.empty()) carpetasOrigen.push_back(tokenO);
        }
        std::string nombreNuevoElemento = carpetasOrigen.back();


        int inodoPadreDestinoId = -1;
        int inodoDestinoDirecto = rastrearRuta(file, sb, destino);

        if (inodoDestinoDirecto != -1) {
            Inode tempDest;
            file.seekg(sb.s_inode_start + (inodoDestinoDirecto * sizeof(Inode)), std::ios::beg);
            file.read(reinterpret_cast<char*>(&tempDest), sizeof(Inode));

            if (tempDest.i_type == '0') {
                inodoPadreDestinoId = inodoDestinoDirecto;
            } else {
                file.close();
                return "Error: la ruta destino ya existe y es un archivo, no una carpeta.";
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

        Inode inodoPadreDestino;
        file.seekg(sb.s_inode_start + (inodoPadreDestinoId * sizeof(Inode)), std::ios::beg);
        file.read(reinterpret_cast<char*>(&inodoPadreDestino), sizeof(Inode));

        if (!tienePermiso(inodoPadreDestino, 2)) {
            file.close();
            return "Error: el usuario no tiene permisos de escritura en la carpeta destino";
        }

        if(buscarEnCarpeta(file, sb, inodoPadreDestinoId, nombreNuevoElemento) != -1){
            file.close();
            return "Error: ya existe un elemento llamado '" + nombreNuevoElemento + "' en el destino";
        }

        if(!copiarRecursivo(file, sb, inodoOrigenId, inodoPadreDestinoId, nombreNuevoElemento, inicioParticion)){
            file.close();
            return "Error: disco lleno o fallo al copiar";
        }

        file.close();

        Registrar::escribirEnJournal(rutaDisco, inicioParticion, "copy", ruta, destino);

        return "Copia de '" + nombreNuevoElemento + "' realizada exitosamente hacia el destino.";
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

    static bool copiarRecursivo(std::fstream& file, Superblock& sb, int inodoOrigenId, int inodoDestinoId, const std::string& nombre, int inicioParticion){
        Inode inodoOrigen;
        file.seekg(sb.s_inode_start + (inodoOrigenId * sizeof(Inode)), std::ios::beg);
        file.read(reinterpret_cast<char*>(&inodoOrigen), sizeof(Inode));

        if (!tienePermiso(inodoOrigen, 4)) {
            std::cout << "-> Omitiendo '" << nombre << "' por falta de permisos de lectura.\n";
            return true; 
        }
    
        if(inodoOrigen.i_type == '1'){
            std::string contenido = "";
            for(int i = 0; i < 12; i++){
                if(inodoOrigen.i_block[i] != -1){
                    FileBlock fb;
                    file.seekg(sb.s_block_start + (inodoOrigen.i_block[i] * sizeof(FileBlock)), std::ios::beg);
                    file.read(reinterpret_cast<char*>(&fb), sizeof(FileBlock));
                    contenido.append(fb.b_content, 64);
                }
            }
            contenido = contenido.substr(0, inodoOrigen.i_size);
            return crearArchivoCopia(file, sb, inodoDestinoId, nombre, contenido, inicioParticion);
        } else {
            int nuevoInodoCarpeta = crearCarpetaCopia(file, sb, inodoDestinoId, nombre, inicioParticion);
            if(nuevoInodoCarpeta == -1) return false;

            for(int i = 0; i < 12; i++){
                if(inodoOrigen.i_block[i] != -1){
                    FolderBlock fb;
                    file.seekg(sb.s_block_start + (inodoOrigen.i_block[i] * sizeof(FolderBlock)), std::ios::beg);
                    file.read(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));

                    for(int j = 0; j < 4; j++){
                        if(fb.b_content[j].b_inodo != -1){
                            char temp[13] = {0};
                            std::strncpy(temp, fb.b_content[j].b_name, 12);
                            std::string nombreHijo(temp);
                            if(nombreHijo != "." && nombreHijo != ".."){
                                copiarRecursivo(file, sb, fb.b_content[j].b_inodo, nuevoInodoCarpeta, nombreHijo, inicioParticion);
                            }
                        }
                    }
                }
            }
            return true;
        }
    }

    static int obtenerInodoLibre(std::fstream& file, Superblock& sb, int inicioParticion){
        if (sb.s_free_inodes_count <= 0) return -1;
        file.seekg(sb.s_bm_inode_start, std::ios::beg);
        char bit;
        for(int i = 0; i < sb.s_inodes_count; i++){
            file.read(&bit, 1);
            if(bit == '0'){
                bit = '1';
                file.seekp(sb.s_bm_inode_start + i, std::ios::beg);
                file.write(&bit, 1);
                sb.s_free_inodes_count--;
                sb.s_first_ino++;
                file.seekp(inicioParticion, std::ios::beg);
                file.write(reinterpret_cast<char*>(&sb), sizeof(Superblock));
                return i;
            }
        }
        return -1;
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
                file.seekp(inicioParticion, std::ios::beg);
                file.write(reinterpret_cast<char*>(&sb), sizeof(Superblock));
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
                for(int k=0; k<4; k++) { fb.b_content[k].b_inodo = -1; memset(fb.b_content[k].b_name, 0, 12); }
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

    static int crearCarpetaCopia(std::fstream& file, Superblock& sb, int inodoPadreId, const std::string& nombre, int inicioParticion){
        int nuevoInodo = obtenerInodoLibre(file, sb, inicioParticion);
        int nuevoBloque = obtenerBloqueLibre(file, sb, inicioParticion);
        if(nuevoInodo == -1 || nuevoBloque == -1) return -1;

        Inode inodo;
        inodo.i_uid = Sesion::uid;
        inodo.i_gid = Sesion::gid;
        inodo.i_size = 0;
        inodo.i_type = '0';
        inodo.i_perm = 664;

        for (int i = 0; i < 15; i++) inodo.i_block[i] = -1;
        inodo.i_block[0] = nuevoBloque;
        file.seekp(sb.s_inode_start + (nuevoInodo * sizeof(Inode)), std::ios::beg);
        file.write(reinterpret_cast<char*>(&inodo), sizeof(Inode));

        FolderBlock fb;

        for(int i=0; i<4; i++) {
            fb.b_content[i].b_inodo = -1; 
            memset(fb.b_content[i].b_name, 0, 12);
        }

        std::strncpy(fb.b_content[0].b_name, ".", 12);
        fb.b_content[0].b_inodo = nuevoInodo;
        std::strncpy(fb.b_content[1].b_name, "..", 12); 
        fb.b_content[1].b_inodo = inodoPadreId;

        file.seekp(sb.s_block_start + (nuevoBloque * sizeof(FolderBlock)), std::ios::beg);
        file.write(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));

        agregarEntradaCarpeta(file, sb, inodoPadreId, nombre, nuevoInodo, inicioParticion);

        return nuevoInodo;
    }

    static bool crearArchivoCopia(std::fstream& file, Superblock& sb, int inodoPadreId, const std::string& nombre, const std::string& cont, int inicioParticion) {
        int bloquesReq = (cont.length() / 64) + ((cont.length() % 64 != 0) ? 1 : 0);
        if (bloquesReq == 0) bloquesReq = 1;

        int nuevoInodo = obtenerInodoLibre(file, sb, inicioParticion);
        if (nuevoInodo == -1) return false;

        Inode inodo;
        inodo.i_uid = Sesion::uid; 
        inodo.i_gid = Sesion::gid; 
        inodo.i_size = cont.length(); 
        inodo.i_type = '1'; 
        inodo.i_perm = 664;

        for (int i = 0; i < 15; i++) inodo.i_block[i] = -1;

        for (int i = 0; i < bloquesReq; i++) {
            int nuevoBloque = obtenerBloqueLibre(file, sb, inicioParticion);
            if (nuevoBloque == -1) return false;
            inodo.i_block[i] = nuevoBloque;

            FileBlock fb;
            memset(fb.b_content, 0, 64);
            int inicioChunk = i * 64;
            int longitud = std::min(64, (int)cont.length() - inicioChunk);
            if (longitud > 0) std::strncpy(fb.b_content, cont.substr(inicioChunk, longitud).c_str(), 64);

            file.seekp(sb.s_block_start + (nuevoBloque * sizeof(FileBlock)), std::ios::beg);
            file.write(reinterpret_cast<char*>(&fb), sizeof(FileBlock));
        }

        file.seekp(sb.s_inode_start + (nuevoInodo * sizeof(Inode)), std::ios::beg);
        file.write(reinterpret_cast<char*>(&inodo), sizeof(Inode));
        
        return agregarEntradaCarpeta(file, sb, inodoPadreId, nombre, nuevoInodo, inicioParticion);
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
};

#endif