//=============================================================================
//  Violet Aura Creations — Binary Format Emitters
//  PE (Windows), ELF (Linux), Mach-O (macOS) direct emission
//=============================================================================

#ifndef BINARY_EMITTER_HPP
#define BINARY_EMITTER_HPP

#pragma once

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable : 4996)
#endif

#include <vector>
#include <string>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <cstring>

#ifndef _WIN32
#include <sys/stat.h>
#endif

// -----------------------------------------------------------------------------
// PE (Portable Executable) Format - Windows
// -----------------------------------------------------------------------------

class PEEmitter {
public:
    bool emitExecutable(const std::string& filename, const std::vector<uint8_t>& code,
         const std::vector<uint8_t>& data) {
        std::cout << "\n\033[1;35m[PE Emitter]\033[0m Creating Windows executable...\n";
        
        std::ofstream out(filename, std::ios::binary);
        if (!out) return false;
        
        // DOS Header (64 bytes)
        writeDOSHeader(out);
        
      // PE Signature
        writeU32(out, 0x00004550);  // "PE\0\0"
        
        // COFF Header
    writeCOFFHeader(out, code, data);
   
      // Optional Header (PE32+)
        writeOptionalHeader(out, code, data);
        
        // Section Headers
        writeSectionHeaders(out);

   // .text section (code)
        writeCodeSection(out, code);
        
        // .data section
      writeDataSection(out, data);
        
  out.close();
        std::cout << "\033[1;32m✅ PE executable created: " << filename << "\033[0m\n";
  return true;
    }
    
private:
    // Helper functions must be defined before use
    void writeU8(std::ofstream& out, uint8_t val) {
        out.write(reinterpret_cast<char*>(&val), 1);
    }
    
    void writeU16(std::ofstream& out, uint16_t val) {
        out.write(reinterpret_cast<char*>(&val), 2);
    }
    
    void writeU32(std::ofstream& out, uint32_t val) {
        out.write(reinterpret_cast<char*>(&val), 4);
    }
    
    void writeU64(std::ofstream& out, uint64_t val) {
        out.write(reinterpret_cast<char*>(&val), 8);
    }
    
    size_t alignTo(size_t value, size_t alignment) {
      return ((value + alignment - 1) / alignment) * alignment;
    }
    
    void writeDOSHeader(std::ofstream& out) {
      // MZ header
        writeU16(out, 0x5A4D);  // "MZ"
        
 // DOS stub (minimal)
        std::vector<uint8_t> dosStub(58, 0);
    out.write(reinterpret_cast<char*>(dosStub.data()), dosStub.size());
        
        // PE header offset
        writeU32(out, 0x40);  // Offset to PE header
    }
 
    void writeCOFFHeader(std::ofstream& out, const std::vector<uint8_t>& code,
    const std::vector<uint8_t>& data) {
        writeU16(out, 0x8664);  // Machine: x86-64
        writeU16(out, 2);       // Number of sections
        writeU32(out, 0);       // TimeDateStamp
        writeU32(out, 0);       // PointerToSymbolTable
        writeU32(out, 0); // NumberOfSymbols
        writeU16(out, 240);     // SizeOfOptionalHeader (PE32+)
        writeU16(out, 0x0022);  // Characteristics: executable, large address aware
    }
    
    void writeOptionalHeader(std::ofstream& out, const std::vector<uint8_t>& code,
      const std::vector<uint8_t>& data) {
        writeU16(out, 0x020B);// Magic: PE32+
    writeU8(out, 14);       // MajorLinkerVersion
        writeU8(out, 0);        // MinorLinkerVersion
        writeU32(out, alignTo(code.size(), 512));  // SizeOfCode
        writeU32(out, alignTo(data.size(), 512));  // SizeOfInitializedData
  writeU32(out, 0);       // SizeOfUninitializedData
   writeU32(out, 0x1000);  // AddressOfEntryPoint
        writeU32(out, 0x1000);  // BaseOfCode
        
  // Windows-specific fields
 writeU64(out, 0x400000); // ImageBase
   writeU32(out, 0x1000);   // SectionAlignment
  writeU32(out, 512);      // FileAlignment
writeU16(out, 6);// MajorOperatingSystemVersion
   writeU16(out, 0);        // MinorOperatingSystemVersion
        writeU16(out, 0);     // MajorImageVersion
   writeU16(out, 0);        // MinorImageVersion
        writeU16(out, 6);        // MajorSubsystemVersion
        writeU16(out, 0);      // MinorSubsystemVersion
        writeU32(out, 0);        // Win32VersionValue
        writeU32(out, 0x3000);   // SizeOfImage
        writeU32(out, 512);      // SizeOfHeaders
   writeU32(out, 0);        // CheckSum
        writeU16(out, 3);        // Subsystem: CONSOLE
        writeU16(out, 0x8160);   // DllCharacteristics
        writeU64(out, 0x100000); // SizeOfStackReserve
        writeU64(out, 0x1000);   // SizeOfStackCommit
        writeU64(out, 0x100000); // SizeOfHeapReserve
        writeU64(out, 0x1000);   // SizeOfHeapCommit
      writeU32(out, 0);        // LoaderFlags
        writeU32(out, 16);       // NumberOfRvaAndSizes
        
        // Data directories (16 entries, 8 bytes each)
        for (int i = 0; i < 16; ++i) {
   writeU64(out, 0);
 }
    }
    
