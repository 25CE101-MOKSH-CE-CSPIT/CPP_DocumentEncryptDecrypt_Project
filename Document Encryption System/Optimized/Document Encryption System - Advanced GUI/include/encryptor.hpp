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
    // Decryption result codes
    enum DecryptResult {
        kDecryptOk = 0,
        kDecryptFileError = 1,
        kDecryptNotEncrypted = 2,  // File is not a valid CRYPTON encrypted file
        kDecryptWrongPassword = 3,
        kDecryptDataError = 4,
        kDecryptOutputError = 5
    };

    static const char* getMagic() { return "\x1A\xCF\x82\x7E"; }

    static std::vector<unsigned char> deriveKey(const std::string& password) {
        std::vector<unsigned char> hash(32);
        picosha2::hash256(password.begin(), password.end(), hash.begin(), hash.end());
        return hash;
    }

    static std::vector<unsigned char> generateRandomBytes(size_t size) {
        std::vector<unsigned char> bytes(size);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        for (size_t i = 0; i < size; ++i) {
            bytes[i] = static_cast<unsigned char>(dis(gen));
        }
        return bytes;
    }

    static std::vector<unsigned char> computeVerifier(const std::vector<unsigned char>& salt, const std::string& password) {
        std::vector<unsigned char> combined = salt;
        combined.insert(combined.end(), password.begin(), password.end());
        std::vector<unsigned char> hash(32);
        picosha2::hash256(combined.begin(), combined.end(), hash.begin(), hash.end());
        return hash;
    }

    static bool encryptFile(const std::string& inputPath, const std::string& outputPath, const std::string& password, const std::string& extension) {
        std::ifstream inFile(inputPath, std::ios::binary);
        if (!inFile) return false;
        std::vector<unsigned char> data((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
        inFile.close();

        auto key = deriveKey(password);
        auto salt = generateRandomBytes(16);
        auto verifier = computeVerifier(salt, password);
        auto metaIV = generateRandomBytes(16);
        auto contentIV = generateRandomBytes(16);

        // Encrypt Extension Metadata (max 15 chars)
        unsigned char metaData[16] = {0};
        metaData[0] = static_cast<unsigned char>(extension.length() > 15 ? 15 : extension.length());
        memcpy(metaData + 1, extension.c_str(), metaData[0]);
        unsigned char encryptedMeta[16];
        plusaes::encrypt_cbc(metaData, 16, key.data(), key.size(), reinterpret_cast<const unsigned char (*)[16]>(metaIV.data()), encryptedMeta, 16, false);

        // Encrypt File Content
        unsigned long padded_size = plusaes::get_padded_encrypted_size(data.size());
        std::vector<unsigned char> encrypted(padded_size);
        if (plusaes::encrypt_cbc(data.data(), data.size(), key.data(), key.size(), reinterpret_cast<const unsigned char (*)[16]>(contentIV.data()), encrypted.data(), encrypted.size(), true) != plusaes::kErrorOk) {
            return false;
        }

        std::ofstream outFile(outputPath, std::ios::binary);
        if (!outFile) return false;
        outFile.write(getMagic(), 4);
        outFile.write(reinterpret_cast<const char*>(salt.data()), 16);
        outFile.write(reinterpret_cast<const char*>(verifier.data()), 32);
        outFile.write(reinterpret_cast<const char*>(metaIV.data()), 16);
        outFile.write(reinterpret_cast<const char*>(encryptedMeta), 16);
        outFile.write(reinterpret_cast<const char*>(contentIV.data()), 16);
        outFile.write(reinterpret_cast<const char*>(encrypted.data()), encrypted.size());
        outFile.close();
        return true;
    }

    static int decryptFile(const std::string& inputPath, std::string& outputPath, const std::string& password) {
        std::ifstream inFile(inputPath, std::ios::binary);
        if (!inFile) return kDecryptFileError;

        char magic[4];
        inFile.read(magic, 4);
        
        std::string originalExt;
        std::vector<unsigned char> contentIV(16);
        auto key = deriveKey(password);

        if (memcmp(magic, getMagic(), 4) == 0) {
            // --- NEW SECURE FORMAT ---
            std::vector<unsigned char> salt(16);
            inFile.read(reinterpret_cast<char*>(salt.data()), 16);
            std::vector<unsigned char> verifier(32);
            inFile.read(reinterpret_cast<char*>(verifier.data()), 32);
            
            // Fast Pass Check
            if (computeVerifier(salt, password) != verifier) {
                inFile.close(); return kDecryptWrongPassword;
            }

            std::vector<unsigned char> metaIV(16);
            inFile.read(reinterpret_cast<char*>(metaIV.data()), 16);
            unsigned char encryptedMeta[16], decryptedMeta[16];
            inFile.read(reinterpret_cast<char*>(encryptedMeta), 16);
            plusaes::decrypt_cbc(encryptedMeta, 16, key.data(), key.size(), reinterpret_cast<const unsigned char (*)[16]>(metaIV.data()), decryptedMeta, 16, nullptr);
            
            unsigned char extLen = decryptedMeta[0];
            if (extLen > 15) extLen = 15;
            originalExt.assign(reinterpret_cast<char*>(decryptedMeta + 1), extLen);
            
            inFile.read(reinterpret_cast<char*>(contentIV.data()), 16);
        } else {
            // --- LEGACY FORMAT ---
            // File does not have the CRYPTON magic header - it is NOT a valid encrypted file
            inFile.close();
            return kDecryptNotEncrypted;
        }

        std::vector<unsigned char> encrypted((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
        inFile.close();

        unsigned long padding_size = 0;
        std::vector<unsigned char> decrypted(encrypted.size());
        if (plusaes::decrypt_cbc(encrypted.data(), encrypted.size(), key.data(), key.size(), reinterpret_cast<const unsigned char (*)[16]>(contentIV.data()), decrypted.data(), decrypted.size(), &padding_size) != plusaes::kErrorOk) {
            return kDecryptDataError;
        }

        std::string finalPath = outputPath;
        if (outputPath.back() == '\\' || outputPath.back() == '/') {
            size_t lastSlash = inputPath.find_last_of("\\/");
            std::string fileName = (lastSlash == std::string::npos) ? inputPath : inputPath.substr(lastSlash + 1);
            if (fileName.length() > 4 && fileName.substr(fileName.length() - 4) == ".enc") fileName = fileName.substr(0, fileName.length() - 4);
            finalPath += fileName + originalExt;
        } else {
            size_t dotPos = finalPath.find_last_of(".");
            if (dotPos != std::string::npos) finalPath = finalPath.substr(0, dotPos);
            finalPath += originalExt;
        }

        std::ofstream outFile(finalPath, std::ios::binary);
        if (!outFile) return kDecryptOutputError;
        outFile.write(reinterpret_cast<const char*>(decrypted.data()), encrypted.size() - padding_size);
        outFile.close();
        outputPath = finalPath;
        return kDecryptOk;
    }

};

#endif
