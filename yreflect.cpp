/**
 * Clang Data Structure Reflector
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
/**
 * OutputType: enum or types declared inside another type are flattened top level
 *
 * MAYBE define namespace type
 */
class OType
{
public:
	static int progressive;
	enum class Type { Alias, Record, Primitive, Pointer, Reference, Array, Enum , MaxType};
	Type type;

	const char * getTypeAsString() const
	{
		static const char * names[] = {"alias", "record", "primitive", "pointer", "reference", "array", "enum"};
		static_assert(sizeof(names) / sizeof(names[0]) == (int)Type::MaxType, "type 2 string error");
		return names[(int)type];
	}

	OType() : iid(progressive++) {}

	std::string name; // basic name
	std::string scope; // namespace or full class
	std::string qualified; // with scope and original name
	std::shared_ptr<OType> detail; // optional templatebase for Record/Union, pointed for Alias, pointed for Pointer/Reference/Array, implementation for Enum
	std::vector<std::shared_ptr<OType> > bases; // parents
	int iid = 0;
	int itemscount = 0; // items for array
	int size = 0; // size in bytes
	bool named = false; // is named

	class OField
	{
	public:
		size_t offset_or_value; // for enum is value, for record is offset
		std::string name;
		std::shared_ptr<OType> type;
	};

	std::vector<OField> fields; // for Record or Union or Enum

	bool isunion = false; // for record

};
int OType::progressive = 1;


using namespace clang;
using namespace clang::driver;
using namespace clang::tooling;

static llvm::cl::OptionCategory ToolingSampleCategory("Tooling Sample");

struct TypeAccumulator
{
	std::vector<std::shared_ptr<OType> > types;
	std::map<std::string, std::shared_ptr<OType> > type2otype;
	std::map<clang::Decl *, std::shared_ptr<OType> > decl2otype;


	void DumpOut(Json::Value & root)
	{
		for (auto & t : types )
		{
			std::cout << t->getTypeAsString() << " n:" << t->name << " q:" << t->qualified << " s:" << t->size << " sub:" << (!t->detail ? "" : t->detail->qualified) << std::endl;

			Json::Value self;
			self["name"] =t->name;
			self["type"] =t->getTypeAsString();
			self["qualified"] = t->qualified;
			self["size"] = t->size;
			if(t->detail)
				self["itemtype"] = t->detail->iid;

			if (t->type == OType::Type::Record)
			{
				self["isunion"] = t->isunion;
				for(auto & x: t->fields)
				{
					Json::Value sub;
					sub["name"] = x.name;
					sub["offset"] = (int)x.offset_or_value;
					if(x.type)
						sub["type"] = x.type->iid; // TODO dump
					self["fields"].append(sub);
				}
			}
			else if(t->type == OType::Type::Array)
			{
				self["items"] = t->itemscount;
			}
			else if(t->type == OType::Type::Enum)
			{
				for(auto & x: t->fields)
				{
					Json::Value sub;
					sub["name"] = x.name;
					sub["value"] = (int)x.offset_or_value;
					self["fields"].append(sub);
				}
			}
			root[std::to_string(t->iid)] = self;
		}
	}

};

class MyASTVisitor : public RecursiveASTVisitor<MyASTVisitor> {
public:
	MyASTVisitor(Rewriter &R, ASTContext*A,TypeAccumulator &ata,Json::Value &aroot) : TheRewriter(R) , context(A),ta(ata),root(aroot) {}
	TypeAccumulator &ta;

