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

  llvm::Optional<llvm::object::ELFSymbolRef>
  findSymbol(std::function<bool(llvm::object::ELFSymbolRef)> pred) {
    auto sym_range = getBinary()->symbols();
    auto iter = std::find_if(sym_range.begin(), sym_range.end(), pred);
    if (iter == sym_range.end()) {
      return {};
    }
    return *iter;
  }

  llvm::Optional<llvm::object::ELFSymbolRef>
  findSymbol(const std::string &name, llvm::object::SymbolRef::Type tp) {
    return findSymbol([&](llvm::object::ELFSymbolRef sym) {
      llvm::Expected<llvm::StringRef> sym_name = sym.getName();
      llvm::Expected<llvm::object::SymbolRef::Type> sym_tp = sym.getType();
      if (!sym_name || !sym_tp) {
        return false;
      }
      return name == *sym_name && tp == *sym_tp;
    });
  }

  llvm::Optional<llvm::object::ELFSymbolRef>
  findSymbol(const std::string &name) {
    return findSymbol([&](llvm::object::ELFSymbolRef sym) {
      llvm::Expected<llvm::StringRef> sym_name = sym.getName();
      return !sym_name && *sym_name == name;
    });
  }

  llvm::Optional<llvm::StringRef>
  getSymbolBuffer(llvm::object::ELFSymbolRef sym) {
    llvm::Expected<std::uint64_t> addr = sym.getValue();
    llvm::Expected<std::uint64_t> size = sym.getSize();
    llvm::Expected<llvm::object::section_iterator> section_iter =
        sym.getSection();
    if (!addr || !size || !section_iter) {
      return {};
    }
    llvm::object::ELFSectionRef section = **section_iter;
    std::uint64_t real_addr = *addr + section.getOffset();
    std::uint64_t real_size = *size;

    return getBinary()->getMemoryBufferRef().getBuffer().slice(
        real_addr, real_addr + real_size);
  }

  bool good() const { return !has_error; }
};
