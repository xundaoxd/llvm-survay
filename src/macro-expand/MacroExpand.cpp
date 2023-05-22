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
int main(int argc, char *argv[]) {
  llvm::cl::ParseCommandLineOptions(argc, argv);

  llvm::IntrusiveRefCntPtr<llvm::vfs::OverlayFileSystem> overlay_filesystem =
      new llvm::vfs::OverlayFileSystem(llvm::vfs::getRealFileSystem());
  llvm::IntrusiveRefCntPtr<llvm::vfs::InMemoryFileSystem> upper_filesystem =
      new llvm::vfs::InMemoryFileSystem();
  overlay_filesystem->pushOverlay(upper_filesystem);

  std::vector<std::string> base_args = CreateArgs();

  std::unique_ptr<clang::CompilerInstance> clang =
      std::make_unique<clang::CompilerInstance>();

  clang->createDiagnostics();
  clang->createFileManager(overlay_filesystem);

  std::vector<const char *> pp_args;
  pp_args.reserve(base_args.size());
  std::transform(base_args.begin(), base_args.end(),
                 std::back_inserter(pp_args),
                 [](auto &item) { return item.data(); });
  clang::CompilerInvocation::CreateFromArgs(clang->getInvocation(), pp_args,
                                            clang->getDiagnostics());

  std::string O;
  llvm::raw_string_ostream pp_os(O);
  MacroExpandAction pp_act(pp_os);
  if (!clang->ExecuteAction(pp_act)) {
    llvm::errs() << "Expand macro failed\n";
    return 1;
  }

  const std::string cppFilename = inputFilename + ".ii";
  base_args.back() = cppFilename;
  upper_filesystem->addFile(cppFilename, 0,
                            llvm::MemoryBuffer::getMemBufferCopy(O));
  std::vector<const char *> args;
  args.reserve(base_args.size());
  std::transform(base_args.begin(), base_args.end(), std::back_inserter(args),
                 [](auto &item) { return item.data(); });
  clang::CompilerInvocation::CreateFromArgs(clang->getInvocation(), args,
                                            clang->getDiagnostics());

  std::error_code EC;
  llvm::raw_fd_ostream OS(outputFilename, EC);
  if (EC) {
    llvm::errs() << EC.message() << '\n';
    return 1;
  }
  DraiExpandAction act(OS);
  if (!clang->ExecuteAction(act)) {
    llvm::errs() << "Expand kernel function failed\n";
    return 1;
  }
  return 0;
}