	std::string DecodeType(clang::QualType t)
	{
		uint64_t array_count = 0;
		int isarray = 0;
		// Get type info for the parameter
		clang::QualType ctype(t);

		// If this is an array of constant size, strip the size from the type and store it in the parameter info
		if (const clang::ConstantArrayType* array_type = dyn_cast<clang::ConstantArrayType>(ctype))
		{
			array_count = *array_type->getSize().getRawData();
			isarray = 1;
			ctype = array_type->getElementType();
		}

		std::string tname = ctype.getAsString();

		// Only handle one level of recursion for pointers and references

		// Get pointee type info if this is a pointer
		if (const clang::PointerType* ptr_type = dyn_cast<clang::PointerType>(ctype))
		{
			return "ptr-not-supported";
			// ctype.Set(ptr_type->getPointeeType());
		}

		// Get pointee type info if this is a reference
		else if (const clang::LValueReferenceType* ref_type = dyn_cast<clang::LValueReferenceType>(ctype))
		{
			return "ref-not-supported";
			//ctype.Set(ref_type->getPointeeType());
		}
		else if (const clang::TemplateSpecializationType* ts = dyn_cast<clang::TemplateSpecializationType>(ctype))
		{

			// HOW TO GET THE CONCRETE TYPE?
			if (ts->desugar().getTypePtr() != ctype.getTypePtr())
				return tname + " is template: " + DecodeType(ts->desugar());
			else
				return tname + " PARTIAL";
		}
		else if (const clang::RecordType* ts = dyn_cast<clang::RecordType>(ctype))
		{

			tname += " is record";
			clang::RecordDecl *red = ts->getDecl();
			if (clang::ClassTemplateSpecializationDecl * csd = dyn_cast<clang::ClassTemplateSpecializationDecl>(red))
			{
				std::string name = csd->getDeclName().getAsString();
				tname += " " + name + " speckind " + std::to_string(csd->getSpecializationKind());
			}
			else if (clang::NamedDecl * nd = dyn_cast<clang::NamedDecl>(red))
			{
				std::string name = nd->getDeclName().getAsString();
				tname += " " + name + " any " + nd->getDeclKindName();
			}
			//DescendRecord(ts);
			return tname;
		}

		// Is this a field that can be safely recorded?
		clang::Type::TypeClass tc = ctype->getTypeClass();
		switch (tc)
		{
		case clang::Type::Builtin:
		case clang::Type::Enum:
		case clang::Type::Elaborated:
		case clang::Type::Record:
			break;
		default:
		{
			std::ostringstream ons;

			ons << "unsupported-class " << (int)tc << std::endl;

		}
		}

		//const clang::ClassTemplateDecl* template_decl = cts_decl->getSpecializedTemplate();
		//type_name_str = template_decl->getQualifiedNameAsString(); //consumer.GetPrintingPolicy());
		return tname;
	}

	std::shared_ptr<OType> DecodeOType(clang::QualType t)
	{
		// Get type info for the parameter
		clang::QualType ctype(t);
		std::string tname = ctype.getAsString();

		std::cout << "DecodeOType " << tname << std::endl;

		// If this is an array of constant size, strip the size from the type and store it in the parameter info
		if (const clang::ConstantArrayType* array_type = dyn_cast<clang::ConstantArrayType>(ctype))
		{
			std::shared_ptr<OType> out(new OType());
			out->type = OType::Type::Array;
			out->size = *array_type->getSize().getRawData();
			out->detail = solveouttype(array_type->getElementType());
			ta.types.push_back(out);
			return out;
		}
		else if (const clang::PointerType* ptr_type = dyn_cast<clang::PointerType>(ctype))
		{
			std::shared_ptr<OType> out(new OType());
			out->type = OType::Type::Pointer;
			out->detail = solveouttype(ptr_type->getPointeeType());
			ta.types.push_back(out);
			return out;
		}
		else if (const clang::LValueReferenceType* ref_type = dyn_cast<clang::LValueReferenceType>(ctype))
		{
			return std::shared_ptr<OType>(0);
			std::shared_ptr<OType> out(new OType());
			out->type = OType::Type::Reference;
			out->detail = solveouttype(ptr_type->getPointeeType());
			ta.types.push_back(out);
			return out;
			//ctype.Set(ref_type->getPointeeType());
		}
		else if (const clang::TemplateSpecializationType* ts = dyn_cast<clang::TemplateSpecializationType>(ctype))
		{
			//std::cout << "TemplateSpecializationType\n";
			// HOW TO GET THE CONCRETE TYPE?
			if (ts->desugar().getTypePtr() != ctype.getTypePtr())
				return DecodeOType(ts->desugar());
			else
				return std::shared_ptr<OType>(0);
		}
		else if (const clang::RecordType* ts = dyn_cast<clang::RecordType>(ctype))
		{
			return std::shared_ptr<OType>(0);

			tname += " is record";
			std::string qn;
			clang::RecordDecl *red = ts->getDecl();
			if (clang::ClassTemplateSpecializationDecl * csd = dyn_cast<clang::ClassTemplateSpecializationDecl>(red))
			{
				std::string name = csd->getDeclName().getAsString();
				tname += " " + name + " speckind " + std::to_string(csd->getSpecializationKind());
			}
			else if (clang::NamedDecl * nd = dyn_cast<clang::NamedDecl>(red))
			{
				std::string name = nd->getDeclName().getAsString();
				tname += " " + name + " any " + nd->getDeclKindName();
			}
			//std::cout << "RecordType " << tname;
			auto r = DescendRecord(ts);
			r->qualified = red->getQualifiedNameAsString();
			return r;
		}
		else if (const clang::BuiltinType* ts = dyn_cast<clang::BuiltinType>(ctype))
		{
			// TODO: deduplicate
			clang::LangOptions LangOpts;
			LangOpts.CPlusPlus = true;
			clang::PrintingPolicy Policy(LangOpts);

			std::string qualified = "_builtin_" + std::string(ts->getName(Policy));
			auto it = ta.type2otype.find(qualified);
			if (it == ta.type2otype.end())
			{
				std::shared_ptr<OType> out(new OType());
				out->type = OType::Type::Primitive;
				out->name = ts->getName(Policy);
				out->qualified = out->name;
#ifdef CLANG37PLUS
				out->size = context->getTypeInfo(ts).Width / 8;
#else
				out->size = context->getTypeInfo(ts).first / 8;
#endif
				ta.type2otype[qualified] = out;
				ta.types.push_back(out);
				return out;
			}
			else
				return it->second;

		}
		else
		{
			std::cout << "\tUNKNOWN TYPE\n";
			return std::shared_ptr<OType>(0);
		}

		// Is this a field that can be safely recorded?
		clang::Type::TypeClass tc = ctype->getTypeClass();
		switch (tc)
		{
		case clang::Type::Builtin:

		case clang::Type::Enum:
		case clang::Type::Elaborated:
		case clang::Type::Record:
			break;
		default:
			{
				std::ostringstream ons;

				ons << "unsupported-class " << (int)tc << std::endl;
			}
		}

		//const clang::ClassTemplateDecl* template_decl = cts_decl->getSpecializedTemplate();
		//type_name_str = template_decl->getQualifiedNameAsString(); //consumer.GetPrintingPolicy());
		return std::shared_ptr<OType>(0);
	}
	/// from top level
	bool TraverseCXXRecordDecl(CXXRecordDecl * red)
	{
		DescendRecord(red, "");
		return true;
	}

