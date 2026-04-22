#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <cctype>  
#include "include/crow_all.h"

#include "comandos/disk_manager.h"
#include "comandos/fdisk.h"
#include "comandos/mount.h"
#include "comandos/cat.h"
#include "comandos/mkfs.h"    
#include "comandos/login.h"  
#include "comandos/mkgrp.h" 
#include "comandos/rmgrp.h"
#include "comandos/mkusr.h"
#include "comandos/rmusr.h"
#include "comandos/chgrp.h"
#include "comandos/mkfile.h"
#include "comandos/mkdir.h"
#include "comandos/rep.h"
#include "comandos/unmount.h"
#include "comandos/journaling.h"
#include "comandos/remove.h"
#include "comandos/rename.h"
#include "comandos/copy.h"
#include "comandos/chown.h"
#include "comandos/chmod.h"
#include "comandos/move.h"
#include "comandos/find.h"
#include "comandos/loss.h"

// Funcion para convertir string af minusculas
std::string toLowerCase(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

// Funcion para remover comillas de una cadena
std::string removeQuotes(const std::string& str) {
    if (str.length() >= 2 && 
        ((str.front() == '"' && str.back() == '"') || 
         (str.front() == '\'' && str.back() == '\''))) {
        return str.substr(1, str.length() - 2);
    }
    return str;
}

// Funcion para parsear parametros con soporte para comillas y cualquier orden
std::string parseParameter(const std::string& commandLine, const std::string& paramName) {
    // Buscar el parametro
    std::string lowerCommandLine = toLowerCase(commandLine);
    std::string lowerParamName = toLowerCase(paramName);
    
    size_t pos = lowerCommandLine.find(lowerParamName + "=");
    if (pos == std::string::npos) {
        return "";
    }
    
    // Encontrar el inicio del valor
    size_t valueStart = pos + paramName.length() + 1;
    if (valueStart >= commandLine.length()) {
        return "";
    }
    
    // Determinar el final del valor
    size_t valueEnd = commandLine.length();
    
    if (commandLine[valueStart] == '"' || commandLine[valueStart] == '\'') {
        char quote = commandLine[valueStart];
        size_t quoteEnd = commandLine.find(quote, valueStart + 1);
        if (quoteEnd != std::string::npos) {
            return commandLine.substr(valueStart + 1, quoteEnd - valueStart - 1);
        } else {
 
            size_t spacePos = commandLine.find(' ', valueStart + 1);
            if (spacePos != std::string::npos) {
                valueEnd = spacePos;
            }
            return commandLine.substr(valueStart + 1, valueEnd - valueStart - 1);
        }
    } else {
        // Si no hay comillas, buscar el siguiente espacio o final de línea
        size_t spacePos = commandLine.find(' ', valueStart);
        if (spacePos != std::string::npos) {
            valueEnd = spacePos;
        }
        return commandLine.substr(valueStart, valueEnd - valueStart);
    }
}

std::string parametrosInvalidos(const std::string& commandLine, const std::vector<std::string>& parametrosValidos){
    std::string minCmd = toLowerCase(commandLine);
    size_t pos = 0;

    while ((pos = minCmd.find("-", pos)) != std::string::npos){
        if(pos > 0 && minCmd[pos - 1] != ' '){
            pos++;
            continue;
        }
        
        size_t eqPos = minCmd.find('=', pos);
        size_t spacePos = minCmd.find(' ', pos);

        size_t endPos = std::min(eqPos, spacePos);
        if(endPos == std::string::npos) {
            endPos = minCmd.length();
        }

        std::string nombreParametro = minCmd.substr(pos, endPos - pos);

        bool permitido = false;
        for(const std::string& valido: parametrosValidos){
            if(nombreParametro == toLowerCase(valido)){
                permitido = true;
                break;
            }
        }

        if(!permitido){
            return nombreParametro;
        }

        pos = endPos;
    }
    return "";
}

bool validarExtension(const std::string& path){
    size_t pos = path.find_last_of('.');

    if(pos == std::string::npos){
        return false;
    }

    std::string ext = toLowerCase(path.substr(pos));

    return ext == ".mia";
}

// Funcion para parsear y ejecutar comandos
std::string executeCommand(const std::string& commandLine) {
    std::istringstream iss(commandLine);
    std::string cmd;
    iss >> cmd;

    if(cmd.empty()){
        return "";
    }
    
    cmd = toLowerCase(cmd);

    if(cmd[0] == '#'){
        return commandLine;
    }

    //COMANDO MKDISK
    if (cmd == "mkdisk") { 
        std::vector<std::string> permitidos = {"-size", "-fit", "-unit", "-path"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando mkdisk";
        }

        std::string sizeStr = parseParameter(commandLine, "-size");
        std::string unit = parseParameter(commandLine, "-unit");
        std::string path = parseParameter(commandLine, "-path");
        std::string fit = parseParameter(commandLine, "-fit");
        
        if (sizeStr.empty() || path.empty()) {
            return "Error: mkdisk requiere parámetros -size y -path\n"
                   "Uso: mkdisk -size=N -unit=[k|m] -path=ruta\n -fit=[BF|FF|WF]"
                   "Los parámetros pueden estar en cualquier orden";
        }

        path = removeQuotes(path);

        if(!validarExtension(path)){
            return "Error: el disco debe tener la extension .mia";
        }
        
        int size;
        try {
            size = std::stoi(sizeStr);
        } catch (const std::exception& e) {
            return "Error: el valor de size debe ser un número entero positivo";
        }
        
        if (size <= 0) {
            return "Error: el tamaño debe ser un número positivo";
        }
        
        if (unit.empty()) {
            unit = "m";
        } else {
            unit = toLowerCase(unit);
        }
        
        if (unit != "k" && unit != "m") {
            return "Error: unit debe ser 'k' (kilobytes) o 'm' (megabytes)";
        }

        return DiskManager::mkdisk(size, unit, path, fit);

    //COMANDO RMDISK
    } else if (cmd == "rmdisk") {
        std::vector<std::string> permitidos = {"-path"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando rmdisk";
        }

        std::string path = parseParameter(commandLine, "-path");

        if (path.empty()) {
            return "Error: rmdisk requiere parámetro -path\n"
                   "Uso: rmdisk -path=ruta";
        }

        path = removeQuotes(path);

        if(!validarExtension(path)){
            return "Error: el disco debe tener la extension .mia";
        }

        return DiskManager::rmdisk(path);

    //COMANDO FDISK
    } else if (cmd == "fdisk"){
        std::vector<std::string> permitidos = {"-path", "-size", "-unit", "-type", "-fit", "-name", "-add", "-delete"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando fdisk";
        }

        std::string ruta = parseParameter(commandLine, "-path");
        std::string nombre = parseParameter(commandLine, "-name");
        std::string deleteP = parseParameter(commandLine, "-delete");
        std::string adicion = parseParameter(commandLine, "-add");

        if (ruta.empty() || nombre.empty()){
            return "Error: fdisk requiere obligatoriamente los parametros -path y -name\n"
                   "Uso crear: fdisk -size=N -path=ruta -name=nombre ...\n"
                   "Uso eliminar: fdisk -delete=[fast|full] -name=nombre -path=ruta\n"
                   "Uso agregar/quitar: fdisk -add=N -unit=[b|k|m] -name=nombre -path=ruta";
        }

        ruta = removeQuotes(ruta);

        if(!validarExtension(ruta)){
            return "Error: el disco debe tener la extension .mia";
        }

        if (!deleteP.empty()) {
            deleteP = toLowerCase(deleteP);
            return ComandoFdisk::eliminarParticion(ruta, nombre, deleteP);
        }

        else if (!adicion.empty()) {
            int espacioAgregado;
            try { 
                espacioAgregado = std::stoi(adicion); 
            } catch (...) { 
                return "Error: el valor de -add debe ser un numero entero"; 
            }
            
            std::string unidad = parseParameter(commandLine, "-unit");
            unidad = unidad.empty() ? "k" : toLowerCase(unidad);

            int add = espacioAgregado;
            if (unidad == "m") {
                add = espacioAgregado * 1024 * 1024;
            } else if (unidad == "k") {
                add = espacioAgregado * 1024;
            } else if (unidad != "b") {
                return "Error: unit para -add debe ser 'b' (bytes), 'k' (kilobytes) o 'm' (megabytes)";
            }
            
            return ComandoFdisk::modificarEspacio(ruta, nombre, add);
        }

       else {
            std::string tamanoS = parseParameter(commandLine, "-size");
            if (tamanoS.empty()) {
                return "Error: fdisk requiere parametro -size para crear particiones\n"
                       "Uso: fdisk -size=N -unit=[k|m] -path=ruta -type=[P|E|L] -fit=[BF|FF|WF] -name=nombre";
            }

            int tamano;
            try {
                tamano = std::stoi(tamanoS);
            } catch (const std::exception& e) {
                return "Error: el valor de size debe ser un número entero positivo";
            }

            if (tamano <= 0) {
                return "Error: el tamaño debe ser un número positivo";
            }

            std::string unidad = parseParameter(commandLine, "-unit");
            if (unidad.empty()) {
                unidad = "k";
            } else {
                unidad = toLowerCase(unidad);
            }

            if (unidad != "b" && unidad != "k" && unidad != "m") {
                return "Error: unit debe ser 'b' (bytes) o 'k' (kilobytes) o 'm' (megabytes)";
            }

            std::string tipo = parseParameter(commandLine, "-type");
            if (tipo.empty()) {
                tipo = "P";
            } else {
                tipo = toLowerCase(tipo);
            }

            std::string fit = parseParameter(commandLine, "-fit");
            if (fit.empty()) {
                fit = "WF";
            } else {
                fit = toLowerCase(fit);
            }

            return ComandoFdisk::execute(tamano, unidad, ruta, tipo, fit, nombre);
        }

    //COMANDO MOUNT
    } else if (cmd == "mount"){
        std::vector<std::string> permitidos = {"-path", "-name"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando mount";
        }

        std::string ruta = parseParameter(commandLine, "-path");
        std::string nombre = parseParameter(commandLine, "-name");

        ruta = removeQuotes(ruta);
        nombre = removeQuotes(nombre);

        
        if(!validarExtension(ruta)){
            return "Error: el disco debe tener la extension .mia";
        }
        
        if (ruta.empty() || nombre.empty()) {
            return "Error: mount requiere parámetros -path y -name\n"
                   "Uso: mount -path=ruta -name=nombre";
        }
        
        return ComandoMount::execute(ruta, nombre);

    //COMANDO UNMOUNT
    } else if (cmd == "unmount"){
        std::vector<std::string> permitidos = {"-id"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando unmount";
        }

        std::string id = parseParameter(commandLine, "-id");

        if (id.empty()) {
            return "Error: unmount requiere el parámetro -id\n"
                   "Uso: unmount -id=id";
        }

        return ComandoUnmount::execute(id);

    //COMANDO MKFS
    } else if (cmd == "mkfs") {
        std::vector<std::string> permitidos = {"-id", "-type", "-fs"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando mkfs";
        }

        std::string id = parseParameter(commandLine, "-id");
        std::string type = parseParameter(commandLine, "-type");

        std::string fs = parseParameter(commandLine, "-fs");
        fs = fs.empty() ? "2fs" : toLowerCase(fs);

        if (fs != "2fs" && fs != "3fs") {
            return "Error: el parametro -fs solo acepta '2fs' o '3fs'";
        }
        
        if (id.empty()) {
            return "Error: mkfs requiere el parámetro -id\n"
                   "Uso: mkfs -id=id [-type=full]";
        }

        return CommandMkfs::execute(id, type, fs);
    
    //COMANDO MOUNTED
    } else if (cmd == "mounted"){
        return ComandoMount::listMountedPartitions();

    //COMANDO CAT
    } else if (cmd == "cat"){
        std::vector<std::string> archivos;

        int i = 1;
        while (true){
            std::string path = parseParameter(commandLine, "-file" + std::to_string(i));
            if(path.empty()) break;
            archivos.push_back(removeQuotes(path));
            i++;
        }

        if(archivos.empty()){
            std::string path = parseParameter(commandLine, "-file");
            if(!path.empty()){
                archivos.push_back(removeQuotes(path));
            } else {
                return "Error: cat requiere almenos un parametro -file1 \n"
                       "Uso: cat -file1=ruta1 ... -filen=rutan";
            }
        }
        
        return ComandoCat::execute(archivos);

    ////COMANDO LOGIN
    //} else if (cmd == "login"){
    //    std::vector<std::string> permitidos = {"-user", "-pass", "-id"};
    //    std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);
//
    //    if(!parametroInvalido.empty()){
    //        return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando login";
    //    }
//
    //    std::string usuario = parseParameter(commandLine, "-user");
    //    std::string password = parseParameter(commandLine, "-pass");
    //    std::string id = parseParameter(commandLine, "-id");
//
    //    if(usuario.empty() || password.empty() || id.empty()){
    //        return "Error: login requiere los parametros -user, -pass e -id \n"
    //               "Uso login -user=usuario -pass=password -id=id_particion";
    //    }
//
    //    usuario = removeQuotes(usuario);
    //    password = removeQuotes(password);
//
    //    return ComandoLogin::execute(usuario, password, id);
    
    //COMANDO LOGOUT
    //} else if(cmd == "logout"){
    //    if(!Sesion::activo){
    //        return "Error: no hay sesion iniciada";
    //    }
//
    //    std::string usuarioActual = Sesion::usuario;
//
    //    Sesion::logout();
//
    //    return "Sesion cerrada correctamente - " + usuarioActual + "";
//
    //COMANDO MKGRP
    } else if (cmd == "mkgrp"){
        std::vector<std::string> permitidos = {"-name"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando mkgrp";
        }

        std::string nombre = parseParameter(commandLine, "-name");

        if(nombre.empty()){
            return "Error: mkgrp requiere el parametro -name\n"
                   "Uso: mkgrp -name=nombre";
        }

        nombre = removeQuotes(nombre);
        return ComandoMkgrp::execute(nombre);
    
    //COMANDO RMGRP
    } else if (cmd == "rmgrp"){
        std::vector<std::string> permitidos = {"-name"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando rmgrp";
        }

        std::string nombre = parseParameter(commandLine, "-name");

        if(nombre.empty()){
            return "Error: rmgrp requiere el parametro -name\n"
                   "Uso: rmgrp -name=nombre";
        }

        nombre = removeQuotes(nombre);
        return ComandoRmgrp::execute(nombre);

    //COMANDO MKUSR
    } else if (cmd == "mkusr"){
        std::vector<std::string> permitidos = {"-user", "-pass", "-grp"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando mkuser";
        }
        
        std::string nombre = parseParameter(commandLine, "-user");
        std::string pass = parseParameter(commandLine, "-pass");
        std::string grupo = parseParameter(commandLine, "-grp");

        if(nombre.empty() || pass.empty() || grupo.empty()){
            return "Error: mkusr requiere los parametros -user, -pass y -grp\n"
                   "Uso: mkusr -user=nombre -pass=contrasena -grp=grupo";
        }

        nombre = removeQuotes(nombre);
        pass = removeQuotes(pass);
        grupo = removeQuotes(grupo);

        return ComandoMkusr::execute(nombre, pass, grupo);
    
    //COMANDO RMUSR
    } else if (cmd == "rmusr"){
        std::vector<std::string> permitidos = {"-user"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando rmuser";
        }

        std::string nombre = parseParameter(commandLine, "-user");

        if(nombre.empty()){
            return "Error: rmusr requiere el parametro -user\n"
                   "Uso: mkusr -user=nombre";
        }

        nombre = removeQuotes(nombre);

        return ComandoRmusr::execute(nombre);

    //COMANDO CHGRP
    } else if (cmd == "chgrp"){
        std::vector<std::string> permitidos = {"-user", "-grp"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando chgrp";
        }

        std::string nombre = parseParameter(commandLine, "-user");
        std::string grupo = parseParameter(commandLine, "-grp");

        if(nombre.empty() || grupo.empty()){
            return "Error: chgrp requiere el parametro -user y -grp\n"
                   "Uso: chgrp -user=nombre -grp=grupo";
        }

        nombre = removeQuotes(nombre);
        grupo = removeQuotes(grupo);

        return ComandoChgrp::execute(nombre, grupo);

    //COMANDO MKFILE
    } else if (cmd == "mkfile"){
        std::vector<std::string> permitidos = {"-path", "-r", "-size", "-cont"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando mkfile";
        }

        std::string path = parseParameter(commandLine, "-path");
        std::string strR = parseParameter(commandLine, "-r");
        std::string strSize = parseParameter(commandLine, "-size");
        std::string cont = parseParameter(commandLine, "-cont");

        if(path.empty()){
            return "Error: mkfile requiere el parametro -path\n"
                   "Uso: mkfile -path=ruta -r -size=tamano -cont=ruta";
        }

        path = removeQuotes(path);
        cont = removeQuotes(cont);

        std::string comandoMinusculas = toLowerCase(commandLine);
        bool r = (comandoMinusculas.find("-r") != std::string::npos);

        int size = strSize.empty() ? 0 : std::stoi(strSize);

        return ComandoMkfile::execute(path, r, size, cont);

    //COMANDO MKDIR
    } else if (cmd == "mkdir"){
        std::vector<std::string> permitidos = {"-path", "-p"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando mkdir";
        }

        std::string path = parseParameter(commandLine, "-path");

        if(path.empty()){
            return "Error: mkdir requiere el parametro -path\n"
                   "Uso: mkdir -path=ruta -p";
        }

        path = removeQuotes(path);

        std::string comandoMinusculas = toLowerCase(commandLine);
        bool p = false;
        std::stringstream ssCmd(comandoMinusculas);
        std::string palabra;

        while (ssCmd >> palabra){
            if (palabra == "-p"){
                p = true;
                break;
            }
        }
        
        return ComandoMkdir::execute(path, p);

    //COMANDO REP
    } else if(cmd == "rep"){
        std::vector<std::string> permitidos = {"-name", "-path", "-id", "-path_file_ls"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando rep";
        }

        std::string name = parseParameter(commandLine, "-name");
        std::string path = parseParameter(commandLine, "-path");
        std::string id = parseParameter(commandLine, "-id");
        std::string ruta = parseParameter(commandLine, "-path_file_ls");
        

        if(name.empty() || path.empty() || id.empty()){
            return "Error: rep requiere los parametros -name, -path y -id\n"
                   "Uso: rep -name=nombre -path=ruta -id=idParticion";
        }

        name = removeQuotes(name);
        path = removeQuotes(path);
        id = removeQuotes(id);
        ruta = removeQuotes(ruta);

        return ComandoRep::execute(name, path, id, ruta);

    //REMOVE
    } else if (cmd == "remove"){
        std::vector<std::string> permitidos = {"-path"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando remove";
        }

        std::string ruta = parseParameter(commandLine, "-path");
        if (ruta.empty()) return "Error: el comando require el parametro -path";

        ruta = removeQuotes(ruta);

        return ComandoRemove::execute(ruta);

    //RENAME
    } else if (cmd == "rename"){
        std::vector<std::string> permitidos = {"-path", "-name"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando rename";
        }

        std::string ruta = parseParameter(commandLine, "-path");
        if (ruta.empty()) return "Error: el comando require el parametro -path";

        std::string nombre = parseParameter(commandLine, "-name");
        if (nombre.empty()) return "Error: el comando require el parametro -name";

        ruta = removeQuotes(ruta);
        nombre = removeQuotes(nombre);

        return ComandoRename::execute(ruta, nombre);

    //COPY
    } else if (cmd == "copy"){
        std::vector<std::string> permitidos = {"-path", "-destino"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando copy";
        }

        std::string ruta = parseParameter(commandLine, "-path");
        if (ruta.empty()) return "Error: el comando require el parametro -path";

        std::string destino = parseParameter(commandLine, "-destino");
        if (destino.empty()) return "Error: el comando require el parametro -destino";

        ruta = removeQuotes(ruta);
        destino = removeQuotes(destino);

        return ComandoCopy::execute(ruta, destino);

    //MOVE
    } else if (cmd == "move"){
        std::vector<std::string> permitidos = {"-path", "-destino"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando move";
        }

        std::string ruta = parseParameter(commandLine, "-path");
        if (ruta.empty()) return "Error: el comando require el parametro -path";

        std::string destino = parseParameter(commandLine, "-destino");
        if (destino.empty()) return "Error: el comando require el parametro -destino";

        ruta = removeQuotes(ruta);
        destino = removeQuotes(destino);

        return ComandoMove::execute(ruta, destino);

    //FIND
    } else if (cmd == "find"){
        std::vector<std::string> permitidos = {"-path", "-name"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando find";
        }

        std::string ruta = parseParameter(commandLine, "-path");
        if (ruta.empty()) return "Error: el comando require el parametro -path";

        std::string nombre = parseParameter(commandLine, "-name");
        if (nombre.empty()) return "Error: el comando require el parametro -name";

        ruta = removeQuotes(ruta);
        nombre = removeQuotes(nombre);

        return ComandoFind::execute(ruta, nombre);

    //CHOWN
    } else if (cmd == "chown"){
        std::vector<std::string> permitidos = {"-path", "-r", "-usuario"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando chown";
        }

        std::string ruta = parseParameter(commandLine, "-path");
        if (ruta.empty()) return "Error: el comando require el parametro -path";

        std::string usuario = parseParameter(commandLine, "-usuario");
        if (usuario.empty()) return "Error: el comando require el parametro -usuario";

        ruta = removeQuotes(ruta);
        usuario = removeQuotes(usuario);

        bool recursivo = (commandLine.find("-r") != std::string::npos || commandLine.find("-R") != std::string::npos);

        return ComandoChown::execute(ruta, usuario, recursivo);

    //CHMOD
    } else if (cmd == "chmod"){
        std::vector<std::string> permitidos = {"-path", "-r", "-ugo"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando chmod";
        }

        std::string ruta = parseParameter(commandLine, "-path");
        if (ruta.empty()) return "Error: el comando require el parametro -path";

        std::string ugo = parseParameter(commandLine, "-ugo");
        if (ugo.empty()) return "Error: el comando require el parametro -ugo";

        ruta = removeQuotes(ruta);
        ugo = removeQuotes(ugo);

        bool recursivo = (commandLine.find("-r") != std::string::npos || commandLine.find("-R") != std::string::npos);

        return ComandoChmod::execute(ruta, ugo, recursivo);
         
    //JOURNALING
    } else if (cmd == "journaling"){
        std::vector<std::string> permitidos = {"-id"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando journaling";
        }

        std::string id = parseParameter(commandLine, "-id");
        if (id.empty()) return "Error: journaling requiere el parámetro obligatorio -id";

        return ComandoJournaling::execute(id);

    //LOSS
    } else if (cmd == "loss"){
        std::vector<std::string> permitidos = {"-id"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando loss";
        }

        std::string id = parseParameter(commandLine, "-id");
        if (id.empty()) return "Error: journaling requiere el parámetro obligatorio -id";

        return ComandoLoss::execute(id);

    //COMANDO INFO
    } else if (cmd == "info") {
        std::string path = parseParameter(commandLine, "-path");

        if (path.empty()) {
            return "Error: info requiere parámetro -path\n"
                   "Uso: info -path=ruta";
        }

        return DiskManager::getDiskInfo(path);

    //COMANDO SALIR
    } else if (cmd == "exit" || cmd == "quit") {
        return "EXIT";
    } else if (cmd.empty()) {
        return "";
    } else {
        return "Error: Comando no reconocido";
    }
}

int buscarEnCarpeta(std::fstream& file, Superblock& sb, int inodoId, const std::string& nombreBuscar) {
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
                    std::string nombreActual = fb.b_content[j].b_name;
                    if (nombreActual == nombreBuscar) {
                        return fb.b_content[j].b_inodo;
                    }
                }
            }
        }
    }
    return -1;
}

