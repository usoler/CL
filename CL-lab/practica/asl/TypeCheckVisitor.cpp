//////////////////////////////////////////////////////////////////////
//
//    TypeCheckVisitor - Walk the parser tree to do the semantic
//                       typecheck for the Asl programming language
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

#include "TypeCheckVisitor.h"
#include "antlr4-runtime.h"

#include "../common/TypesMgr.h"
#include "../common/SymTable.h"
#include "../common/TreeDecoration.h"
#include "../common/SemErrors.h"

#include <iostream>
#include <string>

// uncomment the following line to enable debugging messages with DEBUG*
// #define DEBUG_BUILD
#include "../common/debug.h"

// using namespace std;


// Constructor
TypeCheckVisitor::TypeCheckVisitor(TypesMgr       & Types,
                                   SymTable       & Symbols,
                                   TreeDecoration & Decorations,
                                   SemErrors      & Errors) :
  Types{Types},
  Symbols{Symbols},
  Decorations{Decorations},
  Errors{Errors} {
}

// Accessor/Mutator to the attribute currFunctionType
TypesMgr::TypeId TypeCheckVisitor::getCurrentFunctionTy() const {
  return currFunctionType;
}

void TypeCheckVisitor::setCurrentFunctionTy(TypesMgr::TypeId type) {
  currFunctionType = type;
}

