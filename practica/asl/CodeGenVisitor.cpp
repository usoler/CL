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

#include "CodeGenVisitor.h"
#include "antlr4-runtime.h"

#include "../common/TypesMgr.h"
#include "../common/SymTable.h"
#include "../common/TreeDecoration.h"
#include "../common/code.h"

#include <string>
#include <cstddef>    // std::size_t

// uncomment the following line to enable debugging messages with DEBUG*
// #define DEBUG_BUILD
#include "../common/debug.h"

// using namespace std;


// Constructor
CodeGenVisitor::CodeGenVisitor(TypesMgr       & Types,
                               SymTable       & Symbols,
                               TreeDecoration & Decorations) :
  Types{Types},
  Symbols{Symbols},
  Decorations{Decorations} {
}

// Accessor/Mutator to the attribute currFunctionType
TypesMgr::TypeId CodeGenVisitor::getCurrentFunctionTy() const {
  return currFunctionType;
}

void CodeGenVisitor::setCurrentFunctionTy(TypesMgr::TypeId type) {
  currFunctionType = type;
}

// Methods to visit each kind of node:
//

// Program rules ------------------------------------------------------------------------------------------------------------
antlrcpp::Any CodeGenVisitor::visitProgram(AslParser::ProgramContext *ctx) {
  DEBUG_ENTER();
  
  code my_code;
  SymTable::ScopeId sc = getScopeDecor(ctx);
  Symbols.pushThisScope(sc);
  
  for (auto ctxFunc : ctx->function()) { 
    subroutine subr = visit(ctxFunc);
    my_code.add_subroutine(subr);
  }
  
  Symbols.popScope();
  
  DEBUG_EXIT();
  return my_code;
}

// Function rules ----------------------------------------------------------------------------------------------------------
antlrcpp::Any CodeGenVisitor::visitFunction(AslParser::FunctionContext *ctx) {
  DEBUG_ENTER();
  
  SymTable::ScopeId sc = getScopeDecor(ctx);
  Symbols.pushThisScope(sc);
  
  subroutine subr(ctx->ID()->getText());
  codeCounters.reset();
  
  // Return
  if (ctx->basic_type()) {
    TypesMgr::TypeId tRet = getTypeDecor(ctx->basic_type());
    subr.add_param("_result", Types.to_string(tRet));
  }
  
  // Parameters
  for (auto ctxParam : ctx->parameter()) {
    var param = visit(ctxParam);
    subr.add_param(param.name, param.type);
  }  
  
  // Declarations
  std::vector<var> && lvars = visit(ctx->declarations());
  for (auto & onevar : lvars) {
    subr.add_var(onevar);
  }
  
  // Statements
  instructionList && code = visit(ctx->statements());
  code = code || instruction(instruction::RETURN());
  subr.set_instructions(code);
  
  Symbols.popScope();
  
  DEBUG_EXIT();
  return subr;
}

antlrcpp::Any CodeGenVisitor::visitParameter(AslParser::ParameterContext *ctx) {
  DEBUG_ENTER();
  
  TypesMgr::TypeId   t1 = getTypeDecor(ctx->type());
  std::size_t      size = Types.getSizeOfType(t1);
  
  DEBUG_EXIT();
  return var{ctx->ID()->getText(), Types.to_string(t1), size};
}

// Declaration rules --------------------------------------------------------------------------------------------------------
antlrcpp::Any CodeGenVisitor::visitDeclarations(AslParser::DeclarationsContext *ctx) {
  DEBUG_ENTER();
  
  std::vector<var> lvars;
  for (auto & varDeclCtx : ctx->variable_decl()) {
    std::vector<var> onevars = visit(varDeclCtx);
    for (auto & onevar : onevars) {
      lvars.push_back(onevar);
    }
  }
  
  DEBUG_EXIT();
  return lvars;
}

