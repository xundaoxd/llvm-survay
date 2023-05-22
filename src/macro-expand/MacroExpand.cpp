#include <memory>

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/FrontendOptions.h"
#include "clang/Frontend/TextDiagnosticBuffer.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/VirtualFileSystem.h"

#include "DraiExpandAction.h"
#include "MacroExpandAction.h"

static llvm::cl::OptionCategory MacroExpandCategory("macro-expand options");
static llvm::cl::opt<std::string>
    inputFilename(llvm::cl::init("-"), llvm::cl::Positional,
                  llvm::cl::desc("<file>"), llvm::cl::cat(MacroExpandCategory));
static llvm::cl::list<std::string> includeDirs(
    "I", llvm::cl::Prefix, llvm::cl::value_desc("dir"),
    llvm::cl::desc(
        "Add directory to the end of the list of include search paths"),
    llvm::cl::cat(MacroExpandCategory));
static llvm::cl::list<std::string> macroDefines(
    "D", llvm::cl::Prefix, llvm::cl::value_desc("macro=value"),
    llvm::cl::desc("Define <macro> to <value> (or 1 if <value> omitted)"),
    llvm::cl::cat(MacroExpandCategory));
static llvm::cl::opt<std::string>
    outputFilename("o", llvm::cl::init("-"), llvm::cl::Prefix,
                   llvm::cl::value_desc("file"),
                   llvm::cl::desc("Write output to <file>"),
                   llvm::cl::cat(MacroExpandCategory));

int main(int argc, char *argv[]) {
  llvm::cl::ParseCommandLineOptions(argc, argv);

  llvm::IntrusiveRefCntPtr<llvm::vfs::OverlayFileSystem> overlay_filesystem =
      new llvm::vfs::OverlayFileSystem(llvm::vfs::getRealFileSystem());
  llvm::IntrusiveRefCntPtr<llvm::vfs::InMemoryFileSystem> upper_filesystem =
      new llvm::vfs::InMemoryFileSystem();
  overlay_filesystem->pushOverlay(upper_filesystem);

  std::unique_ptr<clang::CompilerInstance> clang =
      std::make_unique<clang::CompilerInstance>();

  clang->createDiagnostics();
  clang->createFileManager(overlay_filesystem);

  clang::CompilerInvocation::CreateFromArgs(clang->getInvocation(),
                                            {"-E", inputFilename.c_str()},
                                            clang->getDiagnostics());

  std::string O;
  llvm::raw_string_ostream pp_os(O);
  MacroExpandAction pp_act(pp_os);
  if (!clang->ExecuteAction(pp_act)) {
    llvm::errs() << "ExecuteAction failed\n";
    return 1;
  }

  const std::string cppFilename = inputFilename + ".ii";
  upper_filesystem->addFile(cppFilename, 0,
                            llvm::MemoryBuffer::getMemBufferCopy(O));
  clang::CompilerInvocation::CreateFromArgs(clang->getInvocation(),
                                            {"-E", cppFilename.c_str()},
                                            clang->getDiagnostics());

  std::error_code EC;
  llvm::raw_fd_ostream OS(outputFilename, EC);
  DraiExpandAction act(OS);
  clang->ExecuteAction(act);
  return 0;
}
