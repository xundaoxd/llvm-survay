#pragma once

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Frontend/FrontendActions.h"

#include "LayerFilesystem.h"

class MacroExpandAction : public clang::PrintPreprocessedAction {
private:
  llvm::IntrusiveRefCntPtr<LayerFileSystem> filesystem;
  std::string outputFilename;

public:
  MacroExpandAction(llvm::IntrusiveRefCntPtr<LayerFileSystem> filesystem,
                    const std::string &filename)
      : filesystem(filesystem), outputFilename(filename) {}

  void ExecuteAction() override {
    clang::CompilerInstance &CI = getCompilerInstance();
    std::string buf;
    llvm::raw_string_ostream OS(buf);

    // If we're preprocessing a module map, start by dumping the contents of the
    // module itself before switching to the input buffer.
    auto &Input = getCurrentInput();
    if (Input.getKind().getFormat() == clang::InputKind::ModuleMap) {
      if (Input.isFile()) {
        (OS) << "# 1 \"";
        OS.write_escaped(Input.getFile());
        (OS) << "\"\n";
      }
      getCurrentModule()->print(OS);
      (OS) << "#pragma clang module contents\n";
    }

    DoPrintPreprocessedInput(CI.getPreprocessor(), &OS,
                             CI.getPreprocessorOutputOpts());

    filesystem->addFile(outputFilename,
                        llvm::MemoryBuffer::getMemBufferCopy(std::move(buf)));
  }
};
