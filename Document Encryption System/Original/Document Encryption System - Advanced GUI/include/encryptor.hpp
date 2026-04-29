#ifndef ENCRYPTOR_HPP
#define ENCRYPTOR_HPP

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <random>
#include "plusaes.hpp"
#include "picosha2.h"

class Encryptor {
public:
    static std::vector<unsigned char> deriveKey(const std::string& password) {
        std::vector<unsigned char> hash(32);
        picosha2::hash256(password.begin(), password.end(), hash.begin(), hash.end());
        return hash;
    }

    static std::vector<unsigned char> generateIV() {
        std::vector<unsigned char> iv(16);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        for (int i = 0; i < 16; ++i) {
            iv[i] = static_cast<unsigned char>(dis(gen));
        }
        return iv;
    }

    static bool encryptFile(const std::string& inputPath, const std::string& outputPath, const std::string& password, const std::string& extension) {
        std::ifstream inFile(inputPath, std::ios::binary);
        if (!inFile) return false;

        std::vector<unsigned char> data((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
        inFile.close();

        auto key = deriveKey(password);
        auto iv = generateIV();

        unsigned long padded_size = plusaes::get_padded_encrypted_size(data.size());
        std::vector<unsigned char> encrypted(padded_size);

        if (plusaes::encrypt_cbc(data.data(), data.size(), key.data(), key.size(), reinterpret_cast<const unsigned char (*)[16]>(iv.data()), encrypted.data(), encrypted.size(), true) != plusaes::kErrorOk) {
            return false;
        }

        std::ofstream outFile(outputPath, std::ios::binary);
        if (!outFile) return false;

        // Write IV
        outFile.write(reinterpret_cast<const char*>(iv.data()), iv.size());
        
        // Write extension metadata: [1 byte length][string]
        unsigned char extLen = static_cast<unsigned char>(extension.length());
        outFile.write(reinterpret_cast<const char*>(&extLen), 1);
        outFile.write(extension.c_str(), extLen);

        // Write encrypted data
        outFile.write(reinterpret_cast<const char*>(encrypted.data()), encrypted.size());
        outFile.close();

        return true;
    }

    static bool decryptFile(const std::string& inputPath, std::string& outputPath, const std::string& password) {
        std::ifstream inFile(inputPath, std::ios::binary);
        if (!inFile) return false;

        // Read IV
        std::vector<unsigned char> iv(16);
        inFile.read(reinterpret_cast<char*>(iv.data()), 16);
        if (inFile.gcount() != 16) return false;

        // Read extension metadata
        unsigned char extLen = 0;
        inFile.read(reinterpret_cast<char*>(&extLen), 1);
        std::string originalExt(extLen, ' ');
        inFile.read(&originalExt[0], extLen);

        // Read remaining encrypted data
        std::vector<unsigned char> encrypted((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
        inFile.close();

        auto key = deriveKey(password);
        unsigned long padding_size = 0;
        std::vector<unsigned char> decrypted(encrypted.size());

        if (plusaes::decrypt_cbc(encrypted.data(), encrypted.size(), key.data(), key.size(), reinterpret_cast<const unsigned char (*)[16]>(iv.data()), decrypted.data(), decrypted.size(), &padding_size) != plusaes::kErrorOk) {
            return false;
        }

        // Robust extraction of base path
        std::string finalOutputPath = outputPath;
        if (outputPath.back() == '\\' || outputPath.back() == '/') {
            size_t lastSlash = inputPath.find_last_of("\\/");
            std::string fileName = (lastSlash == std::string::npos) ? inputPath : inputPath.substr(lastSlash + 1);
            if (fileName.length() > 4 && fileName.substr(fileName.length() - 4) == ".enc") {
                fileName = fileName.substr(0, fileName.length() - 4);
            }
            finalOutputPath += fileName + originalExt;
        } else {
            // If user provided a full path, strip any extension they gave and use original
            size_t dotPos = finalOutputPath.find_last_of(".");
            if (dotPos != std::string::npos) {
                finalOutputPath = finalOutputPath.substr(0, dotPos);
            }
            finalOutputPath += originalExt;
        }

        std::ofstream outFile(finalOutputPath, std::ios::binary);
        if (!outFile) return false;
        outFile.write(reinterpret_cast<const char*>(decrypted.data()), encrypted.size() - padding_size);
        outFile.close();
        
        // Update the outputPath reference so main.cpp knows the final name
        outputPath = finalOutputPath;

        return true;
    }
};

#endif
