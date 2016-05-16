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
#include <fstream>

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

template <typename a, typename b>
struct Pair2Value
{
	Pair2Value(std::pair<a,b> az): z(az) {}
	operator Json::Value () { 
		Json::Value q;
		q[0] = z.first;
		q[1] = z.second;
		return q;
	}

	std::pair<a,b>  z;
};


class MyASTVisitor : public RecursiveASTVisitor<MyASTVisitor> {
public:
	MyASTVisitor(Rewriter &R, ASTContext*A,Json::Value &aroot, int &astatementid) : statementid(astatementid) ,TheRewriter(R) , context(A),root(aroot){}


	/// from top level
	bool TraverseCXXRecordDecl(CXXRecordDecl * red)
	{
		return true;
	}

	bool TraverseTypeAliasDecl(TypeAliasDecl * tad)
	{
		return true;
	}

	// assume same file
	std::pair<int,int> getloc(clang::SourceLocation  s)
	{
		const clang::SourceManager &SM = TheRewriter.getSourceMgr();
		clang::PresumedLoc PLoc = SM.getPresumedLoc(s);	
		if(PLoc.isInvalid())
			return std::make_pair<int,int>(0,0);		
		else
			return std::make_pair<int,int>(PLoc.getLine(),PLoc.getColumn());
	}

	// assume same file
	std::string getlocstr(clang::SourceLocation  s) 
	{
		const clang::SourceManager &SM = TheRewriter.getSourceMgr();
		clang::PresumedLoc PLoc = SM.getPresumedLoc(s);	
		if(PLoc.isInvalid())
			return "unknown";	
		else
		{
			std::ostringstream ons;
			ons << PLoc.getLine() << ":" << PLoc.getColumn();
			return ons.str();
		}
	}

	bool MyVisitStmt(const Stmt * p,int level , Json::Value & parent)
	{	
		if(!p)
			return false;
		for(int i = 0; i < level; i++)
			std::cout << ' ';
		std::cout << "stmt:" << p->getStmtClassName() << " (" << (statementid+1) << ") level " << level << std::endl;
		Json::Value self;
		self["loc"] = Pair2Value<int,int>(getloc(p->getLocStart()));
		self["id"] = statementid++;
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
				self["id"] = statementid++;

				const IfStmt * I = cast<IfStmt>(p);
				if (const Stmt *Then = I->getThen()) {
					MyVisitStmt(Then,level+1,self["children"]);
				}
				if (const Stmt *Else = I->getElse()) {
					MyVisitStmt(Else,level+1,self["children"]);
				}
				descend = false;
				parent.append(self);
				break;
			}
			case Stmt::CapturedStmtClass:
			{
				const CapturedStmt * C = cast<CapturedStmt>(p);
				self["type"] = "captured";
				MyVisitStmt(C->getCapturedStmt(),level+1,self["children"]);
				break;
			}
			case Stmt::ForStmtClass:
			{
				self["type"] = "for";
				const ForStmt * C = cast<ForStmt>(p);
				descend = true;
				break;
			}
			case Stmt::ReturnStmtClass:
			{
				descend = false;
				self["id"] = statementid++;
				self["type"] = "return";
				parent.append(self);
				break;
			}
			case Stmt::ContinueStmtClass:
			{
				std::cerr << "continue\n";
				self["type"] = "continue";
				self["id"] = statementid++;
				descend =false;
				parent.append(self);
				break;
			}
			case Stmt::BreakStmtClass:
			{
				std::cerr << "break\n";
				self["type"] = "break";
				self["id"] = statementid++;
				descend =false;
				parent.append(self);
				break;
			}
			case Stmt::WhileStmtClass:
			{
				// getConditionVariable () const -> VarDecl
				// getConditionVariableDeclStmt ()  -> DeclStmt
				// getCond -> Expr
				// getBody
				// getWhileLoc () const
				// getLocStart () const LLVM_READONLY
				const WhileStmt * C = cast<WhileStmt>(p);
				self["type"] = "while";
				self["id"] = statementid++;
				descend = false;
				MyVisitStmt(C->getBody(),level+1,self["children"]);
				parent.append(self);
				break;
			}
			case Stmt::DeclStmtClass:
			{
//				self["type"] = "decl";
				descend = false;
//				parent.append(self);
				break;
			}			
			case Stmt::OMPParallelDirectiveClass:
			{
				self["type"] = "ompparallel";
				break;
			}
			case Stmt::OMPParallelForDirectiveClass:
			{
				self["type"] = "ompparallelfor";
				break;
			}			
			case Stmt::OMPSectionsDirectiveClass:
			{
				self["type"] = "ompsections";
				break;
			}	
			case Stmt::OMPSectionDirectiveClass:
			{
				self["type"] = "ompsection";
				break;
			}					
			case Stmt::OMPSingleDirectiveClass:
			{
				self["type"] = "ompsingle";
				break;
			}		
			case Stmt::OMPTaskDirectiveClass:
			{
				self["type"] = "omptask";
				break;
			}			
			case Stmt::OMPTaskyieldDirectiveClass:
			{
				self["type"] = "omptaskyield";
				break;
			}	
			case Stmt::OMPTaskwaitDirectiveClass:
			{
				self["type"] = "omptaskwait";
				break;
			}					
			case Stmt::OMPTaskgroupDirectiveClass:
			{
				self["type"] = "omptaskgroup";
				break;				
			}
			case Stmt::OMPTaskLoopDirectiveClass:
			{
				self["type"] = "omptaskloop";
				break;				
			}			
			default:
			{
				if(isa<Expr>(p))
				{
					//std::cout << "\t\tunknown expr " << p->getStmtClass() << std::endl;
					self["id"] = statementid++;
					self["type"] = "expr";
					parent.append(self);
				}
				else
				std::cout << "\t\tunknown " << p->getStmtClass() << std::endl;
				descend = false;
				break;
			}
		}
		if(descend)
		{
			for (const Stmt *SubStmt : p->children())
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
			std::cout << "\tTraverseFunctionDecl missing body " << (std::string)fx->getName() << "\n";
			return true;
		}
		else
		{
			std::cout << "\tTraverseFunctionDecl body " << (std::string)fx->getName() << "\n";
			Json::Value self;
			self["id"] = statementid++;
			self["type"] = "function";
			self["name"] = (std::string)fx->getName();
			Stmt * p = fx->getBody();
			MyVisitStmt(p,0,self["children"]);
			root.append(self);
			return true; // no recursion
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
	int & statementid;
	Rewriter &TheRewriter;
	Json::Value& root;
	ASTContext * context;
};

