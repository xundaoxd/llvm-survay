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

#include "DraiExpandAction.h"
#include "LayerFilesystem.h"
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
    elfBinary("B", llvm::cl::Prefix, llvm::cl::Required,
              llvm::cl::value_desc("file"),
              llvm::cl::desc("Extract binary from drai binary"),
              llvm::cl::cat(MacroExpandCategory));

static llvm::cl::opt<std::string>
    outputFilename("o", llvm::cl::init("-"), llvm::cl::Prefix,
                   llvm::cl::value_desc("file"),
                   llvm::cl::desc("Write output to <file>"),
                   llvm::cl::cat(MacroExpandCategory));

std::vector<std::string>
CreateArgs(const std::string &filename = inputFilename) {
  std::vector<std::string> tmp{"-E", "-triple", "drai"};
  std::transform(includeDirs.begin(), includeDirs.end(),
                 std::back_inserter(tmp),
                 [](auto &&item) { return "-I" + item; });
  std::transform(macroDefines.begin(), macroDefines.end(),
                 std::back_inserter(tmp),
                 [](auto &&item) { return "-D" + item; });
  tmp.push_back(filename);
  return tmp;
}

std::unique_ptr<clang::CompilerInstance>
CreateClangInstance(llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> vfs) {
  std::unique_ptr<clang::CompilerInstance> clang =
      std::make_unique<clang::CompilerInstance>();

  clang->createDiagnostics();
  clang->createFileManager(vfs);
  return clang;
}

bool InvokeMacroExpand(llvm::IntrusiveRefCntPtr<LayerFileSystem> vfs,
                       clang::CompilerInstance *clang,
                       const std::string &inputFilename,
                       const std::string &outputFilename) {
  std::vector<std::string> base_args = CreateArgs(inputFilename);
  std::vector<const char *> args;
  args.reserve(base_args.size());
  std::transform(base_args.begin(), base_args.end(), std::back_inserter(args),
                 [](auto &item) { return item.data(); });
  clang::CompilerInvocation::CreateFromArgs(clang->getInvocation(), args,
                                            clang->getDiagnostics());

  MacroExpandAction act(vfs, outputFilename);
  if (!clang->ExecuteAction(act)) {
    llvm::errs() << "Expand macro failed\n";
    return false;
  }
  return true;
}

bool InvokeDraiExpand(clang::CompilerInstance *clang,
                      const std::string &inputFilename,
                      const std::string &elfFilename,
                      const std::string &outputFilename) {
  std::vector<std::string> base_args = CreateArgs(inputFilename);
  std::vector<const char *> args;
  args.reserve(base_args.size());
  std::transform(base_args.begin(), base_args.end(), std::back_inserter(args),
                 [](auto &item) { return item.data(); });
  clang::CompilerInvocation::CreateFromArgs(clang->getInvocation(), args,
                                            clang->getDiagnostics());

  std::error_code EC;
  llvm::raw_fd_ostream OS(outputFilename, EC);
  if (EC) {
    return false;
  }
  DraiExpandAction act(elfFilename, OS);
  if (!clang->ExecuteAction(act)) {
    return false;
  }
  return true;
}

int main(int argc, char *argv[]) {
  llvm::cl::ParseCommandLineOptions(argc, argv);
  const std::string cppFilename = inputFilename + ".ii";

  llvm::IntrusiveRefCntPtr<LayerFileSystem> vfs =
      new LayerFileSystem(llvm::vfs::getRealFileSystem());

  auto clang = CreateClangInstance(vfs);

  if (!InvokeMacroExpand(vfs, clang.get(), inputFilename, cppFilename)) {
    llvm::errs() << "Macro expand failed\n";
    return 1;
  }

  if (!InvokeDraiExpand(clang.get(), cppFilename, elfBinary, outputFilename)) {
    llvm::errs() << "Drai expand failed\n";
    return 1;
  }
  return 0;
}
