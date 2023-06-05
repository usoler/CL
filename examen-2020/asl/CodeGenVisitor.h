//////////////////////////////////////////////////////////////////////
//
//    CodeGenVisitor - Walk the parser tree to do
//                     the generation of code
//
//    Copyright (C) 2017-2022  Universitat Politecnica de Catalunya
//
//    This library is free software; you can redistribute it and/or
//    modify it under the terms of the GNU General Public License
//    as published by the Free Software Foundation; either version 3
//    of the License, or (at your option) any later version.
//
//    This library is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//    Affero General Public License for more details.
//
//    You should have received a copy of the GNU Affero General Public
//    License along with this library; if not, write to the Free Software
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
//
//    contact: José Miguel Rivero (rivero@cs.upc.edu)
//             Computer Science Department
//             Universitat Politecnica de Catalunya
//             despatx Omega.110 - Campus Nord UPC
//             08034 Barcelona.  SPAIN
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "antlr4-runtime.h"
#include "AslBaseVisitor.h"

#include "../common/TypesMgr.h"
#include "../common/SymTable.h"
#include "../common/TreeDecoration.h"
#include "../common/code.h"

#include <string>

// using namespace std;


//////////////////////////////////////////////////////////////////////
// Class CodeGenVisitor: derived from AslBaseVisitor.
// The tree visitor go through the parser tree and call the methods of
// this class to generate the code of the program. This is done
// once the SymbolsVisitor and TypeCheckVisitor have finish with no
// semantic error. So all the symbols of the program has been added to
// their respective scope and the type of each expresion has also be
// computed and decorate the parse tree. In this visit, if some node/method
// does not have an associated task, it does not have to be visited/called
// so no redefinition is needed.

class CodeGenVisitor final : public AslBaseVisitor {

public:

  // Constructor
  CodeGenVisitor(TypesMgr       & Types,
                 SymTable       & Symbols,
                 TreeDecoration & Decorations);

  // Methods to visit each kind of node:
  
  // Program rules
  antlrcpp::Any visitProgram(AslParser::ProgramContext *ctx);
  
  // Function rules
  antlrcpp::Any visitFunction(AslParser::FunctionContext *ctx);
  antlrcpp::Any visitParameter(AslParser::ParameterContext *ctx);
  
  // Declaration rules
  antlrcpp::Any visitDeclarations(AslParser::DeclarationsContext *ctx);
  antlrcpp::Any visitVariable_decl(AslParser::Variable_declContext *ctx);
  
  // Statement rules
  antlrcpp::Any visitStatements(AslParser::StatementsContext *ctx);
  antlrcpp::Any visitAssignStmt(AslParser::AssignStmtContext *ctx);
  antlrcpp::Any visitIfStmt(AslParser::IfStmtContext *ctx);
  antlrcpp::Any visitWhileStmt(AslParser::WhileStmtContext *ctx); // NEW
  antlrcpp::Any visitProcCall(AslParser::ProcCallContext *ctx);
  antlrcpp::Any visitReadStmt(AslParser::ReadStmtContext *ctx);
  antlrcpp::Any visitWriteExpr(AslParser::WriteExprContext *ctx);
  antlrcpp::Any visitWriteString(AslParser::WriteStringContext *ctx);
  antlrcpp::Any visitReturnStmt(AslParser::ReturnStmtContext *ctx); // NEW
  
  // Left expr rules
  antlrcpp::Any visitLeft_expr(AslParser::Left_exprContext *ctx);
  
  // Expr rules
  antlrcpp::Any visitParenthesis(AslParser::ParenthesisContext *ctx); // NEW
  antlrcpp::Any visitExprFunc(AslParser::ExprFuncContext *ctx); // NEW
  antlrcpp::Any visitUnary(AslParser::UnaryContext *ctx); // NEW
  antlrcpp::Any visitArithmetic(AslParser::ArithmeticContext *ctx);
  antlrcpp::Any visitRelational(AslParser::RelationalContext *ctx);
  antlrcpp::Any visitLogical(AslParser::LogicalContext *ctx); // NEW
  antlrcpp::Any visitArrayAccess(AslParser::ArrayAccessContext *ctx);
  antlrcpp::Any visitValue(AslParser::ValueContext *ctx);
  antlrcpp::Any visitExprIdent(AslParser::ExprIdentContext *ctx);
  antlrcpp::Any visitSum(AslParser::SumContext *ctx);
  antlrcpp::Any visitFilter(AslParser::FilterContext *ctx);
  
  // Ident rules
  antlrcpp::Any visitIdent(AslParser::IdentContext *ctx);
  
  // Function_call rules
  antlrcpp::Any visitFunction_call(AslParser::Function_callContext *ctx); // NEW

private:

  // Attributes
  TypesMgr        & Types;
  SymTable        & Symbols;
  TreeDecoration  & Decorations;
  counters          codeCounters;
  // Current function type (assigned before visit its instructions)
  TypesMgr::TypeId currFunctionType;

  // Accessor/Mutator to the type (TypeId) of the current function
  TypesMgr::TypeId getCurrentFunctionTy ()                      const;
  void             setCurrentFunctionTy (TypesMgr::TypeId type);

  // Getters for the necessary tree node atributes:
  //   Scope and Type
  SymTable::ScopeId getScopeDecor (antlr4::ParserRuleContext *ctx) const;
  TypesMgr::TypeId  getTypeDecor  (antlr4::ParserRuleContext *ctx) const;


  //////////////////////////////////////////////////////////////////
  // Class CodeAttribs: is declared inside CodeGenVisitor as an
  // auxiliary class to group the three attributes necessaries for
  // code generation (address, offset, instructions list).
  // Some language constructions, for example expressions, can
  // generate the three attributes. Others, like statements, only
  // generate the instruction list.
  class CodeAttribs {
    
  public:
    // Constructors
    CodeAttribs(const std::string & addr,
                const std::string & offs,
                instructionList & code);
    CodeAttribs(const std::string & addr,
                const std::string & offs,
                instructionList && code);

    // Attributes (publics):
    //   - the address that will hold the value of an expression
    std::string addr;
    //   - the offset applied to the address (for array access)
    std::string offs;
    //   - the three-address code associated to an statement/expression
    instructionList code;

  };  // class CodeAttribs
  
};  // class CodeGenVisitor