    void writeSectionHeaders(std::ofstream& out) {
// .text section
        writeSectionHeader(out, ".text", 0x1000, 0x1000, 512, 512, 0x60000020);
        
     // .data section
  writeSectionHeader(out, ".data", 0x2000, 0x2000, 1024, 1536, 0xC0000040);
    }
    
    void writeSectionHeader(std::ofstream& out, const std::string& name,
           uint32_t virtualAddr, uint32_t virtualSize,
    uint32_t rawSize, uint32_t rawOffset,
         uint32_t characteristics) {
        char nameField[8] = {0};
        strncpy(nameField, name.c_str(), 8);
        out.write(nameField, 8);
        
        writeU32(out, virtualSize);
        writeU32(out, virtualAddr);
        writeU32(out, rawSize);
   writeU32(out, rawOffset);
 writeU32(out, 0);  // PointerToRelocations
        writeU32(out, 0);  // PointerToLineNumbers
      writeU16(out, 0);  // NumberOfRelocations
        writeU16(out, 0);  // NumberOfLineNumbers
        writeU32(out, characteristics);
    }
    
    void writeCodeSection(std::ofstream& out, const std::vector<uint8_t>& code) {
     out.write(reinterpret_cast<const char*>(code.data()), code.size());
        
        // Pad to file alignment
        size_t padding = alignTo(code.size(), 512) - code.size();
        std::vector<uint8_t> pad(padding, 0);
        out.write(reinterpret_cast<char*>(pad.data()), pad.size());
    }
    
    void writeDataSection(std::ofstream& out, const std::vector<uint8_t>& data) {
     out.write(reinterpret_cast<const char*>(data.data()), data.size());
        
        size_t padding = alignTo(data.size(), 512) - data.size();
        std::vector<uint8_t> pad(padding, 0);
        out.write(reinterpret_cast<char*>(pad.data()), pad.size());
    }
};

// -----------------------------------------------------------------------------
// ELF (Executable and Linkable Format) - Linux
// -----------------------------------------------------------------------------

class ELFEmitter {
public:
    bool emitExecutable(const std::string& filename, const std::vector<uint8_t>& code,
               const std::vector<uint8_t>& data) {
     std::cout << "\n\033[1;35m[ELF Emitter]\033[0m Creating Linux executable...\n";
        
        std::ofstream out(filename, std::ios::binary);
        if (!out) return false;
        
        // ELF Header
        writeELFHeader(out);
      
        // Program Headers
        writeProgramHeaders(out, code, data);
        
        // Code section
        out.write(reinterpret_cast<const char*>(code.data()), code.size());
        
        // Data section
        out.write(reinterpret_cast<const char*>(data.data()), data.size());
        
out.close();
    
// Make executable
        #ifndef _WIN32
    chmod(filename.c_str(), 0755);
        #endif
        
        std::cout << "\033[1;32m✅ ELF executable created: " << filename << "\033[0m\n";
      return true;
    }
    
private:
    void writeELFHeader(std::ofstream& out) {
        // ELF Magic
  out.write("\x7f""ELF", 4);
        
   writeU8(out, 2);     // 64-bit
        writeU8(out, 1);     // Little endian
        writeU8(out, 1);     // ELF version
        writeU8(out, 0);     // System V ABI
        writeU64(out, 0);    // Padding
    
        writeU16(out, 2);    // Executable file
        writeU16(out, 0x3E); // x86-64
     writeU32(out, 1);    // ELF version
        writeU64(out, 0x400000 + 0x1000);  // Entry point
        writeU64(out, 64);   // Program header offset
        writeU64(out, 0);    // Section header offset (none for now)
        writeU32(out, 0);    // Flags
        writeU16(out, 64);   // ELF header size
        writeU16(out, 56);   // Program header entry size
        writeU16(out, 2);    // Number of program headers
        writeU16(out, 0);  // Section header entry size
        writeU16(out, 0);    // Number of section headers
        writeU16(out, 0);    // Section name string table index
    }
    
