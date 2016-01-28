/**
 * Clang based OpenMP Reflector
 *
 * Emanuele Ruffaldi 2015-2016
 */
#include <sstream>
#include <string>
#include <iostream>
#include <memory>
#include <vector>

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/AST/RecordLayout.h"
// Declares llvm::cl::extrahelp.
#include "llvm/Support/CommandLine.h"

#include "json/json.h"

using namespace clang;
using namespace clang::driver;
using namespace clang::tooling;

static llvm::cl::OptionCategory ToolingSampleCategory("Tooling Sample");

class MyASTVisitor : public RecursiveASTVisitor<MyASTVisitor> {
public:
	MyASTVisitor(Rewriter &R, ASTContext*A,Json::Value &aroot) : TheRewriter(R) , context(A),root(aroot) {}


	/// from top level
	bool TraverseCXXRecordDecl(CXXRecordDecl * red)
	{
		return true;
	}

	bool TraverseTypeAliasDecl(TypeAliasDecl * tad)
	{
		return true;
	}

	bool MyVisitStmt(Stmt * p,int level , Json::Value & parent)
	{	
		for(int i = 0; i < level; i++)
			std::cout << ' ';
		std::cout << "stmt:" << p->getStmtClassName() << " " << level << std::endl;
		Json::Value self;
		bool descend = true;
		switch(p->getStmtClass())
		{
			case Stmt::CompoundStmtClass:
			{
				self["type"] = "compound";
				descend = true;
				break;
			}
			case Stmt::IfStmtClass:
			{
				self["type"] = "if";

				IfStmt * I = cast<IfStmt>(p);
				if (Stmt *Then = I->getThen()) {
					MyVisitStmt(Then,level+1,self["if"]);
				}
				if (Stmt *Else = I->getElse()) {
					MyVisitStmt(Else,level+1,self["else"]);
				}
				descend = false;
				parent.append(self);
				break;
			}
			case Stmt::CapturedStmtClass:
			{
				CapturedStmt * C = cast<CapturedStmt>(p);
				self["type"] = "captured";
				MyVisitStmt(C->getCapturedStmt(),level+1,self["children"]);
				break;
			}
			case Stmt::ForStmtClass:
			{
				self["type"] = "for";
				ForStmt * C = cast<ForStmt>(p);
				descend = true;
				break;
			}
			case Stmt::ReturnStmtClass:
			{
				descend = false;
				self["type"] = "return";
				parent.append(self);
				break;
			}
			case Stmt::OMPTaskDirectiveClass:
			{
				self["type"] = "OMPTaskDirectiveClass";
				break;
			}
			case Stmt::DeclStmtClass:
			{
				descend = false;
				break;
			}			
			case Stmt::OMPParallelDirectiveClass:
			{
				self["type"] = "OMPParallelDirective";
				break;
			}
			default:
			{
				std::cout << "\t\tunknown " << std::endl;
				descend = false;
				break;
			}
		}
		if(descend)
		{
			for (Stmt *SubStmt : p->children())
			{
				MyVisitStmt(SubStmt,level+1,self["children"]);
			}
			parent.append(self);
		}
		return true;
	}

	bool TraverseFunctionDecl(FunctionDecl * fx)
	{
		if(!fx->getBody())
		{
			return false;
		}
		else
		{
			Json::Value self;
			self["type"] = "function";
			self["name"] = (std::string)fx->getName();
			Stmt * p = fx->getBody();
			MyVisitStmt(p,0,self["children"]);
			root.append(self);
			return false; // no recursion
		}
	}


	bool DescendDecl(Decl * s, std::string ss)
	{
		if (isa<TypeAliasDecl>(s))
		{
			return true;
		}
		else if (isa<TypedefDecl>(s))
		{
			return true;
		}
		else if (isa<ClassTemplateSpecializationDecl>(s))
		{
			ClassTemplateSpecializationDecl * ca = cast<ClassTemplateSpecializationDecl>(s);
			//std::cout << ss << " !!!SPEC " << ca->getQualifiedNameAsString()  << std::endl;
			// << " " << DecodeType(clang::QualType(ca->getTypeForDecl(),0)) << std::endl;
			return true;
		}
		else if (isa<ClassTemplateDecl>(s))
		{
			// append for dummy
		}
		else if (isa<CXXRecordDecl>(s))
		{
			return true;
		}
		else if (isa<EnumDecl>(s)) {

			return true;
		}
		else if (isa<FieldDecl>(s)) {
			return true;
		}
		else
		{
			switch (s->getKind())
			{
			case clang::Decl::Function:
				return TraverseFunctionDecl(cast<FunctionDecl>(s));
			case clang::Decl::Namespace:
			case clang::Decl::ClassTemplate:
			case clang::Decl::EnumConstant:
			case clang::Decl::TypeAliasTemplate:
			case clang::Decl::NonTypeTemplateParm:
			case clang::Decl::TemplateTypeParm:
			case clang::Decl::TranslationUnit:
			case clang::Decl::Var:
			case clang::Decl::CXXConstructor:
			case clang::Decl::CXXDestructor:
			case clang::Decl::CXXMethod:
			case clang::Decl::FunctionTemplate:
			case clang::Decl::ParmVar:
			case clang::Decl::Using:
			case clang::Decl::Empty:
			case clang::Decl::AccessSpec:
			case clang::Decl::LinkageSpec:
			case clang::Decl::IndirectField:
			case clang::Decl::CXXConversion:
			case clang::Decl::StaticAssert:
			case clang::Decl::UsingDirective:
			case clang::Decl::Friend:
			case clang::Decl::TemplateTemplateParm:
			case clang::Decl::UnresolvedUsingValue:
				break;

			default:
			{
				//std::cout << ss << " Unsupported " << s->getDeclKindName() << std::endl;

			}
			}
		}
		return true;
	}


