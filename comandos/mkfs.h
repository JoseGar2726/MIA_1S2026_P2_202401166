#ifndef MKFS_H
#define MKFS_H

#include <iostream>   
#include <string>     
#include <sstream>   
#include <fstream> 
#include <cmath>
#include <algorithm>  
#include "../estructuras/structures.h"
#include "mount.h"    

namespace CommandMkfs {
    
    inline std::string toLowerCase(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }
    
    inline std::string execute(const std::string& id, const std::string& type, const std::string& fs) {
        
        ComandoMount::ParticionMontada particion;
        if (!ComandoMount::getMountedPartition(id, particion)) {
            return "Error: la particion con id '" + id + "' no esta montada";
        }

        std::string rutaDisco = particion.ruta;
        int inicioParticion = particion.inicio;
        int tamanoParticion = particion.tamano;

        int n = 0;
        if(fs == "2fs"){
            n = floor((tamanoParticion - sizeof(Superblock)) / (4.0 + sizeof(Inode) + 3.0*sizeof(FileBlock)));
        } else if (fs == "3fs"){
            n = floor((tamanoParticion - sizeof(Superblock) - (50 * sizeof(Journal))) / (4.0 + sizeof(Inode) + 3.0 * sizeof(FileBlock)));
        } else {
            return "Error: Sistema de archivos '" + fs + "' no soportado. Use 2fs o 3fs.";
        }

        if (n <= 0) return "Error: La particion es demasiado pequeña para ser formateada.";

        Superblock sb;
        sb.s_inodes_count = n;
        sb.s_blocks_count = 3 * n;
        sb.s_free_inodes_count = n;
        sb.s_free_blocks_count = 3 * n;
        sb.s_mtime = time(nullptr);
        sb.s_umtime = time(nullptr);
        sb.s_mnt_count = 1;
        sb.s_magic = 0xEF53;
        sb.s_inode_size = sizeof(Inode);
        sb.s_block_size = sizeof(FileBlock);
        sb.s_first_ino = 0;
        sb.s_first_blo = 0;
        sb.s_filesystem_type = (fs == "2fs") ? 2 : 3;

        if (fs == "2fs") {
            sb.s_bm_inode_start = inicioParticion + sizeof(Superblock);
        } else {
            sb.s_bm_inode_start = inicioParticion + sizeof(Superblock) + (50 * sizeof(Journal));
        }

        sb.s_bm_block_start = sb.s_bm_inode_start + n;
        sb.s_inode_start = sb.s_bm_block_start + (3 * n);
        sb.s_block_start = sb.s_inode_start + (n * sizeof(Inode));

        std::fstream file(rutaDisco, std::ios::in | std::ios::out | std::ios::binary);
        if (!file.is_open()) return "Error: No se pudo abrir el disco para formatear.";

        file.seekp(inicioParticion, std::ios::beg);
        file.write(reinterpret_cast<char*>(&sb), sizeof(Superblock));

        if (fs == "3fs") {
            Journal bitacora[50];
            for (int i = 0; i < 50; i++) {
                bitacora[i].j_count = -1;
                memset(&bitacora[i].j_content, 0, sizeof(Information));
            }
            
            file.seekp(inicioParticion + sizeof(Superblock), std::ios::beg);
            file.write(reinterpret_cast<char*>(&bitacora), 50 * sizeof(Journal));
        }

        char cero = '0';
        file.seekp(sb.s_bm_inode_start, std::ios::beg);
        for(int i = 0; i < n; i++) file.write(&cero, 1);
        
        file.seekp(sb.s_bm_block_start, std::ios::beg);
        for(int i = 0; i < 3*n; i++) file.write(&cero, 1);

        Inode inodoRaiz;
        inodoRaiz.i_uid = 1;
        inodoRaiz.i_gid = 1;
        inodoRaiz.i_size = 0;
        inodoRaiz.i_atime = time(nullptr);
        inodoRaiz.i_ctime = time(nullptr);
        inodoRaiz.i_mtime = time(nullptr);
        inodoRaiz.i_type = '0';
        inodoRaiz.i_perm = 664;
        for(int i=0; i<15; i++) inodoRaiz.i_block[i] = -1;
        inodoRaiz.i_block[0] = 0;

        FolderBlock bloqueRaiz;
        for(int i=0; i<4; i++) {
            bloqueRaiz.b_content[i].b_inodo = -1;
            memset(bloqueRaiz.b_content[i].b_name, 0, 12);
        }
        strcpy(bloqueRaiz.b_content[0].b_name, ".");
        bloqueRaiz.b_content[0].b_inodo = 0;
        strcpy(bloqueRaiz.b_content[1].b_name, "..");
        bloqueRaiz.b_content[1].b_inodo = 0;
        strcpy(bloqueRaiz.b_content[2].b_name, "users.txt");
        bloqueRaiz.b_content[2].b_inodo = 1;

        std::string contenidoUsers = "1,G,root\n1,U,root,root,123\n";

        Inode inodoUsers;
        inodoUsers.i_uid = 1;
        inodoUsers.i_gid = 1;
        inodoUsers.i_size = contenidoUsers.length();
        inodoUsers.i_atime = time(nullptr);
        inodoUsers.i_ctime = time(nullptr);
        inodoUsers.i_mtime = time(nullptr);
        inodoUsers.i_type = '1';
        inodoUsers.i_perm = 664;
        for(int i=0; i<15; i++) inodoUsers.i_block[i] = -1;
        inodoUsers.i_block[0] = 1;

        FileBlock bloqueUsers;
        memset(bloqueUsers.b_content, 0, 64);
        strcpy(bloqueUsers.b_content, contenidoUsers.c_str());

        file.seekp(sb.s_inode_start, std::ios::beg);
        file.write(reinterpret_cast<char*>(&inodoRaiz), sizeof(Inode));
        file.write(reinterpret_cast<char*>(&inodoUsers), sizeof(Inode));

        file.seekp(sb.s_block_start, std::ios::beg);
        file.write(reinterpret_cast<char*>(&bloqueRaiz), sizeof(FolderBlock));
        file.write(reinterpret_cast<char*>(&bloqueUsers), sizeof(FileBlock));

        char uno = '1';
        file.seekp(sb.s_bm_inode_start, std::ios::beg);
        file.write(&uno, 1);
        file.write(&uno, 1);

        file.seekp(sb.s_bm_block_start, std::ios::beg);
        file.write(&uno, 1);
        file.write(&uno, 1);

        sb.s_free_inodes_count -= 2;
        sb.s_free_blocks_count -= 2;
        sb.s_first_ino = 2;
        sb.s_first_blo = 2;
        file.seekp(inicioParticion, std::ios::beg);
        file.write(reinterpret_cast<char*>(&sb), sizeof(Superblock));

        file.close();
        return "Formateo exitoso. Particion '" + id + "' formateada en " + (fs == "2fs" ? "EXT2" : "EXT3");

    }
    
}

#endif