antlrcpp::Any CodeGenVisitor::visitVariable_decl(AslParser::Variable_declContext *ctx) {
  DEBUG_ENTER();
  
  TypesMgr::TypeId   t1 = getTypeDecor(ctx->type());
  std::size_t      size = Types.getSizeOfType(t1);
  
  std::vector<var> variables;
  for (auto & id : ctx->ID()) {
    var variable = var{id->getText(), Types.to_string(t1), size};
    variables.push_back(variable);
  }
  
  DEBUG_EXIT();
  return variables;
}

// Statement rules ----------------------------------------------------------------------------------------------------------
antlrcpp::Any CodeGenVisitor::visitStatements(AslParser::StatementsContext *ctx) {
  DEBUG_ENTER();
  
  instructionList code;
  for (auto stCtx : ctx->statement()) {
    instructionList && codeS = visit(stCtx);
    code = code || codeS;
  }
  
  DEBUG_EXIT();
  return code;
}

antlrcpp::Any CodeGenVisitor::visitAssignStmt(AslParser::AssignStmtContext *ctx) {
  DEBUG_ENTER();
  
  instructionList code;
  CodeAttribs     && codAtsE1 = visit(ctx->left_expr());
  std::string           addr1 = codAtsE1.addr;
  // std::string           offs1 = codAtsE1.offs;
  instructionList &     code1 = codAtsE1.code;
  // TypesMgr::TypeId tid1 = getTypeDecor(ctx->left_expr());
  CodeAttribs     && codAtsE2 = visit(ctx->expr());
  std::string           addr2 = codAtsE2.addr;
  // std::string           offs2 = codAtsE2.offs;
  instructionList &     code2 = codAtsE2.code;
  // TypesMgr::TypeId tid2 = getTypeDecor(ctx->expr());
  code = code1 || code2 || instruction::LOAD(addr1, addr2);
  
  DEBUG_EXIT();
  return code;
}

antlrcpp::Any CodeGenVisitor::visitIfStmt(AslParser::IfStmtContext *ctx) {
  DEBUG_ENTER();
  
  instructionList code;
  
  CodeAttribs     && codAtsE = visit(ctx->expr());
  std::string          addr1 = codAtsE.addr;
  instructionList &    code1 = codAtsE.code;
  
  instructionList &&   code2 = visit(ctx->statements(0));
  
  // TODO: MISSING 'ELSE' CODE
  
  std::string label = codeCounters.newLabelIF();
  std::string labelEndIf = "endif" + label;
  
  code = code1 || instruction::FJUMP(addr1, labelEndIf) ||
         code2 || instruction::LABEL(labelEndIf);
         
  DEBUG_EXIT();
  return code;
}

antlrcpp::Any CodeGenVisitor::visitWhileStmt(AslParser::WhileStmtContext *ctx) {
  DEBUG_ENTER();
  
  instructionList code;
  
  CodeAttribs     && codAtsE = visit(ctx->expr());
  std::string          addr1 = codAtsE.addr;
  instructionList &    code1 = codAtsE.code;
  
  instructionList &&   code2 = visit(ctx->statements());
  
  std::string label = codeCounters.newLabelWHILE();
  std::string labelWhile = "while" + label;
  std::string labelEndWhile = "endwhile" + label;
  
  code = instruction::LABEL(labelWhile) || code1 || instruction::FJUMP(addr1, labelEndWhile) || code2 || instruction::UJUMP(labelWhile) || instruction::LABEL(labelEndWhile);
         
  DEBUG_EXIT();
  return code;
}

antlrcpp::Any CodeGenVisitor::visitProcCall(AslParser::ProcCallContext *ctx) {
  DEBUG_ENTER();
  
  instructionList code;
  // std::string name = ctx->ident()->ID()->getSymbol()->getText();
  //std::string name = ctx->ident()->getText();
  //code = instruction::CALL(name);
  
  DEBUG_EXIT();
  return code;
}

