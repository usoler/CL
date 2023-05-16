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
    
    TypesMgr::TypeId tParam = getTypeDecor(ctxParam);
    bool isArray = Types.isArrayTy(tParam);
    
    subr.add_param(param.name, param.type, isArray);
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
  
  if (Types.isArrayTy(t1)) {
      t1 = Types.getArrayElemType(t1);
    }
  
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
    if (Types.isArrayTy(t1)) {
      t1 = Types.getArrayElemType(t1);
    }
    
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
  
  // Left expr
  CodeAttribs     && codAtsE1 = visit(ctx->left_expr());
  std::string           addr1 = codAtsE1.addr;
  std::string           offs1  = codAtsE1.offs;
  instructionList &     code1 = codAtsE1.code;
  TypesMgr::TypeId tLeft = getTypeDecor(ctx->left_expr());
  
  // Right expr
  CodeAttribs     && codAtsE2 = visit(ctx->expr());
  std::string           addr2 = codAtsE2.addr;
  std::string           offs2  = codAtsE2.offs;
  instructionList &     code2 = codAtsE2.code;
  TypesMgr::TypeId tRight = getTypeDecor(ctx->expr());

  code = code1 || code2;
  
  if (Types.isFunctionTy(tRight)) {
    tRight = Types.getFuncReturnType(tRight);
  }
  
  if (Types.isFloatTy(tLeft) and Types.isIntegerTy(tRight)) {
    std::string temp = "%"+codeCounters.newTEMP();
    code = code || instruction::FLOAT(temp, addr2);
    addr2 = temp;
  }
  
  if (ctx->left_expr()->expr()) {
    tLeft = getTypeDecor(ctx->left_expr()->ident());
  }
  
  bool exprIsArrayTy = (offs2 != "");

  if (Types.isArrayTy(tLeft) and not exprIsArrayTy) {
    if (offs1 != "") {
      code = code || instruction::XLOAD(addr1, offs1, addr2);
    } else {
      // Array assignment
      if (Symbols.isParameterClass(addr1)) { // por referencia
        std::string temp = "%"+codeCounters.newTEMP();
        code = code || instruction::LOAD(temp, addr1);
        addr1 = temp;
      }
      
      if (Symbols.isParameterClass(addr2)) { // por referencia
        std::string temp = "%"+codeCounters.newTEMP();
        code = code || instruction::LOAD(temp, addr2);
        addr2 = temp;
      }
            
      int arraySize = Types.getArraySize(tLeft);
      std::string tempOffs = "%"+codeCounters.newTEMP();
      std::string tempRight = "%"+codeCounters.newTEMP();
      for (int i=0; i<arraySize; ++i) {
        std::string index = std::to_string(i);
        code = code || instruction::ILOAD(tempOffs, index) || instruction::LOADX(tempRight, addr2, tempOffs) || instruction::XLOAD(addr1, tempOffs, tempRight);
      }
    }
  } else if (not Types.isArrayTy(tLeft) and exprIsArrayTy) {
    if (Symbols.isParameterClass(addr2)) { // por referencia
        std::string temp = "%"+codeCounters.newTEMP();
        code = code || instruction::LOAD(temp, addr2);
        addr2 = temp;
    }
    
    code = code || instruction::LOADX(addr1, addr2, offs2);
  } else if (Types.isArrayTy(tLeft) and exprIsArrayTy) {
    if (Symbols.isParameterClass(addr1)) { // por referencia
      std::string temp = "%"+codeCounters.newTEMP();
      code = code || instruction::LOAD(temp, addr1);
      addr1 = temp;
    }
    
    if (Symbols.isParameterClass(addr2)) { // por referencia
      std::string temp = "%"+codeCounters.newTEMP();
      code = code || instruction::LOAD(temp, addr2);
      addr2 = temp;
    }
      
    std::string tempRight = "%"+codeCounters.newTEMP();
    code = code || instruction::LOADX(tempRight, addr2, offs2) || instruction::XLOAD(addr1, offs1, tempRight);
  } else if (Types.isFloatTy(tRight)) {
    code = code || instruction::FLOAD(addr1,addr2);
  } else if (Types.isCharacterTy(tRight)) {
    code = code || instruction::CHLOAD(addr1,addr2);
  } else {
    code = code || instruction::ILOAD(addr1,addr2);
  }
  
  DEBUG_EXIT();
  return code;
}