int main(int argc, char* argv[]) {

    srand(time(nullptr));

    crow::App<crow::CORSHandler> app;
    auto& cors = app.get_middleware<crow::CORSHandler>();
    cors.global()
        .headers("X-Custom-Header", "Upgrade-Insecure-Requests", "Content-Type")
        .methods("POST"_method, "GET"_method, "OPTIONS"_method)
        .origin("*"); 

    //ENDPOINT CONSOLA
    CROW_ROUTE(app, "/ejecutar").methods(crow::HTTPMethod::POST, crow::HTTPMethod::OPTIONS)([](const crow::request& req) {
        
        auto json_body = crow::json::load(req.body);
        if (!json_body) return crow::response(400, "Error: JSON Invalido");

        std::string comandoTexto = json_body["consola"].s();
        
        std::cout << "\n[FRONTEND REQUEST] Ejecutando:\n" << comandoTexto << "\n-------------------\n";

        std::string respuestaFinal = "";

        std::istringstream stream(comandoTexto);
        std::string linea;

        while (std::getline(stream, linea)) {
            if (!linea.empty() && linea.back() == '\r') linea.pop_back();
            if (linea.empty()) continue;

            std::string res = executeCommand(linea);

            if (res == "EXIT") {
                respuestaFinal += "Comando exit omitido en modo servidor.\n";
            } else if (!res.empty()) {
                respuestaFinal += res + "\n\n";
            }
        }

        crow::json::wvalue json_response;
        json_response["respuesta"] = respuestaFinal;
        return crow::response(json_response);
    });

    //ENDPOINT LOGIN
    CROW_ROUTE(app, "/login").methods(crow::HTTPMethod::POST, crow::HTTPMethod::OPTIONS)([](const crow::request& req) {
        auto json_body = crow::json::load(req.body);
        if (!json_body) return crow::response(400, "Error: JSON Invalido");

        std::string usuario = json_body["usuario"].s();
        std::string password = json_body["pass"].s();
        std::string id = json_body["id"].s();

        std::string resultado = ComandoLogin::execute(usuario, password, id);

        crow::json::wvalue json_response;
        json_response["respuesta"] = resultado;
        return crow::response(json_response);
    });

    //ENDPOINT LOGOUT
    CROW_ROUTE(app, "/logout").methods(crow::HTTPMethod::POST, crow::HTTPMethod::OPTIONS)([](const crow::request& req) {
        
        crow::json::wvalue json_response;

        if(!Sesion::activo){
            json_response["respuesta"] = "Error: no hay sesion iniciada";
        } else {
            std::string usuarioActual = Sesion::usuario;
            Sesion::logout();
            json_response["respuesta"] = "Sesion cerrada correctamente - " + usuarioActual;
        }

        return crow::response(json_response);
    });

    //ENDPOINT VISUALIZAR CARPETAS Y ARCHIVOS
    CROW_ROUTE(app, "/explorar").methods(crow::HTTPMethod::POST, crow::HTTPMethod::OPTIONS)([](const crow::request& req) {
        if (req.method == crow::HTTPMethod::OPTIONS) {
            return crow::response(200);
        }

        auto json_body = crow::json::load(req.body);
        if (!json_body) return crow::response(400, "{\"error\": \"JSON Invalido\"}");

        std::string id = json_body["id"].s();
        std::string rutaInterior = json_body["ruta"].s();

        ComandoMount::ParticionMontada particion;
        
        if(!ComandoMount::getMountedPartition(id, particion)){
            return crow::response(404, "{\"error\": \"Error: La particion con id '" + id + "' no esta montada\"}");
        }

        std::string rutaDisco = particion.ruta;
        int inicioParticion = particion.inicio;

        std::fstream file(rutaDisco, std::ios::in | std::ios::binary);
        if (!file.is_open()) return crow::response(400, "{\"error\": \"No se pudo abrir el disco\"}");

        Superblock sb;
        file.seekg(inicioParticion, std::ios::beg);
        file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));

        if (sb.s_magic != 0xEF53) {
            file.close();
            return crow::response(400, "{\"error\": \"La particion no esta formateada (Falta mkfs).\"}");
        }

        int inodoIdActual = 0;
        
        if (rutaInterior != "/" && !rutaInterior.empty()) {
            std::vector<std::string> carpetas;
            std::stringstream ss(rutaInterior);
            std::string token;
            while (std::getline(ss, token, '/')) {
                if (!token.empty()) carpetas.push_back(token);
            }
            for (const std::string& nombreCarpeta : carpetas) {
                inodoIdActual = buscarEnCarpeta(file, sb, inodoIdActual, nombreCarpeta);
                if (inodoIdActual == -1) {
                    file.close();
                    return crow::response(404, "{\"error\": \"La ruta no existe\"}");
                }
            }
        }

        Inode inodoCarpeta;
        file.seekg(sb.s_inode_start + (inodoIdActual * sizeof(Inode)), std::ios::beg);
        file.read(reinterpret_cast<char*>(&inodoCarpeta), sizeof(Inode));

        crow::json::wvalue::list listaArchivos;

        for (int i = 0; i < 12; i++) {
            if (inodoCarpeta.i_block[i] != -1) {
                FolderBlock fb;
                file.seekg(sb.s_block_start + (inodoCarpeta.i_block[i] * sizeof(FolderBlock)), std::ios::beg);
                file.read(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));

                for (int j = 0; j < 4; j++) {
                    if (fb.b_content[j].b_inodo != -1) {
                        Inode hijo;
                        file.seekg(sb.s_inode_start + (fb.b_content[j].b_inodo * sizeof(Inode)), std::ios::beg);
                        file.read(reinterpret_cast<char*>(&hijo), sizeof(Inode));

                        std::string nombre = fb.b_content[j].b_name;

                        if (nombre != "." && nombre != "..") {
                            crow::json::wvalue objArchivo;
                            objArchivo["nombre"] = nombre;
                            objArchivo["tipo"] = (hijo.i_type == '0') ? "carpeta" : "archivo";
                            objArchivo["size"] = hijo.i_size;
                            listaArchivos.push_back(std::move(objArchivo));
                        }
                    }
                }
            }
        }
        file.close();

        crow::json::wvalue json_response;
        json_response["archivos"] = std::move(listaArchivos);
        return crow::response(json_response);
    });

    //LEER ARCHIVO
    CROW_ROUTE(app, "/leer_archivo").methods(crow::HTTPMethod::POST, crow::HTTPMethod::OPTIONS)([](const crow::request& req) {
        
        if (req.method == crow::HTTPMethod::OPTIONS) {
            return crow::response(200); 
        }

        auto json_body = crow::json::load(req.body);
        if (!json_body) return crow::response(400, "{\"error\": \"JSON Invalido\"}");

        std::string id = json_body["id"].s();
        std::string rutaInterior = json_body["ruta"].s();

        ComandoMount::ParticionMontada particion;
        if(!ComandoMount::getMountedPartition(id, particion)){
            return crow::response(404, "{\"error\": \"Particion no montada\"}");
        }

        std::fstream file(particion.ruta, std::ios::in | std::ios::binary);
        if (!file.is_open()) return crow::response(400, "{\"error\": \"No se pudo abrir el disco\"}");

        Superblock sb;
        file.seekg(particion.inicio, std::ios::beg);
        file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));

        if (sb.s_magic != 0xEF53) {
            file.close();
            return crow::response(400, "{\"error\": \"La particion no esta formateada (Falta mkfs).\"}");
        }

        int inodoIdActual = 0;
        
        if (rutaInterior != "/" && !rutaInterior.empty()) {
            std::vector<std::string> carpetas;
            std::stringstream ss(rutaInterior);
            std::string token;
            while (std::getline(ss, token, '/')) {
                if (!token.empty()) carpetas.push_back(token);
            }
            for (const std::string& nombre : carpetas) {
                inodoIdActual = buscarEnCarpeta(file, sb, inodoIdActual, nombre);
                if (inodoIdActual == -1) {
                    file.close();
                    return crow::response(404, "{\"error\": \"El archivo no existe\"}");
                }
            }
        }

        Inode inodoArchivo;
        file.seekg(sb.s_inode_start + (inodoIdActual * sizeof(Inode)), std::ios::beg);
        file.read(reinterpret_cast<char*>(&inodoArchivo), sizeof(Inode));

        if (inodoArchivo.i_type != '1') {
            file.close();
            return crow::response(400, "{\"error\": \"La ruta no es un archivo de texto\"}");
        }

        std::string contenidoFinal = "";
        for (int i = 0; i < 12; i++) {
            if (inodoArchivo.i_block[i] != -1) {
                FileBlock fb;
                file.seekg(sb.s_block_start + (inodoArchivo.i_block[i] * sizeof(FileBlock)), std::ios::beg);
                file.read(reinterpret_cast<char*>(&fb), sizeof(FileBlock));
                
                for (int c = 0; c < 64; c++) {
                    if (fb.b_content[c] != '\0') {
                        contenidoFinal += fb.b_content[c];
                    } else {
                        break;
                    }
                }
            }
        }
        file.close();

        crow::json::wvalue json_response;
        json_response["contenido"] = contenidoFinal;
        return crow::response(json_response);
    });

    std::cout << "==========================================" << std::endl;
    std::cout << "C++ DISK - SERVIDOR ACTIVO" << std::endl;
    std::cout << "MIA Proyecto 2 - 2026" << std::endl;
    std::cout << "Escuchando en http://localhost:8080" << std::endl;
    std::cout << "==========================================" << std::endl;
    
    app.port(8080).multithreaded().run();

    return 0;
}