antlrcpp::Any CodeGenVisitor::visitReadStmt(AslParser::ReadStmtContext *ctx) {
  DEBUG_ENTER();
  CodeAttribs     && codAtsE = visit(ctx->left_expr());
  std::string          addr1 = codAtsE.addr;
  // std::string          offs1 = codAtsE.offs;
  instructionList &    code1 = codAtsE.code;
  instructionList &     code = code1;
  // TypesMgr::TypeId tid1 = getTypeDecor(ctx->left_expr());
  code = code1 || instruction::READI(addr1);
  DEBUG_EXIT();
  return code;
}

antlrcpp::Any CodeGenVisitor::visitWriteExpr(AslParser::WriteExprContext *ctx) {
  DEBUG_ENTER();
  
  CodeAttribs     && codAt1 = visit(ctx->expr());
  std::string         addr1 = codAt1.addr;
  // std::string         offs1 = codAt1.offs;
  instructionList &   code1 = codAt1.code;
  
  instructionList &    code = code1;
    
  TypesMgr::TypeId tid1 = getTypeDecor(ctx->expr());
  if (Types.isFloatTy(tid1)) {
    code = code1 || instruction::WRITEF(addr1);
  } else if (Types.isCharacterTy(tid1)) {
    code = code1 || instruction::WRITEC(addr1);
  } else {
    code = code1 || instruction::WRITEI(addr1); 
  }
  
  DEBUG_EXIT();
  return code;
}

antlrcpp::Any CodeGenVisitor::visitWriteString(AslParser::WriteStringContext *ctx) {
  DEBUG_ENTER();
  
  instructionList code;
  std::string s = ctx->STRING()->getText();
  code = code || instruction::WRITES(s);
  
  DEBUG_EXIT();
  return code;
}

antlrcpp::Any CodeGenVisitor::visitReturnStmt(AslParser::ReturnStmtContext *ctx) {
  DEBUG_ENTER();
  
  instructionList code = instructionList();
  
  if (ctx->expr()) {
    CodeAttribs     && codAt1 = visit(ctx->expr());
    std::string         addr1 = codAt1.addr;
    instructionList &   code1 = codAt1.code;
    
    code = code || code1 || instruction::LOAD("_result", addr1) || instruction::RETURN();
  }
           
  DEBUG_EXIT();
  return code;
}

// Left expr rules ----------------------------------------------------------------------------------------------------------
antlrcpp::Any CodeGenVisitor::visitLeftExprIdent(AslParser::LeftExprIdentContext *ctx) {
  DEBUG_ENTER();
  CodeAttribs && codAts = visit(ctx->ident());
  DEBUG_EXIT();
  return codAts;
}

antlrcpp::Any CodeGenVisitor::visitLeftArrayAccess(AslParser::LeftArrayAccessContext *ctx) {
  DEBUG_ENTER();
  CodeAttribs && codAts = visit(ctx->array());
  DEBUG_EXIT();
  return codAts;
}

// Expr rules ---------------------------------------------------------------------------------------------------------------

antlrcpp::Any CodeGenVisitor::visitParenthesis(AslParser::ParenthesisContext *ctx) {
  DEBUG_ENTER();
  CodeAttribs && codAts = visit(ctx->expr());
  DEBUG_EXIT();
  return codAts;
}

antlrcpp::Any CodeGenVisitor::visitExprFunc(AslParser::ExprFuncContext *ctx) {
  DEBUG_ENTER();
  CodeAttribs && codAts = visit(ctx->function_call());
  DEBUG_EXIT();
  return codAts;
}

antlrcpp::Any CodeGenVisitor::visitUnary(AslParser::UnaryContext *ctx) {
  DEBUG_ENTER();
  
  CodeAttribs     && codAts1 = visit(ctx->expr());
  std::string          addr = codAts1.addr;
  instructionList &    code = codAts1.code;
  
  TypesMgr::TypeId t1 = getTypeDecor(ctx->expr());
  
  std::string temp = "%" + codeCounters.newTEMP();
  
  if (ctx->NOT()) {
    code = code || instruction::NOT(temp, addr);
  } else if (ctx->PLUS()) {
    code = code;
    temp = addr;
  } else if (ctx->MINUS()) {
    if (Types.isFloatTy(t1)) {
      // Negacion de floats
      code = code || instruction::FNEG(temp, addr);
    } else {
      // Negacion de enteros
      code = code || instruction::NEG(temp, addr);
    }
  }
  
  CodeAttribs codAts(temp, "", code);
  
  DEBUG_EXIT();
  return codAts;
}

