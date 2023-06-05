//////////////////////////////////////////////////////////////////////
//
//    SymbolsVisitor - Walk the parser tree to register symbols
//                     for the Asl programming language
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

#include "SymbolsVisitor.h"
#include "antlr4-runtime.h"

#include "../common/TypesMgr.h"
#include "../common/SymTable.h"
#include "../common/TreeDecoration.h"
#include "../common/SemErrors.h"

#include <iostream>
#include <string>
#include <vector>

#include <cstddef>    // std::size_t

// uncomment the following line to enable debugging messages with DEBUG*
// #define DEBUG_BUILD
#include "../common/debug.h"

// using namespace std;


// Constructor
SymbolsVisitor::SymbolsVisitor(TypesMgr       & Types,
                               SymTable       & Symbols,
                               TreeDecoration & Decorations,
                               SemErrors      & Errors) :
  Types{Types},
  Symbols{Symbols},
  Decorations{Decorations},
  Errors{Errors} {
}

// Methods to visit each kind of node:
//
antlrcpp::Any SymbolsVisitor::visitProgram(AslParser::ProgramContext *ctx) {
  DEBUG_ENTER();
  SymTable::ScopeId sc = Symbols.pushNewScope(SymTable::GLOBAL_SCOPE_NAME);
  putScopeDecor(ctx, sc);
  for (auto ctxFunc : ctx->function()) { 
    visit(ctxFunc);
  }
  // Symbols.print();
  Symbols.popScope();
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any SymbolsVisitor::visitFunction(AslParser::FunctionContext *ctx) {
  DEBUG_ENTER();
  // WARNING: SI SE MUEVE NO VA   --------------------------------------------------------
  std::string funcName = ctx->ID()->getText();
  SymTable::ScopeId sc = Symbols.pushNewScope(funcName);
  putScopeDecor(ctx, sc);
  
  // Handle type for 'parameter' values 
  std::vector<TypesMgr::TypeId> lParamsTy;

  int numOfParameters = ctx->parameter().size();
  for (int i=0; i<numOfParameters; ++i) {
    // Visita el parametro
    visit(ctx->parameter(i));
    
    // Coge el tipo del parametro
    TypesMgr::TypeId tParam = getTypeDecor(ctx->parameter(i));
    
    // Añade el tipo del parametro a la lista
    lParamsTy.push_back(tParam);
  }
  
  visit(ctx->declarations());
  
  //Symbols.print();
  Symbols.popScope();
  // --------------------------------------------------------------------------------------
  
  if (Symbols.findInCurrentScope(funcName)) {
    Errors.declaredIdent(ctx->ID());
  } else {
    // Handle type for 'return' value
    // Asigna por defecto el tipo de return a 'void'
    TypesMgr::TypeId tRet = Types.createVoidTy();
    // Comprueba si la funcion tiene valor de 'return'
    if (ctx->basic_type()) {
        // Visita el type
        visit(ctx->basic_type());
        // Coge el tipo del return correspondiente
        tRet = getTypeDecor(ctx->basic_type());
    }
    
    // Asigna los types de los parametros y el valor return de la function
    TypesMgr::TypeId tFunc = Types.createFunctionTy(lParamsTy, tRet);
    
    putTypeDecor(ctx, tFunc);
    
    // Añade la funcion en la tabla de simbolos
    Symbols.addFunction(funcName, tFunc);
  }
  
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any SymbolsVisitor::visitParameter(AslParser::ParameterContext *ctx) {
  DEBUG_ENTER();
  std::string ident = ctx->ID()->getText();
  if (Symbols.findInCurrentScope(ident)) {
    Errors.declaredIdent(ctx->ID());
  } else {
    // Visita el type
    visit(ctx->type());
    
    // Coge el tipo del parametro
    TypesMgr::TypeId t1 = getTypeDecor(ctx->type());
    putTypeDecor(ctx, t1);
    
    // Guarda el parametro en la tabla de simbolos
    Symbols.addParameter(ident, t1);
  }
  
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any SymbolsVisitor::visitDeclarations(AslParser::DeclarationsContext *ctx) {
  DEBUG_ENTER();
  visitChildren(ctx);
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any SymbolsVisitor::visitVariable_decl(AslParser::Variable_declContext *ctx) {
  DEBUG_ENTER();
  
  // Visita el type
  visit(ctx->type());
  
  // Coge el tipo de la declaracion de variables
  TypesMgr::TypeId t1 = getTypeDecor(ctx->type());
  
  // Para cada ID de la declaracion de variables (ID >= 1)
  int numOfIds = ctx->ID().size();
  for (int i = 0; i < numOfIds; ++i) {
    // Coge el name de la variable 
    std::string ident = ctx->ID(i)->getText();

    if (Symbols.findInCurrentScope(ident)) {
      Errors.declaredIdent(ctx->ID(i));
    } else {
      // Guarda el identificador como variable con su tipo
      Symbols.addLocalVar(ident, t1);
    }
  }
  
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any SymbolsVisitor::visitType(AslParser::TypeContext *ctx) {
  DEBUG_ENTER();
  
  if (ctx->ARRAY()) { // case: array type
    visit(ctx->basic_type());
    TypesMgr::TypeId tElem = getTypeDecor(ctx->basic_type());
  
    // Coge el tamanyo del array
    unsigned int size = std::stoi(ctx->INTVAL(0)->getText());
    
    // Construye el array con el tamanyo y el tipo de elemento
    TypesMgr::TypeId tArr = Types.createArrayTy(size, tElem);
    putTypeDecor(ctx, tArr);
  } else if (ctx->MATRIX()) { // case: matrix type
    visit(ctx->basic_type());
    TypesMgr::TypeId tElem = getTypeDecor(ctx->basic_type());
    
    unsigned int rows = std::stoi(ctx->INTVAL(0)->getText());
    unsigned int cols = std::stoi(ctx->INTVAL(1)->getText());
    
    TypesMgr::TypeId tMatrix = Types.createMatrixTy(rows, cols, tElem);
    putTypeDecor(ctx, tMatrix);
  } else { // case: basic type
    visit(ctx->basic_type());
  
    TypesMgr::TypeId t = getTypeDecor(ctx->basic_type());
    putTypeDecor(ctx, t);
  }
  
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any SymbolsVisitor::visitBasic_type(AslParser::Basic_typeContext *ctx) {
  DEBUG_ENTER();
  
  if (ctx->INT()) {
    TypesMgr::TypeId t = Types.createIntegerTy();
    putTypeDecor(ctx, t);
  } else if (ctx->FLOAT()) {
    TypesMgr::TypeId t = Types.createFloatTy();
    putTypeDecor(ctx, t);    
  } else if (ctx->BOOL()) {
    TypesMgr::TypeId t = Types.createBooleanTy();
    putTypeDecor(ctx, t);
  } else if (ctx->CHAR()) {
    TypesMgr::TypeId t = Types.createCharacterTy();
    putTypeDecor(ctx, t);
  }
  
  DEBUG_EXIT();
  return 0;
}

// Getters for the necessary tree node atributes:
//   Scope and Type
SymTable::ScopeId SymbolsVisitor::getScopeDecor(antlr4::ParserRuleContext *ctx) {
  return Decorations.getScope(ctx);
}
TypesMgr::TypeId SymbolsVisitor::getTypeDecor(antlr4::ParserRuleContext *ctx) {
  return Decorations.getType(ctx);
}

// Setters for the necessary tree node attributes:
//   Scope and Type
void SymbolsVisitor::putScopeDecor(antlr4::ParserRuleContext *ctx, SymTable::ScopeId s) {
  Decorations.putScope(ctx, s);
}
void SymbolsVisitor::putTypeDecor(antlr4::ParserRuleContext *ctx, TypesMgr::TypeId t) {
  Decorations.putType(ctx, t);
}