	bool VisitDecl(Decl *s) {
		return DescendDecl(s, "");
	}

	void DumpOut()
	{
	}


private:
	Rewriter &TheRewriter;
	Json::Value& root;
	ASTContext * context;
};

// Implementation of the ASTConsumer interface for reading an AST produced
// by the Clang parser.
class MyASTConsumer : public ASTConsumer {
public:
	MyASTConsumer(Rewriter &R, ASTContext *A, CompilerInstance &CI,Json::Value& aroot) : Visitor(R, A,aroot),root(aroot) {}

	void HandleTranslationUnit(ASTContext &Context) override {
		/* we can use ASTContext to get the TranslationUnitDecl, which is
		   a single Decl that collectively represents the entire source file */

		if (CI.hasDiagnostics() && CI.getDiagnostics().hasFatalErrorOccurred())
			return;
		Visitor.TraverseDecl(Context.getTranslationUnitDecl());

		Visitor.DumpOut();

		// now emit the output
		// or filter
	}

private:
	Json::Value &root;
	MyASTVisitor Visitor;
	clang::CompilerInstance CI;
};

// For each source file provided to the tool, a new FrontendAction is created.
class MyFrontendAction : public ASTFrontendAction {
public:
	MyFrontendAction() 
	{

	}

	~MyFrontendAction()
	{
		Json::StreamWriterBuilder wbuilder;
		wbuilder["indentation"] = "  ";
		std::string document = Json::writeString(wbuilder, root);
		std::cout << document;
	}

	void EndSourceFileAction() override {
		SourceManager &SM = TheRewriter.getSourceMgr();
		//llvm::errs() << "** EndSourceFileAction for: "
		//           << SM.getFileEntryForID(SM.getMainFileID())->getName() << "\n";
		// Now emit the rewritten buffer.
		// OUTBUF TheRewriter.getEditBuffer(SM.getMainFileID()).write(llvm::outs());
	}

#ifdef CLANG37
	std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
	        StringRef file) override {
		llvm::errs() << "** Creating AST consumer for: " << file << "\n";
		TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
		return llvm::make_unique<MyASTConsumer>(TheRewriter, &CI.getASTContext(), CI,root[file]);
	}
#else
	ASTConsumer* CreateASTConsumer(CompilerInstance &CI,
	                               StringRef file) override {
		llvm::errs() << "** Creating AST consumer for: " << file << "\n";
		TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
		return new MyASTConsumer(TheRewriter, &CI.getASTContext(), CI,root[file]);
	}
#endif
private:
	Json::Value root;
	Rewriter TheRewriter;
};

void usage()
{
	printf("yextractmp tool by Emanuele Ruffaldi 2015-2016\nSyntax: yextractmp sourcefile.cpp [tooloptions] -- [compileroptions]");
}

using namespace clang::tooling;
using namespace llvm;

static cl::OptionCategory MyToolCategory("my-tool options");

static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static cl::extrahelp MoreHelp("\nMore help text...");


int main(int argc, const char **argv) {
	if(argc < 3)
	{
		usage();
		return -1;
	}
	CommonOptionsParser op(argc, argv, MyToolCategory);
	ClangTool Tool(op.getCompilations(), op.getSourcePathList());

	// ClangTool::run accepts a FrontendActionFactory, which is then used to
	// create new objects implementing the FrontendAction interface. Here we use
	// the helper newFrontendActionFactory to create a default factory that will
	// return a new MyFrontendAction object every time.
	// To further customize this, we could create our own factory class.
	return Tool.run(newFrontendActionFactory<MyFrontendAction>().get());
}