#pragma once

#include "clang/AST/Mangle.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Rewrite/Core/Rewriter.h"

class DraiExpandASTVisitor
    : public clang::RecursiveASTVisitor<DraiExpandASTVisitor> {
public:
  DraiExpandASTVisitor(clang::ASTContext *Context, clang::Rewriter &TheRewriter)
      : Context(Context), TheRewriter(TheRewriter),
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
    return true;
  }

  std::string GenKernelFuncWrapper(clang::FunctionDecl *Decl) {
    std::string wrapper;
    llvm::raw_string_ostream OS(wrapper);

    OS << "::drai::graph::DraiOp " << Decl->getName() << "(";
    {
      auto first = Decl->param_begin();
      auto last = Decl->param_end();
      if (first != last) {
        OS << (*first)->getType().getAsString() << " " << (*first)->getName();
      }

      while (++first != last) {
        OS << ", " << (*first)->getType().getAsString() << " "
           << (*first)->getName();
      }
    }
    OS << "){\n";

    // TODO

    OS << "}\n";
    return wrapper;
  }

private:
  clang::ASTContext *Context;
  clang::Rewriter &TheRewriter;
  std::unique_ptr<clang::ItaniumMangleContext> MC;
};

class DraiExpandASTConsumer : public clang::ASTConsumer {
public:
  DraiExpandASTConsumer(clang::ASTContext *Context,
                        clang::Rewriter &TheRewriter)
      : Visitor(Context, TheRewriter) {}

  void HandleTranslationUnit(clang::ASTContext &Context) override {
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
  }

private:
  DraiExpandASTVisitor Visitor;
};

class DraiExpandAction : public clang::ASTFrontendAction {
private:
  llvm::raw_ostream &OS;
  clang::Rewriter TheRewriter;

public:
  DraiExpandAction(llvm::raw_ostream &OS) : OS(OS) {}

  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &CI,
                    llvm::StringRef InFile) override {
    TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    return std::make_unique<DraiExpandASTConsumer>(&CI.getASTContext(),
                                                   TheRewriter);
  }
  void EndSourceFileAction() override {
    TheRewriter.getEditBuffer(TheRewriter.getSourceMgr().getMainFileID())
        .write(OS);
  }
};