// Implementation of the ASTConsumer interface for reading an AST produced
// by the Clang parser.
class MyASTConsumer : public ASTConsumer {
public:
	MyASTConsumer(Rewriter &R, ASTContext *A, CompilerInstance &CI,Json::Value& aroot, int & astatementid) : Visitor(R, A,aroot,astatementid),root(aroot) {}

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
		std::cout << "ctor MyFrontendAction\n";
	}

	~MyFrontendAction()
	{
		Json::StreamWriterBuilder wbuilder;
		wbuilder["indentation"] = "  ";
		std::string document = Json::writeString(wbuilder, root);
		std::ofstream onf(lastfile + ".ast.json");
		onf << document;
	}

	void EndSourceFileAction() override {
		SourceManager &SM = TheRewriter.getSourceMgr();
		//llvm::errs() << "** EndSourceFileAction for: "
		//           << SM.getFileEntryForID(SM.getMainFileID())->getName() << "\n";
		// Now emit the rewritten buffer.
		// OUTBUF TheRewriter.getEditBuffer(SM.getMainFileID()).write(llvm::outs());
	}

#ifdef CLANG37PLUS
	std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
	        StringRef file) override {
		lastfile = file;
		filecount++;
		llvm::errs() << "** Creating AST consumer for: " << file << "\n";
		TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
		return llvm::make_unique<MyASTConsumer>(TheRewriter, &CI.getASTContext(), CI,root[file],statementid);
	}
#else
	ASTConsumer* CreateASTConsumer(CompilerInstance &CI,
	                               StringRef file) override {
		llvm::errs() << "** Creating AST consumer for: " << file << "\n";
		TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
		return new MyASTConsumer(TheRewriter, &CI.getASTContext(), CI,root[file],statementid);
	}
#endif
private:
	std::string lastfile;
	int statementid = 1;
	int filecount = 0;
	Json::Value root;
	Rewriter TheRewriter;
};

void usage()
{
	printf("yextractmp tool by Emanuele Ruffaldi 2015-2016\nSyntax: yextractmp sourcefile.cpp [tooloptions] [-- [compileroptions]]\n-fopenmp is automatically added\n");
}

using namespace clang::tooling;
using namespace llvm;

static cl::OptionCategory MyToolCategory("my-tool options");

static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static cl::extrahelp MoreHelp("\nMore help text...");
//static cl::opt<bool> YourOwnOption("abc", cl::cat(MyToolCategory));
// Usage: llvm::outs() << YourOwnOption.getValue();

int main(int argc, const char **argv) {
	if(argc < 2)
	{
		usage();
		return -1;
	}
	std::vector<const char *> fargv;
	bool missingmm = true;

	for(int i = 0; i < argc; i++)
	{
		if(strcmp(argv[i],"--")==0)
			missingmm = false;
		fargv.push_back(argv[i]);
	}
	if(missingmm)
		fargv.push_back("--");
	fargv.push_back("-fopenmp");

	int iargc = fargv.size();
	CommonOptionsParser op(iargc, &fargv[0], MyToolCategory);
	ClangTool Tool(op.getCompilations(), op.getSourcePathList());

	// ClangTool::run accepts a FrontendActionFactory, which is then used to
	// create new objects implementing the FrontendAction interface. Here we use
	// the helper newFrontendActionFactory to create a default factory that will
	// return a new MyFrontendAction object every time.
	// To further customize this, we could create our own factory class.
	return Tool.run(newFrontendActionFactory<MyFrontendAction>().get());
}