antlrcpp::Any CodeGenVisitor::visitArithmetic(AslParser::ArithmeticContext *ctx) {
  DEBUG_ENTER();
  
  // Left expression
  CodeAttribs     && codAt1 = visit(ctx->expr(0));
  std::string         addr1 = codAt1.addr;
  instructionList &   code1 = codAt1.code;
  
  // Right expression
  CodeAttribs     && codAt2 = visit(ctx->expr(1));
  std::string         addr2 = codAt2.addr;
  instructionList &   code2 = codAt2.code;
  
  instructionList &&   code = code1 || code2;
  
  TypesMgr::TypeId t1 = getTypeDecor(ctx->expr(0));
  TypesMgr::TypeId t2 = getTypeDecor(ctx->expr(1));
  // TypesMgr::TypeId  t = getTypeDecor(ctx);
  
  std::string temp = "%"+codeCounters.newTEMP();

  if (Types.isIntegerTy(t1) and Types.isIntegerTy(t2)) {
    // Aritmetica de enteros
    if (ctx->MUL()) {
      code = code || instruction::MUL(temp, addr1, addr2);
    } else if (ctx->DIV()) {
      code = code || instruction::DIV(temp, addr1, addr2);
    } else if (ctx->MOD()) {
      std::string temp2 = "%"+codeCounters.newTEMP();
      std::string temp3 = "%"+codeCounters.newTEMP();
      
      code = code || instruction::DIV(temp2, addr1, addr2) || instruction::MUL(temp3, addr2, temp2) || instruction::SUB(temp, addr1, temp3);
    } else if (ctx->PLUS()) {
      code = code || instruction::ADD(temp, addr1, addr2);
    } else if (ctx->MINUS()) {
      code = code || instruction::SUB(temp, addr1, addr2);
    }
  } else {
    // Conversion a floats
    std::string tempAddr1 = addr1;
    std::string tempAddr2 = addr2;
    if (Types.isIntegerTy(t1)) {
      tempAddr1 = "%"+codeCounters.newTEMP();
      code = code || instruction::FLOAT(tempAddr1, addr1);
    }
    if (Types.isIntegerTy(t2)) {
      tempAddr2 = "%"+codeCounters.newTEMP();
      code = code || instruction::FLOAT(tempAddr2, addr2);
    }
    
    // Aritmetica de floats
    if (ctx->MUL()) {
      code = code || instruction::FMUL(temp, tempAddr1, tempAddr2);
    } else if (ctx->DIV()) {
      code = code || instruction::FDIV(temp, tempAddr1, tempAddr2);
    } else if (ctx->MOD()) {
      std::string temp2 = "%"+codeCounters.newTEMP();
      std::string temp3 = "%"+codeCounters.newTEMP();
      
      code = code || instruction::FDIV(temp2, tempAddr1, tempAddr2) || instruction::FMUL(temp3, tempAddr2, temp2) || instruction::FSUB(temp, tempAddr1, temp3);
    } else if (ctx->PLUS()) {
      code = code || instruction::FADD(temp, tempAddr1, tempAddr2);
    } else if (ctx->MINUS()) {
      code = code || instruction::FSUB(temp, tempAddr1, tempAddr2);
    }
  }
  
  CodeAttribs codAts(temp, "", code);
  
  DEBUG_EXIT();
  return codAts;
}