	bool TraverseTypeAliasDecl(TypeAliasDecl * tad)
	{
		DoTypeAliasDecl(tad, "");
		return true;
	}

	bool TraverseFunctionDecl(FunctionDecl * fx)
	{
		return false; // no recursion
	}


	std::shared_ptr<OType> solveouttype( clang::QualType  t)
	{
		// check cache or new
		auto it = ta.type2otype.find(t.getAsString());
		if (it == ta.type2otype.end())
		{
			std::shared_ptr<OType> r = DecodeOType(t);
			ta.type2otype[t.getAsString()] = r;
			return r;
		}
		else
			return it->second;
	}

	bool DoTypedefDecl(TypedefDecl * tad, std::string ss = "")
	{
		std::cout << ss  << " TypedefDecl  " << std::flush << tad->getQualifiedNameAsString() << " " << DecodeType(tad->getUnderlyingType()) << std::endl;
		auto x = DecodeOType(tad->getUnderlyingType());
		if (x) // can be partial
		{
			std::shared_ptr<OType> out(new OType());
			out->name = tad->getName();
			out->type = OType::Type::Alias;
			out->named = true;
			out->detail = x;
			out->size = out->detail ? out->detail->size : 0;
			out->qualified = tad->getQualifiedNameAsString();
			ta.types.push_back(out);
		}
		return true;
	}

	bool DoTypeAliasDecl(TypeAliasDecl * tad, std::string ss)
	{
		// as Typedef
		std::cout << ss  << " AliasDecl " << std::flush << tad->getQualifiedNameAsString() << " " << DecodeType(tad->getUnderlyingType()) << std::endl;
		auto x = DecodeOType(tad->getUnderlyingType());
		if (x) // can be partial
		{
			std::shared_ptr<OType> out(new OType());
			out->name = tad->getName();
			out->type = OType::Type::Alias;
			out->named = true;
			out->detail = x;
			out->size = out->detail ? out->detail->size : 0;
			out->qualified = tad->getQualifiedNameAsString();
			ta.types.push_back(out);
		}
		return true;
	}

