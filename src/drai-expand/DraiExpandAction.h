#pragma once
#include <iomanip>
#include <regex>
#include <sstream>

#include "fmt/format.h"
#include "clang/AST/Mangle.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include "ElfBinary.h"

namespace {

char header_template[] = R"^(
#include <mutex>

)^";

char function_template[] = R"^(
void $func_name($func_args) {
  static unsigned char binary[] = {$binary_data};

  static std::once_flag flag;
  std::call_once(flag, [&]() {
    drai::graph::KernelEntry entry((void(*)($func_args))&$func_name, std::string(reinterpret_cast<const char*>(binary), sizeof(binary)));
    drai::graph::DraiContext::Instance()->RegisterKernel(std::move(entry));
  });
  auto&& args = ::drai::PopLaunchConfig();
  assert(args.size() >= 1);
  drai::graph::DraiGraph* graph = (drai::graph::DraiGraph*)args[0];
  auto op = graph->NewKernel((void(*)($func_args))&$func_name $arg_vars);
  if (args.size() >= 2) {
    op.CoreCount(args[1]);
  }
}
)^";

char call_template[] = R"^(
  ::drai::PushLaunchConfig($config);
  $callee($arg_vars))^";

} // namespace

class DraiExpandASTVisitor
    : public clang::RecursiveASTVisitor<DraiExpandASTVisitor> {
public:
  DraiExpandASTVisitor(ElfBinary &elf, clang::ASTContext *Context,
                       clang::Rewriter &TheRewriter)
      : elf(elf), Context(Context), TheRewriter(TheRewriter),
        MC(clang::ItaniumMangleContext::create(*Context,
                                               Context->getDiagnostics())) {}

  bool shouldVisitTemplateInstantiations() const { return true; }

  bool VisitFunctionDecl(clang::FunctionDecl *Decl) {
    if (Decl->hasAttr<clang::DRAIGlobalAttr>()) {
      switch (Decl->getTemplatedKind()) {
      default:
        break;
      case clang::FunctionDecl::TK_FunctionTemplate:
        TheRewriter.ReplaceText(TheRewriter.getSourceMgr().getExpansionRange(
                                    Decl->getBody()->getSourceRange()),
                                ";\n");
        break;
      case clang::FunctionDecl::TK_NonTemplate:
        TheRewriter.ReplaceText(TheRewriter.getSourceMgr().getExpansionRange(
                                    Decl->getSourceRange()),
                                GenKernelFuncWrapper(Decl));
        break;
      case clang::FunctionDecl::TK_FunctionTemplateSpecialization:
        TheRewriter.InsertTextAfterToken(
            TheRewriter.getSourceMgr().getExpansionLoc(Decl->getEndLoc()),
            GenKernelFuncWrapper(Decl));
        break;
      }
    }
    return true;
  }
  bool VisitDRAIKernelCallExpr(clang::DRAIKernelCallExpr *Decl) {
    // TODO
    std::string tmp = call_template;

    {
      // for $config
      auto cfgs = Decl->getConfig()->inits();
      std::stringstream ss;
      auto first = cfgs.begin();
      auto last = cfgs.end();
      if (first != last) {
        ss << TheRewriter.getRewrittenText((*first)->getSourceRange());
      }
      while (++first != last) {
        ss << ", " << TheRewriter.getRewrittenText((*first)->getSourceRange());
      }
      tmp = std::regex_replace(tmp, std::regex("\\$config"), ss.str());
    }

    {
      // for $callee
      clang::SourceRange targ_range = Decl->getSourceRange();
      targ_range.setEnd(Decl->getConfig()->getSourceRange().getBegin());
      std::string callee = TheRewriter.getRewrittenText(
          clang::CharSourceRange::getCharRange(targ_range));
      tmp = std::regex_replace(tmp, std::regex("\\$callee"), callee);
    }

    {
      // for $arg_vars
      std::stringstream ss;
      auto first = Decl->arg_begin();
      auto last = Decl->arg_end();
      if (first != last) {
        ss << TheRewriter.getRewrittenText((*first)->getSourceRange());
      }
      while (++first != last) {
        ss << ", " << TheRewriter.getRewrittenText((*first)->getSourceRange());
      }
      tmp = std::regex_replace(tmp, std::regex("\\$arg_vars"), ss.str());
    }

    TheRewriter.ReplaceText(
        TheRewriter.getSourceMgr().getExpansionRange(Decl->getSourceRange()),
        tmp);
    return true;
  }

  std::string GenKernelFuncWrapper(clang::FunctionDecl *Decl) {
    std::string tmp = function_template;

    {
      // for $func_name
      tmp = std::regex_replace(tmp, std::regex("\\$func_name"),
                               Decl->getName().str());
    }

    {
      // for $func_args
      std::stringstream ss;
      auto first = Decl->param_begin();
      auto last = Decl->param_end();
      if (first != last) {
        ss << (*first)->getType().getAsString() << " "
           << (*first)->getName().str();
      }
      while (++first != last) {
        ss << ", " << (*first)->getType().getAsString() << " "
           << (*first)->getName().str();
      }
      tmp = std::regex_replace(tmp, std::regex("\\$func_args"), ss.str());
    }

    {
      // for $binary_data
      std::string mangle_name;
      llvm::raw_string_ostream OS(mangle_name);
      MC->mangleName(llvm::cast<clang::NamedDecl>(Decl), OS);
      llvm::Optional<llvm::object::ELFSymbolRef> sym =
          elf.findSymbol(mangle_name);
      if (!sym) {
        llvm::errs() << "Could not find symbol: " << mangle_name << '\n';
        exit(1);
      }
      llvm::Optional<llvm::StringRef> buf = elf.getSymbolBuffer(*sym);
      if (!buf) {
        llvm::errs() << "Could not find function: " << mangle_name << '\n';
        exit(1);
      }
      tmp =
          std::regex_replace(tmp, std::regex("\\$binary_data"),
                             fmt::format("0x{:02x}", fmt::join(*buf, ", 0x")));
    }

    {
      // for $arg_vars
      std::stringstream ss;
      for (auto &&param : Decl->parameters()) {
        ss << ", " << param->getName().str();
      }
      tmp = std::regex_replace(tmp, std::regex("\\$arg_vars"), ss.str());
    }

    return tmp;
  }

  void FinishVisit() {
    auto &&SM = TheRewriter.getSourceMgr();
    clang::SourceLocation begin = SM.translateLineCol(SM.getMainFileID(), 1, 1);
    TheRewriter.InsertTextBefore(begin, header_template);
  }

private:
  clang::ASTContext *Context;
  clang::Rewriter &TheRewriter;
  std::unique_ptr<clang::ItaniumMangleContext> MC;
  ElfBinary &elf;
};