antlrcpp::Any CodeGenVisitor::visitRelational(AslParser::RelationalContext *ctx) {
  DEBUG_ENTER();
  
  // Left expression
  CodeAttribs     && codAt1 = visit(ctx->expr(0));
  std::string         addr1 = codAt1.addr;
  instructionList &   code1 = codAt1.code;
  
  // Right expression
  CodeAttribs     && codAt2 = visit(ctx->expr(1));
  std::string         addr2 = codAt2.addr;
  instructionList &   code2 = codAt2.code;
  
  instructionList &&   code = code1 || code2;
  
  TypesMgr::TypeId t1 = getTypeDecor(ctx->expr(0));
  TypesMgr::TypeId t2 = getTypeDecor(ctx->expr(1));
  
  std::string temp = "%"+codeCounters.newTEMP();
  
  if (Types.isIntegerTy(t1) and Types.isIntegerTy(t2)) {
    // Comparacion de enteros
    if (ctx->EQUAL()) {
      code = code || instruction::EQ(temp, addr1, addr2);
    } else if (ctx->DIFF()) {
      code = code || instruction::EQ(temp, addr1, addr2) || instruction::NOT(temp, temp);
    } else if (ctx->GT()) {
      code = code || instruction::LT(temp, addr2, addr1);
    } else if (ctx->LT()) {
      code = code || instruction::LT(temp, addr1, addr2);
    } else if (ctx->GTE()) {
      code = code || instruction::LE(temp, addr2, addr1);
    } else if (ctx->LTE()) {
      code = code || instruction::LE(temp, addr1, addr2);
    }
  } else {
    // Conversion a floats
    if (Types.isFloatTy(t1)) {
      code = code || instruction::FLOAT(addr1, addr1);
    } else {
      code = code || instruction::FLOAT(addr2, addr2);
    }
    
    // Comparacion de floats
    if (ctx->EQUAL()) {
      code = code || instruction::FEQ(temp, addr1, addr2);
    } else if (ctx->DIFF()) {
      code = code || instruction::FEQ(temp, addr1, addr2) || instruction::NOT(temp, temp);
    } else if (ctx->GT()) {
      code = code || instruction::FLT(temp, addr2, addr1);
    } else if (ctx->LT()) {
      code = code || instruction::FLT(temp, addr1, addr2);
    } else if (ctx->GTE()) {
      code = code || instruction::FLE(temp, addr2, addr1);
    } else if (ctx->LTE()) {
      code = code || instruction::FLE(temp, addr1, addr2);
    }
  }
  
  CodeAttribs codAts(temp, "", code);
  
  DEBUG_EXIT();
  return codAts;
}

antlrcpp::Any CodeGenVisitor::visitLogical(AslParser::LogicalContext *ctx) {
  DEBUG_ENTER();
  
  // Left expression
  CodeAttribs     && codAt1 = visit(ctx->expr(0));
  std::string         addr1 = codAt1.addr;
  instructionList &   code1 = codAt1.code;
  
  // Right expression
  CodeAttribs     && codAt2 = visit(ctx->expr(1));
  std::string         addr2 = codAt2.addr;
  instructionList &   code2 = codAt2.code;
  
  instructionList &&   code = code1 || code2;
  
  std::string temp = "%"+codeCounters.newTEMP();
  
  if (ctx->AND()) {
    code = code || instruction::AND(temp, addr1, addr2);
  } else if (ctx->OR()) {
    code = code || instruction::OR(temp, addr1, addr2);
  }
  
  CodeAttribs codAts(temp, "", code);
  
  DEBUG_EXIT();
  return codAts;
}

antlrcpp::Any CodeGenVisitor::visitArrayAccess(AslParser::ArrayAccessContext *ctx) {
  DEBUG_ENTER();
  CodeAttribs && codAts = visit(ctx->array());
  DEBUG_EXIT();
  return codAts;
}

antlrcpp::Any CodeGenVisitor::visitValue(AslParser::ValueContext *ctx) {
  DEBUG_ENTER();
  
  instructionList code;
  std::string temp = "%"+codeCounters.newTEMP();
    
  if (ctx->INTVAL()) {
    code = instruction::ILOAD(temp, ctx->getText());
  } else if (ctx->FLOATVAL()) {
    code = instruction::FLOAD(temp, ctx->getText());
  } else if (ctx->CHARVAL()) {
    code = instruction::CHLOAD(temp, ctx->getText());
  } else if (ctx->BOOLVAL()) {
    std::string value = "1";
    if (ctx->getText() == "false") {
      value = "0";
    }
    code = instruction::ILOAD(temp, value);
  } else {
    code = instruction::LOAD(temp, ctx->getText());
  }
    
  CodeAttribs codAts(temp, "", code);
  
  DEBUG_EXIT();
  return codAts;
}