// Methods to visit each kind of node:
//
antlrcpp::Any TypeCheckVisitor::visitProgram(AslParser::ProgramContext *ctx) {
  DEBUG_ENTER();
  SymTable::ScopeId sc = getScopeDecor(ctx);
  Symbols.pushThisScope(sc);
  for (auto ctxFunc : ctx->function()) { 
    visit(ctxFunc);
  }
  if (Symbols.noMainProperlyDeclared())
    Errors.noMainProperlyDeclared(ctx);
  Symbols.popScope();
  Errors.print();
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitFunction(AslParser::FunctionContext *ctx) {
  DEBUG_ENTER();
  
  SymTable::ScopeId sc = getScopeDecor(ctx);
  Symbols.pushThisScope(sc);
  // Symbols.print();
  
  // Coge el tipo de la funcion
  TypesMgr::TypeId t1 = getTypeDecor(ctx);
  
  // Setea el tipo de la funcion
  setCurrentFunctionTy(t1);
  
  // Visita los statements de la funcion
  visit(ctx->statements());
  
  Symbols.popScope();
  
  DEBUG_EXIT();
  return 0;
}

// antlrcpp::Any TypeCheckVisitor::visitDeclarations(AslParser::DeclarationsContext *ctx) {
//   DEBUG_ENTER();
//   antlrcpp::Any r = visitChildren(ctx);
//   DEBUG_EXIT();
//   return r;
// }

// antlrcpp::Any TypeCheckVisitor::visitVariable_decl(AslParser::Variable_declContext *ctx) {
//   DEBUG_ENTER();
//   antlrcpp::Any r = visitChildren(ctx);
//   DEBUG_EXIT();
//   return r;
// }

// antlrcpp::Any TypeCheckVisitor::visitType(AslParser::TypeContext *ctx) {
//   DEBUG_ENTER();
//   antlrcpp::Any r = visitChildren(ctx);
//   DEBUG_EXIT();
//   return r;
// }

antlrcpp::Any TypeCheckVisitor::visitStatements(AslParser::StatementsContext *ctx) {
  DEBUG_ENTER();
  visitChildren(ctx);
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitAssignStmt(AslParser::AssignStmtContext *ctx) {
  DEBUG_ENTER();
  visit(ctx->left_expr());
  visit(ctx->expr());
  TypesMgr::TypeId t1 = getTypeDecor(ctx->left_expr());
  TypesMgr::TypeId t2 = getTypeDecor(ctx->expr());
  if ((not Types.isErrorTy(t1)) and (not Types.isErrorTy(t2)) and
      (not Types.copyableTypes(t1, t2)))
    Errors.incompatibleAssignment(ctx->ASSIGN());
  if ((not Types.isErrorTy(t1)) and (not getIsLValueDecor(ctx->left_expr())))
    Errors.nonReferenceableLeftExpr(ctx->left_expr());
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitIfStmt(AslParser::IfStmtContext *ctx) {
  DEBUG_ENTER();
  
  // Visita la expresion
  visit(ctx->expr());
  // Coge el tipo de la expresion
  TypesMgr::TypeId t1 = getTypeDecor(ctx->expr());
  
  // Comprueba si el tipo de la expresion no es tipo error ni tipo boolean, entonces saca error
  if ((not Types.isErrorTy(t1)) and (not Types.isBooleanTy(t1)))
    Errors.booleanRequired(ctx);
  
  // Visita los statements del THEN
  visit(ctx->statements(0));
  
  // Comprueba si tiene statements del ELSE, entonces visita sus statements
  if (ctx->statements().size() > 1) {
    visit(ctx->statements(1));
  }
  
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitWhileStmt(AslParser::WhileStmtContext *ctx) {
  DEBUG_ENTER();
  
  // Visita la expresion
  visit(ctx->expr());
  // Coge el tipo de la expresion
  TypesMgr::TypeId t1 = getTypeDecor(ctx->expr());
  
  // Comprueba si el tipo de la expresion no es tipo error ni tipo boolean, entonces saca error
  if ((not Types.isErrorTy(t1)) and (not Types.isBooleanTy(t1)))
    Errors.booleanRequired(ctx);
  
  // Visita los statements del DO
  visit(ctx->statements());
  
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitReturnStmt(AslParser::ReturnStmtContext *ctx) {
    DEBUG_ENTER();
    
    // Setea por defecto el tipo 'void'
    TypesMgr::TypeId t1 = Types.createVoidTy();
    
    // Comprueba si contiene alguna expresion
    if (ctx->expr()) {
      // Coge el tipo de la expresion
      visit(ctx->expr());
      t1 = getTypeDecor(ctx->expr());
    }

    // Coge el tipo de la funcion (tipos vectores, tipo return)
    TypesMgr::TypeId t2 = getCurrentFunctionTy();
    // Comprueba que la funcion no tenga tipo 'error'
    if (not Types.isErrorTy(t2)) {
      // Coge el tipo de return
      TypesMgr::TypeId ret = Types.getFuncReturnType(t2);

      // Comprueba si son del mismo tipo, si no entonces lanza error
      if (not Types.copyableTypes(t1,ret)) {
          Errors.incompatibleReturn(ctx->RETURN());
      }
    }
    
    DEBUG_EXIT();
    return 0;
}

antlrcpp::Any TypeCheckVisitor::visitProcCall(AslParser::ProcCallContext *ctx) {
  DEBUG_ENTER();
  visitChildren(ctx);
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitReadStmt(AslParser::ReadStmtContext *ctx) {
  DEBUG_ENTER();
  visit(ctx->left_expr());
  TypesMgr::TypeId t1 = getTypeDecor(ctx->left_expr());
  if ((not Types.isErrorTy(t1)) and (not Types.isPrimitiveTy(t1)) and
      (not Types.isFunctionTy(t1)))
    Errors.readWriteRequireBasic(ctx);
  if ((not Types.isErrorTy(t1)) and (not getIsLValueDecor(ctx->left_expr())))
    Errors.nonReferenceableExpression(ctx);
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitWriteExpr(AslParser::WriteExprContext *ctx) {
  DEBUG_ENTER();
  visit(ctx->expr());
  TypesMgr::TypeId t1 = getTypeDecor(ctx->expr());
  if ((not Types.isErrorTy(t1)) and (not Types.isPrimitiveTy(t1)))
    Errors.readWriteRequireBasic(ctx);
  DEBUG_EXIT();
  return 0;
}

// antlrcpp::Any TypeCheckVisitor::visitWriteString(AslParser::WriteStringContext *ctx) {
//   DEBUG_ENTER();
//   antlrcpp::Any r = visitChildren(ctx);
//   DEBUG_EXIT();
//   return r;
// }

antlrcpp::Any TypeCheckVisitor::visitLeftArrayAccess(AslParser::LeftArrayAccessContext *ctx) {
  DEBUG_ENTER();
  
  // Visita el identificador
  visit(ctx->array());
  
  // Coge el tipo del identificador
  TypesMgr::TypeId t1 = getTypeDecor(ctx->array());
  
  // Asigna el mismo tipo del identificador al ArrayAccess
  putTypeDecor(ctx, t1);
  
  // Coge el isLValue del identificador
  bool b = getIsLValueDecor(ctx->array());
  
  // Asigna el mismo isLValue del identificador al ArrayAccess
  putIsLValueDecor(ctx, b);
  
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitArrayAccess(AslParser::ArrayAccessContext *ctx) {
  DEBUG_ENTER();
  
  // Visita el identificador
  visit(ctx->array());
  
  // Asigna el mismo tipo del identificador al ArrayAccess
  TypesMgr::TypeId t1 = getTypeDecor(ctx->array());
  putTypeDecor(ctx, t1);
  
  // Asigna el mismo isLValue del identificador al ArrayAccess
  bool b = getIsLValueDecor(ctx->array());  
  putIsLValueDecor(ctx, b);
  
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitParenthesis(AslParser::ParenthesisContext *ctx) {
  DEBUG_ENTER();
  
  // Coge el valor de la expresion
  visit(ctx->expr());
  TypesMgr::TypeId t1 = getTypeDecor(ctx->expr());
  
  // El tipo del resultado es el mismo que el de la expresion
  putTypeDecor(ctx, t1);
  putIsLValueDecor(ctx, false);
  
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitUnary(AslParser::UnaryContext *ctx) {
  DEBUG_ENTER();
  
  // Coge el valor de la expresion
  visit(ctx->expr());
  TypesMgr::TypeId t1 = getTypeDecor(ctx->expr());
  
  // Comprueba si el tipo de la expresion no es tipo error ni tipo boolean/numerico segun operador, entonces saca error
  if (ctx->NOT()) {
      if (((not Types.isErrorTy(t1)) and (not Types.isBooleanTy(t1))))
        Errors.incompatibleOperator(ctx->op);
  } else {
      if (((not Types.isErrorTy(t1)) and (not Types.isNumericTy(t1))))
        Errors.incompatibleOperator(ctx->op);
  }
    
  // El tipo del resultado es el mismo que el de la expresion
  putTypeDecor(ctx, t1);
  putIsLValueDecor(ctx, false);
  
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitArithmetic(AslParser::ArithmeticContext *ctx) {
  DEBUG_ENTER();
  
  // Coge los valores de las expresiones izquierda y derecha
  visit(ctx->expr(0));
  TypesMgr::TypeId t1 = getTypeDecor(ctx->expr(0));
  visit(ctx->expr(1));
  TypesMgr::TypeId t2 = getTypeDecor(ctx->expr(1));
  
  // Comprueba si los tipos de las expresiones no son tipo error ni tipo numerico, entonces saca error
  if (((not Types.isErrorTy(t1)) and (not Types.isNumericTy(t1))) or
      ((not Types.isErrorTy(t2)) and (not Types.isNumericTy(t2))))
    Errors.incompatibleOperator(ctx->op);
  
  // El tipo del resultado es tipo entero/float
  TypesMgr::TypeId t = getTypeCoercion(t1, t2);
  putTypeDecor(ctx, t);
  putIsLValueDecor(ctx, false);
  
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitRelational(AslParser::RelationalContext *ctx) {
  DEBUG_ENTER();
  
  // Coge los valores de las expresiones izquierda y derecha
  visit(ctx->expr(0));
  TypesMgr::TypeId t1 = getTypeDecor(ctx->expr(0));
  visit(ctx->expr(1));
  TypesMgr::TypeId t2 = getTypeDecor(ctx->expr(1));
  
  // Coge la operacion usada
  std::string oper = ctx->op->getText();
  
  // Comprueba si los tipos de las expresiones no son tipo error y si no son tipos comparables, entonces saca error
  if ((not Types.isErrorTy(t1)) and (not Types.isErrorTy(t2)) and
      (not Types.comparableTypes(t1, t2, oper)))
    Errors.incompatibleOperator(ctx->op);
  
  // El tipo del resultado es tipo boolean
  TypesMgr::TypeId t = Types.createBooleanTy();
  putTypeDecor(ctx, t);
  putIsLValueDecor(ctx, false);
  
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitLogical(AslParser::LogicalContext *ctx) {
  DEBUG_ENTER();
  
  // Coge los valores de las expresiones izquierda y derecha
  visit(ctx->expr(0));
  TypesMgr::TypeId t1 = getTypeDecor(ctx->expr(0));
  visit(ctx->expr(1));
  TypesMgr::TypeId t2 = getTypeDecor(ctx->expr(1));
  
  // Comprueba si los tipos de las expresiones no son tipo error ni tipo boolean, entonces saca error
  if (((not Types.isErrorTy(t1)) and (not Types.isBooleanTy(t1))) or
      ((not Types.isErrorTy(t2)) and (not Types.isBooleanTy(t2))))
    Errors.incompatibleOperator(ctx->op);
  
  // El tipo del resultado es tipo boolean
  TypesMgr::TypeId t = Types.createBooleanTy();
  putTypeDecor(ctx, t);
  putIsLValueDecor(ctx, false);
  
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitValue(AslParser::ValueContext *ctx) {
  DEBUG_ENTER();

  if (ctx->INTVAL()) {
    TypesMgr::TypeId t = Types.createIntegerTy();
    putTypeDecor(ctx, t);
  } else if (ctx->FLOATVAL()) {
    TypesMgr::TypeId t = Types.createFloatTy();
    putTypeDecor(ctx, t);    
  } else if (ctx->BOOLVAL()) {
    TypesMgr::TypeId t = Types.createBooleanTy();
    putTypeDecor(ctx, t);
  } else if (ctx->CHARVAL()) {
    TypesMgr::TypeId t = Types.createCharacterTy();
    putTypeDecor(ctx, t);
  }
  
  putIsLValueDecor(ctx, false);
  
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitLeftExprIdent(AslParser::LeftExprIdentContext *ctx) {
  DEBUG_ENTER();
  
  // Visita el identificador
  visit(ctx->ident());
  
  // Asigna el mismo tipo que el del identificador
  TypesMgr::TypeId t1 = getTypeDecor(ctx->ident());
  putTypeDecor(ctx, t1);
  
  // Asigna el mismo isLValue que el del identificador
  bool b = getIsLValueDecor(ctx->ident());
  putIsLValueDecor(ctx, b);
  
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitExprIdent(AslParser::ExprIdentContext *ctx) {
  DEBUG_ENTER();
  
  // Visita el identificador
  visit(ctx->ident());
  
  // Asigna el mismo tipo que el del identificador
  TypesMgr::TypeId t1 = getTypeDecor(ctx->ident());
  putTypeDecor(ctx, t1);
  
  // Asigna el mismo isLValue que el del identificador
  bool b = getIsLValueDecor(ctx->ident());
  putIsLValueDecor(ctx, b);
  
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitExprFunc(AslParser::ExprFuncContext *ctx) {
  DEBUG_ENTER();
  // Visita el function_call
  visit(ctx->function_call());

  TypesMgr::TypeId tFunc = getTypeDecor(ctx->function_call());
  bool b = getIsLValueDecor(ctx->function_call());;
  
  if (Types.isVoidTy(tFunc)) {
    Errors.isNotFunction(ctx->function_call());
    tFunc = Types.createErrorTy();
    b = false;
  }
  
  putTypeDecor(ctx, tFunc);
  putIsLValueDecor(ctx, b);

  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitIdent(AslParser::IdentContext *ctx) {
  DEBUG_ENTER();
  std::string ident = ctx->getText();
  if (Symbols.findInStack(ident) == -1) {
    Errors.undeclaredIdent(ctx->ID());
    TypesMgr::TypeId te = Types.createErrorTy();
    putTypeDecor(ctx, te);
    putIsLValueDecor(ctx, true);
  }
  else {
    TypesMgr::TypeId t1 = Symbols.getType(ident);
    putTypeDecor(ctx, t1);
    if (Symbols.isFunctionClass(ident))
      putIsLValueDecor(ctx, false);
    else
      putIsLValueDecor(ctx, true);
  }
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitFunction_call(AslParser::Function_callContext *ctx) {
  DEBUG_ENTER();
  
  // Visita el identificador
  visit(ctx->ident());
  
  // Asigna el mismo tipo que el del identificador
  TypesMgr::TypeId tFunc = getTypeDecor(ctx->ident());
  
  // Comprueba si no es un tipo funcion o un tipo error, entonces lanza error
  if (not Types.isFunctionTy(tFunc) and not Types.isErrorTy(tFunc)) {
    Errors.isNotCallable(ctx->ident());
  }
  
  if (Types.isFunctionTy(tFunc) and not Types.isErrorTy(tFunc)) {
    TypesMgr::TypeId tRet = Types.getFuncReturnType(tFunc);
    putTypeDecor(ctx, tRet);
    
    // Asigna el mismo isLValue que el del identificador
    bool b = getIsLValueDecor(ctx->ident());
    putIsLValueDecor(ctx, b);
    
    // Visita cada expr
    int numOfExpr = Types.getNumOfParameters(tFunc);
    for (int i=0; i<numOfExpr; ++i) {
      // Visita la expr
      visit(ctx->expr(i));
    }
  }
  
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitArray(AslParser::ArrayContext *ctx) {
    DEBUG_ENTER();
    
    // Visita el identificador
    visit(ctx->ident());
    
    // Coge el tipo del identificador
    TypesMgr::TypeId t1 = getTypeDecor(ctx->ident());
    
    // Comprueba si no es un tipo error ni un tipo array, entonces lanza error
    if (not Types.isErrorTy(t1) and not Types.isArrayTy(t1)) {
        Errors.nonArrayInArrayAccess(ctx);
    }
    
    // Visita la expresion
    visit(ctx->expr());
    
    // Coge el tipo de la expresion
    TypesMgr::TypeId t2 = getTypeDecor(ctx->expr());
    
    // Comprueba si no es un tipo error ni un tipo entero
    if (not Types.isErrorTy(t2) and not Types.isIntegerTy(t2)) {
        Errors.nonIntegerIndexInArrayAccess(ctx->expr());
    }
    
    // Asigna el tipo y si es isLValue al array
    TypesMgr::TypeId t3 = Types.createErrorTy();
    bool isLValue = false;
    
    if (Types.isArrayTy(t1)) {
        t3 = Types.getArrayElemType(t1);
        isLValue = true;
    }
    
    putTypeDecor(ctx, t3);
    putIsLValueDecor(ctx, isLValue);
    
    
    DEBUG_EXIT();
    return 0;
}

// Getters for the necessary tree node atributes:
//   Scope, Type ans IsLValue
SymTable::ScopeId TypeCheckVisitor::getScopeDecor(antlr4::ParserRuleContext *ctx) {
  return Decorations.getScope(ctx);
}
TypesMgr::TypeId TypeCheckVisitor::getTypeDecor(antlr4::ParserRuleContext *ctx) {
  return Decorations.getType(ctx);
}
bool TypeCheckVisitor::getIsLValueDecor(antlr4::ParserRuleContext *ctx) {
  return Decorations.getIsLValue(ctx);
}

// Setters for the necessary tree node attributes:
//   Scope, Type ans IsLValue
void TypeCheckVisitor::putScopeDecor(antlr4::ParserRuleContext *ctx, SymTable::ScopeId s) {
  Decorations.putScope(ctx, s);
}
void TypeCheckVisitor::putTypeDecor(antlr4::ParserRuleContext *ctx, TypesMgr::TypeId t) {
  Decorations.putType(ctx, t);
}
void TypeCheckVisitor::putIsLValueDecor(antlr4::ParserRuleContext *ctx, bool b) {
  Decorations.putIsLValue(ctx, b);
}

TypesMgr::TypeId TypeCheckVisitor::getTypeCoercion(TypesMgr::TypeId t1, TypesMgr::TypeId t2) {
    if (Types.isFloatTy(t1) or Types.isFloatTy(t2)) {
        return Types.createFloatTy();
    }
    
    return Types.createIntegerTy();
}