class DraiExpandASTConsumer : public clang::ASTConsumer {
public:
  DraiExpandASTConsumer(ElfBinary &elf, clang::ASTContext *Context,
                        clang::Rewriter &TheRewriter)
      : Visitor(elf, Context, TheRewriter) {}

  void HandleTranslationUnit(clang::ASTContext &Context) override {
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    Visitor.FinishVisit();
  }

private:
  DraiExpandASTVisitor Visitor;
};

class DraiExpandAction : public clang::ASTFrontendAction {
private:
  llvm::raw_ostream &OS;
  clang::Rewriter TheRewriter;
  ElfBinary elf_binary;

public:
  DraiExpandAction(const std::string &elf_binary, llvm::raw_ostream &OS)
      : elf_binary(elf_binary), OS(OS) {
    if (!this->elf_binary.good()) {
      llvm::errs() << "Could not open elf binary " << elf_binary << '\n';
      exit(1);
    }
  }

  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &CI,
                    llvm::StringRef InFile) override {
    TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    return std::make_unique<DraiExpandASTConsumer>(
        elf_binary, &CI.getASTContext(), TheRewriter);
  }
  void EndSourceFileAction() override {
    TheRewriter.getEditBuffer(TheRewriter.getSourceMgr().getMainFileID())
        .write(OS);
  }
};
