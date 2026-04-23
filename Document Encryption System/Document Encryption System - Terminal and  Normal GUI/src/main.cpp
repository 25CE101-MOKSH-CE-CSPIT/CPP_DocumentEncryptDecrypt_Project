#include <iostream>
#include <string>
#include <iomanip>
#include <windows.h>
#include "encryptor.hpp"

// Define colors
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define BOLD    "\033[1m"

void printHeader() {
    std::cout << BLUE << BOLD;
    std::cout << "==================================================\n";
    std::cout << "           CRYPTON - Document Secure              \n";
    std::cout << "==================================================\n";
    std::cout << RESET << std::endl;
}

void printFooter() {
    std::cout << BLUE << BOLD;
    std::cout << "==================================================\n";
    std::cout << RESET << std::endl;
}

int main() {
    printHeader();

    while (true) {
        std::cout << CYAN << "1. Encrypt a Document\n";
        std::cout << "2. Decrypt a Document\n";
        std::cout << "0. Exit\n" << RESET;
        std::cout << YELLOW << "\nChoose an option: " << RESET;

        int choice;
        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::cin.ignore(1000, '\n');
            std::cout << RED << "Invalid input. Please enter a number.\n" << RESET;
            continue;
        }

        if (choice == 0) break;
        if (choice < 1 || choice > 2) {
            std::cout << RED << "Invalid choice. Please select 1, 2, or 0.\n" << RESET;
            continue;
        }

        std::cin.ignore(1000, '\n'); // Clear buffer
        
        std::string inputPath, outputPath, password;

        auto cleanPath = [](std::string& p) {
            if (!p.empty() && p.front() == '"' && p.back() == '"') {
                p = p.substr(1, p.length() - 2);
            }
        };

        while (true) {
            std::cout << YELLOW << "Enter input file path: " << RESET;
            std::getline(std::cin, inputPath);
            cleanPath(inputPath);
            if (inputPath.empty()) {
                std::cout << RED << "Path cannot be empty!\n" << RESET;
                continue;
            }
            DWORD dwAttrib = GetFileAttributesA(inputPath.c_str());
            if (dwAttrib == INVALID_FILE_ATTRIBUTES || (dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
                std::cout << RED << "File not found! Please check the path.\n" << RESET;
                continue;
            }
            break;
        }

        while (true) {
            std::cout << YELLOW << "Enter output folder or file: " << RESET;
            std::getline(std::cin, outputPath);
            cleanPath(outputPath);
            if (outputPath.empty()) {
                std::cout << RED << "Path cannot be empty!\n" << RESET;
                continue;
            }
            
            DWORD dwAttrib = GetFileAttributesA(outputPath.c_str());
            bool exists = (dwAttrib != INVALID_FILE_ATTRIBUTES);
            if (!exists) {
                size_t lastSlash = outputPath.find_last_of("\\/");
                if (lastSlash != std::string::npos) {
                    std::string parent = outputPath.substr(0, lastSlash);
                    DWORD pAttrib = GetFileAttributesA(parent.c_str());
                    if (pAttrib == INVALID_FILE_ATTRIBUTES || !(pAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
                        std::cout << RED << "Destination folder does not exist!\n" << RESET;
                        continue;
                    }
                }
            }
            break;
        }

        auto isDirectory = [](const std::string& path) {
            DWORD dwAttrib = GetFileAttributesA(path.c_str());
            return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
        };

        bool outputIsDir = isDirectory(outputPath) || outputPath.back() == '\\' || outputPath.back() == '/';
        if (outputIsDir && outputPath.back() != '\\' && outputPath.back() != '/') {
            outputPath += "\\";
        }

        while (true) {
            std::cout << YELLOW << "Enter password: " << RESET;
            std::getline(std::cin, password);
            if (!password.empty()) break;
            std::cout << RED << "Password cannot be empty!\n" << RESET;
        }

        if (choice == 1) {
            size_t dotPos = inputPath.find_last_of(".");
            std::string ext = (dotPos == std::string::npos) ? "" : inputPath.substr(dotPos);
            std::string fileName = inputPath;
            size_t lastSlash = inputPath.find_last_of("\\/");
            if (lastSlash != std::string::npos) fileName = inputPath.substr(lastSlash + 1);
            
            if (outputIsDir) {
                size_t fileDot = fileName.find_last_of(".");
                std::string baseName = (fileDot == std::string::npos) ? fileName : fileName.substr(0, fileDot);
                outputPath += baseName + ".enc";
            } else if (outputPath.find_last_of(".") == std::string::npos || outputPath.substr(outputPath.find_last_of(".")) != ".enc") {
                size_t dot = outputPath.find_last_of(".");
                if (dot != std::string::npos) outputPath = outputPath.substr(0, dot);
                outputPath += ".enc";
            }

            std::cout << MAGENTA << "Encrypting... " << RESET;
            if (Encryptor::encryptFile(inputPath, outputPath, password, ext)) {
                std::cout << GREEN << "Encryption Successful: Saved to: " << outputPath << RESET << "\n";
            } else {
                std::cout << RED << "Failed! Check permissions." << RESET << "\n";
            }
        } else {
            std::cout << MAGENTA << "Decrypting... " << RESET;
            if (Encryptor::decryptFile(inputPath, outputPath, password)) {
                std::cout << GREEN << "Decryption Successful: Restored to: " << outputPath << RESET << "\n";
            } else {
                std::cout << RED << "Error: Wrong Password!" << RESET << "\n";
            }
        }

        std::cout << "\n";
        printFooter();
    }

    std::cout << GREEN << "Goodbye! Stay secure.\n" << RESET;
    return 0;
}