	bool DescendDecl(Decl * s, std::string ss)
	{
		if (isa<TypeAliasDecl>(s))
		{
			DoTypeAliasDecl(cast<TypeAliasDecl>(s), ss);
			return true;
		}
		else if (isa<TypedefDecl>(s))
		{
			DoTypedefDecl(cast<TypedefDecl>(s), ss);
			return true;
		}
		else if (isa<ClassTemplateSpecializationDecl>(s))
		{
			ClassTemplateSpecializationDecl * ca = cast<ClassTemplateSpecializationDecl>(s);
			//std::cout << ss << " !!!SPEC " << ca->getQualifiedNameAsString()  << std::endl;
			// << " " << DecodeType(clang::QualType(ca->getTypeForDecl(),0)) << std::endl;
			DecodeOType(clang::QualType(ca->getTypeForDecl(), 0));
			return true;
		}
		else if (isa<ClassTemplateDecl>(s))
		{
			// append for dummy
		}
		else if (isa<CXXRecordDecl>(s))
		{
			DescendRecord(cast<CXXRecordDecl>(s), ss);
			return true;
		}
		else if (isa<EnumDecl>(s)) {


			EnumDecl * enum_decl = cast<EnumDecl>(s);
			//std::cout << ss << " EnumDecl " << enum_decl->getQualifiedNameAsString() << " " << DecodeType(enum_decl->getIntegerType()) << std::endl;

			std::shared_ptr<OType> out(new OType());
			out->name = enum_decl->getName();
			out->type = OType::Type::Enum;
			out->qualified = enum_decl->getQualifiedNameAsString();
			out->named = true;
			out->detail = solveouttype(enum_decl->getIntegerType());
			out->size = out->detail ? out->detail->size : 0;
			ta.types.push_back(out);

			for (clang::EnumDecl::enumerator_iterator i = enum_decl->enumerator_begin(); i != enum_decl->enumerator_end(); ++i)
			{
				clang::EnumConstantDecl* constant_decl = *i;

				// Strip out the raw 64-bit value - the compiler will automatically modify any values
				// greater than 64-bits without having to worry about that here
				llvm::APSInt value = constant_decl->getInitVal();
				int value_int = (int)value.getRawData()[0];

				// Clang doesn't construct the enum name as a C++ compiler would see it so do that first
				// NOTE: May want to revisit this later
				std::string constant_name = constant_decl->getNameAsString();
				//std::cout << "\tENUM " << constant_name << " " << value_int << std::endl;
				OType::OField of;
				of.offset_or_value = value_int;
				of.name = constant_decl->getNameAsString();
				out->fields.push_back(of);
			}
			return true;
		}
		else if (isa<FieldDecl>(s)) {
			FieldDecl *fd = cast<FieldDecl>(s);
			//std::cout << ss << "!!! FieldDecl " << fd->getQualifiedNameAsString() << " " << std::endl;
			//std::cout << "\tINDEX " << fd->getFieldIndex() << std::endl;

			// TODO: canonicalize: std::bitset<toUType(Capability::NUM)>
			//std::cout << "\tTYPE " << DecodeType(fd->getType()) << std::endl;
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

	/**
	 * Description of a record type
	 */
	std::shared_ptr<OType> DescendRecord(const clang::RecordType * rt)
	{
		clang::CXXRecordDecl *red = dyn_cast<clang::CXXRecordDecl>(rt->getDecl());
		if (!red)
		{
			//std::cout << "no cxxrecord\n";
			return std::shared_ptr<OType>(0);
		}
		else if (!red->getDefinition())
		{
			//std::cout << "foward\n";
			return std::shared_ptr<OType>(0);
		}
		std::ostringstream ox;
		const clang::ASTRecordLayout& layout = context->getASTRecordLayout(red);
		ox << " SIZE " << layout.getSize().getQuantity();
		if (red->getNumVBases())
		{
			//std::cout << "vbases\n";
			return std::shared_ptr<OType>(0);
		}
		else if (red->isDependentType())
		{
			//std::cout << "isdep\n";
			return std::shared_ptr<OType>(0);
		}

		std::shared_ptr<OType> out(new OType());
		out->name = red->getName();
		out->qualified = red->getQualifiedNameAsString();
		out->type = OType::Type::Record;
		out->named = true;
		out->size = layout.getSize().getQuantity();
		//out->alignment = layout.getAlignment().getQuantity();
		ta.types.push_back(out);

		if (red->getNumBases())
		{
			for (clang::CXXRecordDecl::base_class_const_iterator base_it = red->bases_begin(); base_it != red->bases_end(); base_it++)
			{
				auto t = base_it->getType();
				out->bases.push_back(solveouttype(t));
				ox << " BASE " << t.getAsString();
			}
		}

		for (int i = 0; i < layout.getFieldCount(); i++)
		{
			OType::OField f;
			f.offset_or_value = layout.getFieldOffset(i);
			out->fields.push_back(f);
			ox << " FIELD " << layout.getFieldOffset(i);
		}

		int ifield = 0;
		clang::DeclContext* decl_context = red->castToDeclContext(red);
		//const clang::ASTRecordLayout& layout = m_ASTContext.getASTRecordLayout(record_decl);
		for (clang::DeclContext::decl_iterator i = decl_context->decls_begin(); i != decl_context->decls_end(); ++i)
		{
			switch (i->getKind())
			{
			case clang::Decl::AccessSpec:
			case clang::Decl::Friend:
			case clang::Decl::CXXConstructor:
			case clang::Decl::CXXConversion:
			case clang::Decl::CXXMethod:
			case clang::Decl::CXXDestructor:
			case clang::Decl::FunctionTemplate:
				break;
			case clang::Decl::CXXRecord: // ENTER!
			case clang::Decl::Typedef: // ENTER!
			case clang::Decl::Enum: // ENTER!
			{
				DescendDecl(*i, "");
			}
			break;
			case clang::Decl::StaticAssert:
				continue;
			case clang::Decl::Field:
			{
				clang::FieldDecl* field_decl = dyn_cast<clang::FieldDecl>(*i);
				out->fields[ifield].name = field_decl->getName();
				std::cout << "Solving field " << out->fields[ifield].name << std::endl;
				out->fields[ifield].type = solveouttype(field_decl->getType());
				std::cout << "\tSolved field " << out->fields[ifield].type << std::endl;
			}
			ifield++;
			break;
			default:
				break;
			}

#if 0
			if (isa<clang::NamedDecl>(*i))
			{
				clang::NamedDecl* named_decl = cast<clang::NamedDecl>(*i);
				DescendDecl(named_decl, ss + "/" + red->getQualifiedNameAsString());
				// VisitDecl does it
			}
			else
			{
				// UNS AccessSpec,Friend
				switch (i->getKind())
				{
				case clang::Decl::AccessSpec:
				case clang::Decl::Friend:
					continue;
				default:
					break;
				}
				std::cout << ss + " unsupported decl " << i->getDeclKindName() << std::endl;
			}
#endif


			// Var
			// Field
			// TODO CXXRecord
			// TODO Enum
			// TODO Typedef
			if (clang::NamedDecl* named_decl = dyn_cast<clang::NamedDecl>(*i))
			{
				ox << " NAMED " << named_decl->getNameAsString() << "!" << named_decl->getDeclKindName();
				//DescendDecl(named_decl, ss + "/" + red->getQualifiedNameAsString());
				// VisitDecl does it
			}
			else
				ox << " UNKNOWNSUB " << i->getDeclKindName();
		}
		//std::cout << ox.str();
		return std::shared_ptr<OType>(out);

	}

	bool DescendRecord(CXXRecordDecl * red, std::string ss, bool te = false)
	{
		if (!red->getDefinition() ||

		        (red->isThisDeclarationADefinition() == clang::VarDecl::DeclarationOnly && !red->isFreeStanding()))
		{
			//std::cout << "DECLONLY " << red->getDeclName().getAsString() << "\n";
			return  true;
		}
		if (red->isDependentType())
		{
			//std::cout << "deptype\n";
			return true;
		}

		//std::cout << ss << " CXXRecordDecl " << red->getQualifiedNameAsString() << std::endl;
		if (red->getNumVBases())
		{
			//std::cout << "has vbases or multipel inheritance\n";
			return true;
		}
		DescendRecord(dyn_cast<RecordType>(red-> 	getTypeForDecl()));
		return true;
	}

	bool VisitDecl(Decl *s) {
		return DescendDecl(s, "");
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
	MyASTConsumer(Rewriter &R, ASTContext *A, CompilerInstance &CI,Json::Value& aroot) : Visitor(R, A,ta,aroot),root(aroot) {}

	void HandleTranslationUnit(ASTContext &Context) override {
		/* we can use ASTContext to get the TranslationUnitDecl, which is
		   a single Decl that collectively represents the entire source file */

		if (CI.hasDiagnostics() && CI.getDiagnostics().hasFatalErrorOccurred())
			return;
		Visitor.TraverseDecl(Context.getTranslationUnitDecl());

		std::cout << "result: created " << ta.types.size() << " types\n";
		ta.DumpOut(root);
		std::cout << std::endl;

		// now emit the output
		// or filter
	}

private:
	TypeAccumulator ta;
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
		wbuilder["indentation"] = "\t";
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

#ifdef CLANG37PLUS
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
	printf("yextract tool by Emanuele Ruffaldi 2015\nSyntax: yextract sourcefile.cpp [tooloptions] -- [compileroptions]\n");
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