antlrcpp::Any CodeGenVisitor::visitIfStmt(AslParser::IfStmtContext *ctx) {
  DEBUG_ENTER();
  
  instructionList code;
  
  CodeAttribs     && codAtsE = visit(ctx->expr());
  std::string          addr1 = codAtsE.addr;
  std::string          offs1 = codAtsE.offs;
  instructionList &    conditionCode = codAtsE.code;
  
  code = code || conditionCode;
  
  if (offs1 != "") {
    if (Symbols.isParameterClass(addr1)) { // por referencia
      std::string temp = "%"+codeCounters.newTEMP();
      code = code || instruction::LOAD(temp, addr1);
      addr1 = temp;
    }
    
    code = code || instruction::LOADX(addr1, addr1, offs1);
  }
  
  instructionList &&   thenCode = visit(ctx->statements(0));
  
  std::string label = codeCounters.newLabelIF();
  std::string labelElse = "else" + label;
  std::string labelEndIf = "endif" + label;
  
  if (ctx->ELSE()) {
    instructionList && elseCode = visit(ctx->statements(1));
    
    code = code || instruction::FJUMP(addr1, labelElse) 
    || thenCode || instruction::UJUMP(labelEndIf) 
    || instruction::LABEL(labelElse) || elseCode
    || instruction::LABEL(labelEndIf);
  } else {
    code = code || instruction::FJUMP(addr1, labelEndIf) || thenCode || instruction::LABEL(labelEndIf);
  }
         
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
  
  CodeAttribs &&      codAts = visit(ctx->function_call());
  std::string          addr = codAts.addr;
  instructionList &    code = codAts.code;
    
  DEBUG_EXIT();
  return code;
}

antlrcpp::Any CodeGenVisitor::visitReadStmt(AslParser::ReadStmtContext *ctx) {
  DEBUG_ENTER();
  
  CodeAttribs     && codAtsE = visit(ctx->left_expr());
  std::string          addr1 = codAtsE.addr;
  std::string          offs1 = codAtsE.offs;
  instructionList &    code1 = codAtsE.code;
  
  instructionList &     code = code1;
  
  TypesMgr::TypeId tid = getTypeDecor(ctx->left_expr());

  std::string temp = addr1;
  if (offs1 != "") {
    temp = "%"+codeCounters.newTEMP();
  }
  
  if (Types.isFloatTy(tid)) {
    code = code || instruction::READF(temp);
  } else if (Types.isCharacterTy(tid)) {
    code = code || instruction::READC(temp);
  } else {
    code = code || instruction::READI(temp);
  }
  
  if (offs1 != "") {
    code = code || instruction::XLOAD(addr1, offs1, temp);
  }
  
  DEBUG_EXIT();
  return code;
}

antlrcpp::Any CodeGenVisitor::visitWriteExpr(AslParser::WriteExprContext *ctx) {
  DEBUG_ENTER();
  
  CodeAttribs     && codAt1 = visit(ctx->expr());
  std::string         addr1 = codAt1.addr;
  std::string         offs1 = codAt1.offs;
  instructionList &   code1 = codAt1.code;
  
  instructionList &   code = code1;
  
  TypesMgr::TypeId tid1 = getTypeDecor(ctx->expr());

  std::string temp = addr1;
  if (offs1 != "") {
    temp = "%"+codeCounters.newTEMP();
  
    if (Symbols.isLocalVarClass(addr1)) {
      code = code || instruction::LOADX(temp, addr1, offs1);
    } else {
      std::string temp2 = "%"+codeCounters.newTEMP();
      code = code || instruction::LOAD(temp2, addr1) || instruction::LOADX(temp, temp2, offs1);
    }    
  }
    
  if (Types.isFloatTy(tid1)) {
    code = code || instruction::WRITEF(temp);
  } else if (Types.isCharacterTy(tid1)) {
    code = code || instruction::WRITEC(temp);
  } else {
    code = code || instruction::WRITEI(temp); 
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
  } else {
    code = instruction::RETURN();
  }
           
  DEBUG_EXIT();
  return code;
}

