#pragma once
#include "llvm/Support/VirtualFileSystem.h"

class LayerFileSystem : public llvm::vfs::OverlayFileSystem {
private:
  llvm::IntrusiveRefCntPtr<llvm::vfs::InMemoryFileSystem> Top;

  void pushOverlay(llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> FS) {
    llvm::vfs::OverlayFileSystem::pushOverlay(std::move(FS));
  }

public:
  using llvm::vfs::OverlayFileSystem::OverlayFileSystem;
  LayerFileSystem(llvm::IntrusiveRefCntPtr<FileSystem> Base,
                  llvm::IntrusiveRefCntPtr<llvm::vfs::InMemoryFileSystem>
                      Upper = new llvm::vfs::InMemoryFileSystem())
      : OverlayFileSystem(std::move(Base)) {
    pushOverlay(std::move(Upper));
  }

  void pushOverlay(llvm::IntrusiveRefCntPtr<llvm::vfs::InMemoryFileSystem> FS) {
    Top = FS;
    pushOverlay(llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem>(std::move(FS)));
  }

  bool addFile(const llvm::Twine &Path,
               std::unique_ptr<llvm::MemoryBuffer> Buffer) {
    return Top->addFile(Path, 0, std::move(Buffer));
  }
};
