#ifndef LOSS_H
#define LOSS_H

#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include "../estructuras/sesion.h"
#include "../estructuras/structures.h"
#include "mount.h"

class ComandoLoss {
public:
    static std::string execute(const std::string& id) {
        ComandoMount::ParticionMontada particion;
        if (!ComandoMount::getMountedPartition(id, particion)) {
            return "Error: no se encuentra una particion montada con el ID '" + id + "'";
        }

        std::string rutaDisco = particion.ruta;
        int inicioParticion = particion.inicio;

        std::fstream file(rutaDisco, std::ios::in | std::ios::out | std::ios::binary);
        if (!file.is_open()) return "Error: no se pudo abrir el disco";

        Superblock sb;
        file.seekg(inicioParticion, std::ios::beg);
        file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));

        if (sb.s_filesystem_type != 3) {
            file.close();
            return "Error: el comando 'loss' solo es aplicable a sistemas de archivos EXT3";
        }

        char nulo = '\0';

        file.seekp(sb.s_bm_inode_start, std::ios::beg);
        for (int i = 0; i < sb.s_inodes_count; i++) file.write(&nulo, 1);

        file.seekp(sb.s_bm_block_start, std::ios::beg);
        for (int i = 0; i < sb.s_blocks_count; i++) file.write(&nulo, 1);

        file.seekp(sb.s_inode_start, std::ios::beg);
        int tamanoInodos = sb.s_inodes_count * sizeof(Inode);
        for (int i = 0; i < tamanoInodos; i++) file.write(&nulo, 1);

        file.seekp(sb.s_block_start, std::ios::beg);
        int tamanoBloques = sb.s_blocks_count * 64;
        for (int i = 0; i < tamanoBloques; i++) file.write(&nulo, 1);

        file.close();

        return "Simulacion de perdida de datos exitosa en particion '" + id + "'. El sistema de archivos ha quedado inconsistente.";
    }
};

#endif