// Left expr rules ----------------------------------------------------------------------------------------------------------
antlrcpp::Any CodeGenVisitor::visitLeft_expr(AslParser::Left_exprContext *ctx) {
  DEBUG_ENTER();
    
  std::string name = ctx->ident()->getText();
  
  CodeAttribs     && codAt1 = visit(ctx->ident());
  std::string         addr1 = codAt1.addr;
  instructionList &   code1 = codAt1.code;

  instructionList &   code = code1;
  
  std::string offs = "";
  if (ctx->expr()) {
    CodeAttribs     && codAt2 = visit(ctx->expr());
    offs = codAt2.addr;
    instructionList &   code2 = codAt2.code;
        
    code = code || code2;
    
    if (Symbols.isParameterClass(addr1)) { // por referencia
      std::string temp = "%"+codeCounters.newTEMP();
      code = code || instruction::LOAD(temp, addr1);
      addr1 = temp;
    }
  }
    
  CodeAttribs codAts(addr1, offs, code);
  
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
  std::string          offs = codAts1.offs;
  instructionList &    code = codAts1.code;
  
  if (offs != "") {
    std::string tempExpr1 = "%"+codeCounters.newTEMP();
  
    if (Symbols.isLocalVarClass(addr)) {
      code = code || instruction::LOADX(tempExpr1, addr, offs);
    } else {
      std::string tempTempExpr1 = "%"+codeCounters.newTEMP();
      code = code || instruction::LOAD(tempTempExpr1, addr) || instruction::LOADX(tempExpr1, tempTempExpr1, offs);
    }
    
    addr = tempExpr1;
  }
  
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
  std::string         offs1 = codAt1.offs;
  instructionList &   code1 = codAt1.code;
  
  // Right expression
  CodeAttribs     && codAt2 = visit(ctx->expr(1));
  std::string         addr2 = codAt2.addr;
  std::string         offs2 = codAt2.offs;
  instructionList &   code2 = codAt2.code;
  
  instructionList &&   code = code1 || code2;
  
  if (offs1 != "") {
    std::string tempExpr1 = "%"+codeCounters.newTEMP();
  
    if (Symbols.isLocalVarClass(addr1)) {
      code = code || instruction::LOADX(tempExpr1, addr1, offs1);
    } else {
      std::string tempTempExpr1 = "%"+codeCounters.newTEMP();
      code = code || instruction::LOAD(tempTempExpr1, addr1) || instruction::LOADX(tempExpr1, tempTempExpr1, offs1);
    }
    
    addr1 = tempExpr1;
  }
  
  if (offs2 != "") {
    std::string tempExpr2 = "%"+codeCounters.newTEMP();
  
    if (Symbols.isLocalVarClass(addr2)) {
      code = code || instruction::LOADX(tempExpr2, addr2, offs2);
    } else {
      std::string tempTempExpr2 = "%"+codeCounters.newTEMP();
      code = code || instruction::LOAD(tempTempExpr2, addr2) || instruction::LOADX(tempExpr2, tempTempExpr2, offs2);
    }
    
    addr2 = tempExpr2;
  }
  
  TypesMgr::TypeId t1 = getTypeDecor(ctx->expr(0));
  TypesMgr::TypeId t2 = getTypeDecor(ctx->expr(1));
  
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
  std::string         offs1 = codAt1.offs;
  instructionList &   code1 = codAt1.code;
  
  // Right expression
  CodeAttribs     && codAt2 = visit(ctx->expr(1));
  std::string         addr2 = codAt2.addr;
  std::string         offs2 = codAt2.offs;
  instructionList &   code2 = codAt2.code;
  
  instructionList &&   code = code1 || code2;
  
  if (offs1 != "") {
    std::string tempExpr1 = "%"+codeCounters.newTEMP();
  
    if (Symbols.isLocalVarClass(addr1)) {
      code = code || instruction::LOADX(tempExpr1, addr1, offs1);
    } else {
      std::string tempTempExpr1 = "%"+codeCounters.newTEMP();
      code = code || instruction::LOAD(tempTempExpr1, addr1) || instruction::LOADX(tempExpr1, tempTempExpr1, offs1);
    }
    
    addr1 = tempExpr1;
  }
  
  if (offs2 != "") {
    std::string tempExpr2 = "%"+codeCounters.newTEMP();
  
    if (Symbols.isLocalVarClass(addr2)) {
      code = code || instruction::LOADX(tempExpr2, addr2, offs2);
    } else {
      std::string tempTempExpr2 = "%"+codeCounters.newTEMP();
      code = code || instruction::LOAD(tempTempExpr2, addr2) || instruction::LOADX(tempExpr2, tempTempExpr2, offs2);
    }
    
    addr2 = tempExpr2;
  }
  
  TypesMgr::TypeId t1 = getTypeDecor(ctx->expr(0));
  TypesMgr::TypeId t2 = getTypeDecor(ctx->expr(1));
  
  std::string temp = "%"+codeCounters.newTEMP();
  
  if (not Types.isFloatTy(t1) and not Types.isFloatTy(t2)) {
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
    std::string tempAddr1 = addr1;
    std::string tempAddr2 = addr2;

    if (not Types.isFloatTy(t1)) {
      tempAddr1 = "%" + codeCounters.newTEMP();
      code = code || instruction::FLOAT(tempAddr1, addr1);
    }
    
    if (not Types.isFloatTy(t2)) {
      tempAddr2 = "%" + codeCounters.newTEMP();
      code = code || instruction::FLOAT(tempAddr2, addr2);
    } 

    // Comparacion de floats
    if (ctx->EQUAL()) {
      code = code || instruction::FEQ(temp, tempAddr1, tempAddr2);
    } else if (ctx->DIFF()) {
      code = code || instruction::FEQ(temp, tempAddr1, tempAddr2) || instruction::NOT(temp, temp);
    } else if (ctx->GT()) {
      code = code || instruction::FLT(temp, tempAddr2, tempAddr1);
    } else if (ctx->LT()) {
      code = code || instruction::FLT(temp, tempAddr1, tempAddr2);
    } else if (ctx->GTE()) {
      code = code || instruction::FLE(temp, tempAddr2, tempAddr1);
    } else if (ctx->LTE()) {
      code = code || instruction::FLE(temp, tempAddr1, tempAddr2);
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
  std::string         offs1 = codAt1.offs;
  instructionList &   code1 = codAt1.code;
  
  // Right expression
  CodeAttribs     && codAt2 = visit(ctx->expr(1));
  std::string         addr2 = codAt2.addr;
  std::string         offs2 = codAt2.offs;
  instructionList &   code2 = codAt2.code;
  
  instructionList &&   code = code1 || code2;
  
  if (offs1 != "") {
    std::string tempExpr1 = "%"+codeCounters.newTEMP();
  
    if (Symbols.isLocalVarClass(addr1)) {
      code = code || instruction::LOADX(tempExpr1, addr1, offs1);
    } else {
      std::string tempTempExpr1 = "%"+codeCounters.newTEMP();
      code = code || instruction::LOAD(tempTempExpr1, addr1) || instruction::LOADX(tempExpr1, tempTempExpr1, offs1);
    }
    
    addr1 = tempExpr1;
  }
  
  if (offs2 != "") {
    std::string tempExpr2 = "%"+codeCounters.newTEMP();
  
    if (Symbols.isLocalVarClass(addr2)) {
      code = code || instruction::LOADX(tempExpr2, addr2, offs2);
    } else {
      std::string tempTempExpr2 = "%"+codeCounters.newTEMP();
      code = code || instruction::LOAD(tempTempExpr2, addr2) || instruction::LOADX(tempExpr2, tempTempExpr2, offs2);
    }
    
    addr2 = tempExpr2;
  }
    
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
  
  std::string name = ctx->ident()->getText();
    
  CodeAttribs     && codAt1 = visit(ctx->ident());
  std::string         addr1 = codAt1.addr;
  instructionList &   code1 = codAt1.code;
  
  CodeAttribs     && codAt2 = visit(ctx->expr());
  std::string         addr2 = codAt2.addr;
  instructionList &   code2 = codAt2.code;
  
  instructionList && code = code1 || code2;
  
  CodeAttribs codAts(name, addr2, code);
  
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
    std::string value = ctx->getText();
    value = value.substr(1, value.size()-2);
    code = instruction::CHLOAD(temp, value);
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

antlrcpp::Any CodeGenVisitor::visitFactorial(AslParser::FactorialContext *ctx) {
  DEBUG_ENTER();
  
  CodeAttribs     && codAts1 = visit(ctx->expr());
  std::string          addr = codAts1.addr;
  std::string          offs = codAts1.offs;
  instructionList &    code = codAts1.code;
    
  if (offs != "") {
    std::string tempExpr1 = "%"+codeCounters.newTEMP();
  
    if (Symbols.isLocalVarClass(addr)) {
      code = code || instruction::LOADX(tempExpr1, addr, offs);
    } else {
      std::string tempTempExpr1 = "%"+codeCounters.newTEMP();
      code = code || instruction::LOAD(tempTempExpr1, addr) || instruction::LOADX(tempExpr1, tempTempExpr1, offs);
    }
    
    addr = tempExpr1;
  }
  
  TypesMgr::TypeId t1 = getTypeDecor(ctx->expr());
  
  std::string index = "%" + codeCounters.newTEMP();
  std::string zero = "%" + codeCounters.newTEMP();
  std::string one = "%" + codeCounters.newTEMP();
  std::string tempCondition = "%" + codeCounters.newTEMP();
  std::string tempAddr = "%" + codeCounters.newTEMP();
  std::string tempMult = "%" + codeCounters.newTEMP();
  
  std::string label = codeCounters.newLabelWHILE();
  std::string labelFactorial = "factorial" + label;
  std::string labelHalt = "halt" + label;
  std::string labelZeroCase = "zeroCase" + label;
  std::string labelEndFactorial = "endfactorial" + label;
  
  if (Symbols.isLocalVarClass(addr)) {
    code = code || instruction::LOAD(tempMult, addr) || instruction::LOAD(tempAddr, addr);
  } else {
    code = code || instruction::ILOAD(tempMult, addr) || instruction::ILOAD(tempAddr, addr);
  }
  
  code = code 
          // Prepare constants
          || instruction::ILOAD(zero, "0")
          || instruction::ILOAD(index, "2")
          || instruction::ILOAD(one, "1")
          // Check Zero Case
          || instruction::EQ(tempCondition, zero, tempAddr)
          || instruction::NOT(tempCondition, tempCondition)
          || instruction::FJUMP(tempCondition, labelZeroCase)
          // Check Negative Operand
          || instruction::LE(tempCondition, zero, tempAddr)
          || instruction::FJUMP(tempCondition, labelHalt)
          // Loop
          || instruction::LABEL(labelFactorial)
          || instruction::LT(tempCondition, one, tempAddr)
          || instruction::FJUMP(tempCondition, labelEndFactorial)
          || instruction::SUB(tempAddr, tempAddr, one)
          || instruction::MUL(tempMult, tempMult, tempAddr)
          || instruction::UJUMP(labelFactorial)
          // Label Negative Operand
          || instruction::LABEL(labelHalt)
          || instruction::HALT(code::INVALID_INTEGER_OPERAND)
          // Label Zero Case
          || instruction::LABEL(labelZeroCase)
          || instruction::ILOAD(tempMult, "1")
          || instruction::LABEL(labelEndFactorial);
  
  CodeAttribs codAts(tempMult, "", code);
  
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

// Function_call rules ------------------------------------------------------------------------------------------------------
antlrcpp::Any CodeGenVisitor::visitFunction_call(AslParser::Function_callContext *ctx) {
  DEBUG_ENTER();
  
  std::string funcName = ctx->ident()->getText();
  TypesMgr::TypeId tFunc = getTypeDecor(ctx->ident()); 
  
  instructionList code = instructionList();
  
  if (not Types.isVoidFunction(tFunc)) {
    code = code || instruction::PUSH(); // make space for function result
  }
  
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
    
    std::string tempAddr1 = addr1;
    if (Types.isIntegerTy(tExpr) and Types.isFloatTy(tParam)) {
      tempAddr1 = "%" + codeCounters.newTEMP();
      code = code || instruction::FLOAT(tempAddr1, addr1);
    } else if (Types.isArrayTy(tParam) and Symbols.isLocalVarClass(addr1)) {
      tempAddr1 = "%" + codeCounters.newTEMP();
      code = code || instruction::ALOAD(tempAddr1, addr1);
    }

    pushCode = pushCode || instruction::PUSH(tempAddr1);
    popCode = popCode || instruction::POP();
  }
  
  code = code || pushCode || instruction::CALL(funcName) || popCode;
  
  std::string temp = "";
  if (not Types.isVoidFunction(tFunc)) {
    temp = "%"+codeCounters.newTEMP();
    code = code || instruction::POP(temp); // pop result and store it in temp
  }
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