    void writeProgramHeaders(std::ofstream& out, const std::vector<uint8_t>& code,
           const std::vector<uint8_t>& data) {
        // Code segment (PT_LOAD)
 writeU32(out, 1);    // PT_LOAD
        writeU32(out, 5);    // Flags: R+X
        writeU64(out, 0x1000);  // Offset in file
   writeU64(out, 0x400000 + 0x1000);  // Virtual address
        writeU64(out, 0x400000 + 0x1000);  // Physical address
        writeU64(out, code.size());         // Size in file
        writeU64(out, code.size());         // Size in memory
    writeU64(out, 0x1000);         // Alignment
        
        // Data segment (PT_LOAD)
      writeU32(out, 1);    // PT_LOAD
        writeU32(out, 6);    // Flags: R+W
  writeU64(out, 0x1000 + code.size());  // Offset in file
        writeU64(out, 0x400000 + 0x2000);     // Virtual address
        writeU64(out, 0x400000 + 0x2000);     // Physical address
        writeU64(out, data.size());   // Size in file
     writeU64(out, data.size());           // Size in memory
        writeU64(out, 0x1000);    // Alignment
    }
    
    void writeU8(std::ofstream& out, uint8_t val) {
        out.write(reinterpret_cast<char*>(&val), 1);
    }
    
    void writeU16(std::ofstream& out, uint16_t val) {
        out.write(reinterpret_cast<char*>(&val), 2);
    }
    
    void writeU32(std::ofstream& out, uint32_t val) {
 out.write(reinterpret_cast<char*>(&val), 4);
    }
    
    void writeU64(std::ofstream& out, uint64_t val) {
        out.write(reinterpret_cast<char*>(&val), 8);
  }
};

// -----------------------------------------------------------------------------
// Mach-O Format - macOS
// -----------------------------------------------------------------------------

class MachOEmitter {
public:
    bool emitExecutable(const std::string& filename, const std::vector<uint8_t>& code,
 const std::vector<uint8_t>& data) {
        std::cout << "\n\033[1;35m[Mach-O Emitter]\033[0m Creating macOS executable...\n";
   
        std::ofstream out(filename, std::ios::binary);
        if (!out) return false;
        
        // Mach-O Header (64-bit)
        writeMachOHeader(out);
        
        // Load Commands
writeLoadCommands(out, code, data);
  
        // Code section
        out.write(reinterpret_cast<const char*>(code.data()), code.size());
        
      // Data section
out.write(reinterpret_cast<const char*>(data.data()), data.size());
      
        out.close();
        
        #ifndef _WIN32
  chmod(filename.c_str(), 0755);
    #endif
        
        std::cout << "\033[1;32m✅ Mach-O executable created: " << filename << "\033[0m\n";
    return true;
    }
    
private:
    void writeMachOHeader(std::ofstream& out) {
        writeU32(out, 0xFEEDFACF);  // Magic (64-bit)
        writeU32(out, 0x01000007);  // CPU type: x86-64
        writeU32(out, 3);           // CPU subtype
 writeU32(out, 2);     // File type: executable
        writeU32(out, 1);     // Number of load commands
        writeU32(out, 0);      // Size of load commands (update later)
        writeU32(out, 1);        // Flags
 writeU32(out, 0);       // Reserved
    }
    
    void writeLoadCommands(std::ofstream& out, const std::vector<uint8_t>& code,
         const std::vector<uint8_t>& data) {
  // Simplified - would need full LC_SEGMENT_64 and other commands
        std::cout << "\033[1;33m[Mach-O]\033[0m Load commands stub\n";
    }
    
 void writeU32(std::ofstream& out, uint32_t val) {
      out.write(reinterpret_cast<char*>(&val), 4);
    }
};

// -----------------------------------------------------------------------------
// Universal Binary Writer
// -----------------------------------------------------------------------------

class BinaryWriter {
public:
    static bool writeBinary(const std::string& filename, 
           const std::vector<uint8_t>& code,
  const std::vector<uint8_t>& data) {
        #ifdef _WIN32
    PEEmitter emitter;
   return emitter.emitExecutable(filename, code, data);
   #elif __APPLE__
            MachOEmitter emitter;
            return emitter.emitExecutable(filename, code, data);
        #else
       ELFEmitter emitter;
            return emitter.emitExecutable(filename, code, data);
        #endif
    }
};

#endif // BINARY_EMITTER_HPP
