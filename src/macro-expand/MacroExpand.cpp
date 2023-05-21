#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/FrontendOptions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"

static llvm::cl::OptionCategory MacroExpandCategory("macro-expand options");

int main(int argc, const char **argv) {
  llvm::Expected<clang::tooling::CommonOptionsParser> OptionsParser =
      clang::tooling::CommonOptionsParser::create(argc, argv,
                                                  MacroExpandCategory);
  if (!OptionsParser) {
    llvm::errs() << OptionsParser.takeError() << '\n';
    return 1;
  }
  clang::tooling::ClangTool Tool(OptionsParser->getCompilations(),
                                 OptionsParser->getSourcePathList());
  return Tool.run(
      clang::tooling::newFrontendActionFactory<clang::PrintPreprocessedAction>()
          .get());
}
