#pragma once

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Frontend/FrontendActions.h"
#include "llvm/Support/VirtualFileSystem.h"

class MacroExpandAction : public clang::PrintPreprocessedAction {
private:
  llvm::raw_ostream &OS;

public:
  MacroExpandAction(llvm::raw_ostream &OS) : OS(OS) {}
  void ExecuteAction() override {
    clang::CompilerInstance &CI = getCompilerInstance();

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
  }
};
