#pragma once
#include "llvm/Object/ELFObjectFile.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/Error.h"

class ElfBinary {
private:
  std::string elf_path;
  llvm::object::OwningBinary<llvm::object::ObjectFile> obj_file;

  bool has_error{false};

public:
  ElfBinary(const std::string &elf_path) : elf_path(elf_path) {
    llvm::Expected<llvm::object::OwningBinary<llvm::object::ObjectFile>>
        objObjOrErr = llvm::object::ObjectFile::createObjectFile(elf_path);
    if (!objObjOrErr) {
      has_error = true;
    }
    obj_file = std::move(*objObjOrErr);
  }

  llvm::object::ELFObjectFileBase *getBinary() {
    return llvm::dyn_cast<llvm::object::ELFObjectFileBase>(
        obj_file.getBinary());
  }

  std::unique_ptr<llvm::object::ELFSymbolRef>
  findSymbol(const std::string &name, llvm::object::SymbolRef::Type tp) {
    for (auto &&sym : getBinary()->symbols()) {
      llvm::Expected<llvm::StringRef> sym_name = sym.getName();
      if (!sym_name) {
        continue;
      }
      if (name != *sym_name) {
        continue;
      }
      llvm::Expected<llvm::object::SymbolRef::Type> sym_tp = sym.getType();
      if (!sym_tp) {
        continue;
      }
      if (*sym_tp != tp) {
        continue;
      }
      return std::make_unique<llvm::object::ELFSymbolRef>(std::move(sym));
    }
    return nullptr;
  }

  std::unique_ptr<llvm::object::ELFSymbolRef>
  findSymbol(const std::string &name) {
    for (auto &&sym : getBinary()->symbols()) {
      llvm::Expected<llvm::StringRef> sym_name = sym.getName();
      if (!sym_name) {
        continue;
      }
      if (name == *sym_name) {
        return std::make_unique<llvm::object::ELFSymbolRef>(std::move(sym));
      }
    }
    return nullptr;
  }

  std::unique_ptr<std::string> getSymbolBuffer(const std::string &name) {
    std::unique_ptr<llvm::object::ELFSymbolRef> sym = findSymbol(name);
    if (!sym) {
      return nullptr;
    }
    llvm::Expected<std::uint64_t> addr = sym->getValue();
    llvm::Expected<std::uint64_t> size = sym->getSize();
    llvm::Expected<llvm::object::section_iterator> section_iter =
        sym->getSection();
    if (!addr || !size || !section_iter) {
      return nullptr;
    }
    llvm::object::ELFSectionRef section = **section_iter;
    std::uint64_t real_addr = *addr + section.getOffset();
    std::uint64_t real_size = *size;

    return std::make_unique<std::string>(
        getBinary()
            ->getMemoryBufferRef()
            .getBuffer()
            .slice(real_addr, real_addr + real_size)
            .str());
  }

  std::unique_ptr<std::string>
  getSymbolBuffer(const std::string &name, llvm::object::SymbolRef::Type tp) {
    std::unique_ptr<llvm::object::ELFSymbolRef> sym = findSymbol(name, tp);
    if (!sym) {
      return nullptr;
    }
    llvm::Expected<std::uint64_t> addr = sym->getValue();
    llvm::Expected<std::uint64_t> size = sym->getSize();
    llvm::Expected<llvm::object::section_iterator> section_iter =
        sym->getSection();
    if (!addr || !size || !section_iter) {
      return nullptr;
    }
    llvm::object::ELFSectionRef section = **section_iter;
    std::uint64_t real_addr = *addr + section.getOffset();
    std::uint64_t real_size = *size;

    return std::make_unique<std::string>(
        getBinary()
            ->getMemoryBufferRef()
            .getBuffer()
            .slice(real_addr, real_addr + real_size)
            .str());
  }

  bool good() const { return !has_error; }
};