antlrcpp::Any CodeGenVisitor::visitExprIdent(AslParser::ExprIdentContext *ctx) {
  DEBUG_ENTER();
  CodeAttribs && codAts = visit(ctx->ident());
  DEBUG_EXIT();
  return codAts;
}

// Ident rules --------------------------------------------------------------------------------------------------------------
antlrcpp::Any CodeGenVisitor::visitIdent(AslParser::IdentContext *ctx) {
  DEBUG_ENTER();
  CodeAttribs codAts(ctx->ID()->getText(), "", instructionList());
  DEBUG_EXIT();
  return codAts;
}

// Array rules --------------------------------------------------------------------------------------------------------------
antlrcpp::Any CodeGenVisitor::visitArray(AslParser::ArrayContext *ctx) {
  DEBUG_ENTER();
    
  DEBUG_EXIT();
  return 0;
}

// Function_call rules ------------------------------------------------------------------------------------------------------
antlrcpp::Any CodeGenVisitor::visitFunction_call(AslParser::Function_callContext *ctx) {
  DEBUG_ENTER();
  
  std::string funcName = ctx->ident()->getText();
  TypesMgr::TypeId tFunc = getTypeDecor(ctx->ident()); 
  
  instructionList code = instructionList();
  
  code = code || instruction::PUSH(); // make space for function result
  
  instructionList pushCode = instructionList();
  instructionList popCode = instructionList();

  int numOfExpr = ctx->expr().size();
  for (int i=0; i<numOfExpr; ++i) {
    CodeAttribs     && codAt1 = visit(ctx->expr(i));
    std::string         addr1 = codAt1.addr;
    instructionList &   code1 = codAt1.code;
    
    code = code || code1;
    
    TypesMgr::TypeId tExpr = getTypeDecor(ctx->expr(i));
    TypesMgr::TypeId tParam = Types.getParameterType(tFunc, i);
    
    if (Types.isIntegerTy(tExpr) and Types.isFloatTy(tParam)) {
      std::string tempAddr1 = "%" + codeCounters.newTEMP();
      code = code || instruction::FLOAT(tempAddr1, addr1);
    } else if (Types.isArrayTy(tParam)) {
      std::string tempAddr1 = "%" + codeCounters.newTEMP();
      code = code || instruction::ALOAD(tempAddr1, addr1);
    }

    pushCode = pushCode || instruction::PUSH(addr1);
    popCode = popCode || instruction::POP();
  }
  
  code = code || pushCode || instruction::CALL(funcName) || popCode;
  
  std::string temp = "%"+codeCounters.newTEMP();
  
  code = code || instruction::POP(temp); // pop result and store it in temp
  
  CodeAttribs codAts(temp, "", code);
  
  DEBUG_EXIT();
  return codAts;
}


// Getters for the necessary tree node atributes:
//   Scope and Type
SymTable::ScopeId CodeGenVisitor::getScopeDecor(antlr4::ParserRuleContext *ctx) const {
  return Decorations.getScope(ctx);
}
TypesMgr::TypeId CodeGenVisitor::getTypeDecor(antlr4::ParserRuleContext *ctx) const {
  return Decorations.getType(ctx);
}


// Constructors of the class CodeAttribs:
//
CodeGenVisitor::CodeAttribs::CodeAttribs(const std::string & addr,
                                         const std::string & offs,
                                         instructionList & code) :
  addr{addr}, offs{offs}, code{code} {
}

CodeGenVisitor::CodeAttribs::CodeAttribs(const std::string & addr,
                                         const std::string & offs,
                                         instructionList && code) :
  addr{addr}, offs{offs}, code{code} {
}
