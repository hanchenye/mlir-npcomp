//===- CPASupport.h - Support types and utilities for CPA -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Support types and utilities for the Cartesian Product Algorithm for
// Type Inference.
//
// See:
//   http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.30.8177
//   http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.129.2756
//===----------------------------------------------------------------------===//

#include "mlir/IR/Types.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/ilist.h"
#include "llvm/Support/Allocator.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"

#ifndef NPCOMP_TYPING_CPA_SUPPORT_H
#define NPCOMP_TYPING_CPA_SUPPORT_H

namespace mlir {
namespace NPCOMP {
namespace Typing {
namespace CPA {

class Context;

/// A uniqued string identifier.
class Identifier {
public:
  StringRef getValue() const { return value; }

  void print(raw_ostream &os, bool brief = false);

  friend raw_ostream &operator<<(raw_ostream &os, Identifier ident) {
    ident.print(os);
    return os;
  }

private:
  Identifier(StringRef value) : value(value) {}
  StringRef value;
  friend class Context;
};

/// Base class for the CPA type hierarchy.
class ObjectBase {
public:
  enum class Kind {
    // Type
    FIRST_TYPE,
    TypeBase = FIRST_TYPE,
    TypeVar,
    CastType,
    ReadType,
    WriteType,

    // ValueType
    FIRST_VALUE_TYPE,
    ValueType = FIRST_VALUE_TYPE,
    IRValueType,
    ObjectValueType,
    LAST_VALUE_TYPE = ObjectValueType,

    LAST_TYPE = TypeVar,

    // Constraint
    Constraint,
    ConstraintSet,
    TypeVarSet,
  };
  ObjectBase(Kind kind) : kind(kind) {}
  virtual ~ObjectBase();

  Kind getKind() const { return kind; }

  virtual void print(raw_ostream &os, bool brief = false) = 0;

private:
  const Kind kind;
};

/// Base class for types.
/// This type hierarchy is adapted from section 2.1 of:
///   Precise Constraint-Based Type Inference for Java
///
/// Referred to as: 'τ' (tau)
class TypeBase : public ObjectBase {
public:
  using ObjectBase::ObjectBase;
  static bool classof(const ObjectBase *tb) {
    return tb->getKind() >= Kind::FIRST_TYPE &&
           tb->getKind() <= Kind::LAST_TYPE;
  }

private:
};

/// A unique type variable.
/// Both the pointer and the ordinal will be unique within a context.
/// Referred to as 't'
class TypeVar : public TypeBase, public llvm::ilist_node<TypeVar> {
public:
  static bool classof(const ObjectBase *tb) {
    return tb->getKind() == Kind::TypeVar;
  }

  int getOrdinal() { return ordinal; }

  void print(raw_ostream &os, bool brief = false) override;

  /// Every instantiated type can be anchored. This is purely used for
  /// re-association at a later time with the originaing IR.
  Value getAnchorValue() { return anchorValue; }
  void setAnchorValue(Value anchorValue) { this->anchorValue = anchorValue; }

private:
  TypeVar(int ordinal) : TypeBase(Kind::TypeVar), ordinal(ordinal) {}
  int ordinal;
  Value anchorValue;
  friend class Context;
};

/// A type-cast type.
/// Referred to as: 'cast(δ, t)'
class CastType : public TypeBase {
public:
  static bool classof(const ObjectBase *tb) {
    return tb->getKind() == Kind::CastType;
  }

  Identifier *getTypeIdentifier() { return typeIdentifier; }
  TypeVar *getTypeVar() { return typeVar; }

  void print(raw_ostream &os, bool brief = false) override;

private:
  CastType(Identifier *typeIdentifier, TypeVar *typeVar)
      : TypeBase(Kind::CastType), typeIdentifier(typeIdentifier),
        typeVar(typeVar) {}
  Identifier *typeIdentifier;
  TypeVar *typeVar;
  friend class Context;
};

/// Type representing a read-field operation.
/// Referred to as: 'read τ'
class ReadType : public TypeBase {
public:
  static bool classof(const ObjectBase *tb) {
    return tb->getKind() == Kind::ReadType;
  }

  TypeBase *getType() { return type; }

  void print(raw_ostream &os, bool brief = false) override;

private:
  ReadType(TypeBase *type) : TypeBase(Kind::ReadType), type(type) {}
  TypeBase *type;
  friend class Context;
};

/// Type representing a read-field operation.
/// Referred to as: 'read τ'
class WriteType : public TypeBase {
public:
  static bool classof(const ObjectBase *tb) {
    return tb->getKind() == Kind::WriteType;
  }

  TypeBase *getType() { return type; }

  void print(raw_ostream &os, bool brief = false) override;

private:
  WriteType(TypeBase *type) : TypeBase(Kind::WriteType), type(type) {}
  TypeBase *type;
  friend class Context;
};

/// A legal value type in the language. We represent this as one of:
///   IRValueType: Wraps a primitive MLIR type
///   ObjectValueType: Defines an object.
/// Referred to as 'τv' (tau-v)
class ValueType : public TypeBase {
public:
  using TypeBase::TypeBase;
  bool classof(ObjectBase *ob) {
    return ob->getKind() >= Kind::FIRST_VALUE_TYPE &&
           ob->getKind() <= Kind::LAST_VALUE_TYPE;
  }
};

/// Concrete ValueType that wraps an MLIR Type.
class IRValueType : public ValueType {
public:
  IRValueType(mlir::Type irType)
      : ValueType(Kind::IRValueType), irType(irType) {}
  static bool classof(ObjectBase *ob) {
    return ob->getKind() == Kind::IRValueType;
  }

  mlir::Type getIrType() { return irType; }

  void print(raw_ostream &os, bool brief = false) override;

private:
  const mlir::Type irType;
};

/// ValueType for an object.
/// Referred to as 'obj(δ, [ li : τi ])'
class ObjectValueType : public ValueType {
public:
  static bool classof(ObjectBase *ob) {
    return ob->getKind() == Kind::ObjectValueType;
  }

  Identifier *getTypeIdentifier() { return typeIdentifier; }
  size_t getFieldCount() { return fieldCount; }
  llvm::ArrayRef<Identifier *> getFieldIdentifiers() {
    return llvm::ArrayRef<Identifier *>(fieldIdentifiers, fieldCount);
  }
  llvm::ArrayRef<TypeBase *> getFieldTypes() {
    return llvm::ArrayRef<TypeBase *>(fieldTypes, fieldCount);
  }

  void print(raw_ostream &os, bool brief = false) override;

private:
  ObjectValueType(Identifier *typeIdentifier, size_t fieldCount,
                  Identifier **fieldIdentifiers, TypeBase **fieldTypes)
      : ValueType(Kind::ObjectValueType), typeIdentifier(typeIdentifier),
        fieldCount(fieldCount), fieldIdentifiers(fieldIdentifiers),
        fieldTypes(fieldTypes) {}
  Identifier *typeIdentifier;
  size_t fieldCount;
  Identifier **fieldIdentifiers;
  TypeBase **fieldTypes;
  friend class Context;
};

/// A Constraint between two types.
/// Referred to as: 'τ1 <: τ2'
class Constraint : public ObjectBase, public llvm::ilist_node<Constraint> {
public:
  static bool classof(ObjectBase *ob) {
    return ob->getKind() == Kind::Constraint;
  }

  TypeBase *getT1() { return t1; }
  TypeBase *getT2() { return t2; }

  void setContextOp(Operation *contextOp) { this->contextOp = contextOp; }

  void print(raw_ostream &os, bool brief = false) override;

private:
  Constraint(TypeBase *t1, TypeBase *t2)
      : ObjectBase(Kind::Constraint), t1(t1), t2(t2) {}
  TypeBase *t1;
  TypeBase *t2;
  Operation *contextOp = nullptr;
  friend class Context;
};

/// A set of constraints.
/// Referred to as: 'C'
class ConstraintSet : public ObjectBase {
public:
  static bool classof(ObjectBase *ob) {
    return ob->getKind() == Kind::ConstraintSet;
  }

  llvm::simple_ilist<Constraint> &getConstraints() { return constraints; }

  void print(raw_ostream &os, bool brief = false) override;

private:
  ConstraintSet() : ObjectBase(Kind::ConstraintSet){};
  llvm::simple_ilist<Constraint> constraints;
  friend class Context;
};

/// A set of TypeVar.
/// Referred to as 't_bar'
class TypeVarSet : public ObjectBase {
public:
  static bool classof(ObjectBase *ob) {
    return ob->getKind() == Kind::TypeVarSet;
  }

  llvm::simple_ilist<TypeVar> &getTypeVars() { return typeVars; }

  void print(raw_ostream &os, bool brief = false) override;

private:
  TypeVarSet() : ObjectBase(Kind::TypeVarSet) {}
  llvm::simple_ilist<TypeVar> typeVars;
  friend class Context;
};

/// Represents an evaluation scope (i.e. a "countour" in the literature) that
/// tracks type variables, IR associations and constraints.
class Environment {
public:
  Environment(Context &context);

  Context &getContext() { return context; }
  ConstraintSet *getConstraints() { return constraints; }
  TypeVarSet *getTypeVars() { return typeVars; }

  /// Maps an IR value to a CPA type by applying an IR Type -> CPA Type
  /// transfer function if not already mapped.
  TypeBase *mapValueToType(Value value);

private:
  Context &context;
  ConstraintSet *constraints;
  TypeVarSet *typeVars;
  llvm::DenseMap<Value, TypeBase *> valueTypeMap;
};

/// Manages instances and containers needed for the lifetime of a CPA
/// analysis.
class Context {
public:
  /// Maps an IR Type to a CPA TypeBase.
  /// This is currently not overridable but a hook may need to be provided
  /// eventually.
  TypeBase *mapIrType(::mlir::Type irType);

  TypeVar *newTypeVar() {
    TypeVar *tv = allocator.Allocate<TypeVar>(1);
    new (tv) TypeVar(++typeVarCounter);
    return tv;
  }

  /// Gets a uniqued IRValueType for the IR Type.
  IRValueType *getIRValueType(Type irType) {
    auto it = irValueTypeMap.find(irType);
    if (it != irValueTypeMap.end())
      return it->second;
    auto *irv = allocator.Allocate<IRValueType>(1);
    new (irv) IRValueType(irType);
    irValueTypeMap[irType] = irv;
    return irv;
  }

  /// Creates a new ObjectValueType.
  /// Object value types are not uniqued.
  ObjectValueType *
  newObjectValueType(Identifier *typeIdentifier,
                     llvm::ArrayRef<Identifier *> fieldIdentifiers) {
    size_t n = fieldIdentifiers.size();
    Identifier **allocFieldIdentifiers = allocator.Allocate<Identifier *>(n);
    std::copy_n(fieldIdentifiers.begin(), n, allocFieldIdentifiers);
    TypeBase **allocFieldTypes = allocator.Allocate<TypeBase *>(n);
    std::fill_n(allocFieldTypes, n, nullptr);
    auto *ovt = allocator.Allocate<ObjectValueType>(1);
    new (ovt) ObjectValueType(typeIdentifier, n, allocFieldIdentifiers,
                              allocFieldTypes);
    return ovt;
  }

  /// Gets a uniqued Identifier for the given value.
  Identifier *getIdentifier(StringRef value) {
    auto it = identifierMap.find(value);
    if (it != identifierMap.end())
      return it->second;
    auto *chars = allocator.Allocate<char>(value.size());
    std::memcpy(chars, value.data(), value.size());
    StringRef uniquedValue(chars, value.size());
    Identifier *id = allocator.Allocate<Identifier>(1);
    new (id) Identifier(uniquedValue);
    identifierMap[uniquedValue] = id;
    return id;
  }

  /// Gets a CastType.
  CastType *getCastType(Identifier *typeIdentifier, TypeVar *typeVar) {
    auto *ct = allocator.Allocate<CastType>(1);
    new (ct) CastType(typeIdentifier, typeVar);
    return ct;
  }

  /// Gets a ReadType.
  ReadType *getReadType(TypeBase *type) {
    auto *rt = allocator.Allocate<ReadType>(1);
    new (rt) ReadType(type);
    return rt;
  }

  /// Gets a WriteType.
  WriteType *getWriteType(TypeBase *type) {
    auto *wt = allocator.Allocate<WriteType>(1);
    new (wt) WriteType(type);
    return wt;
  }

  /// Creates a Constraint.
  Constraint *newConstraint(TypeBase *t1, TypeBase *t2) {
    auto *c = allocator.Allocate<Constraint>(1);
    new (c) Constraint(t1, t2);
    return c;
  }

  /// Creates a new ConstraintSet.
  ConstraintSet *newConstraintSet() {
    auto *cs = allocator.Allocate<ConstraintSet>(1);
    new (cs) ConstraintSet();
    return cs;
  }

  /// Creates a new TypeVarSet.
  TypeVarSet *newTypeVarSet() {
    auto *tvs = allocator.Allocate<TypeVarSet>(1);
    new (tvs) TypeVarSet();
    return tvs;
  }

private:
  llvm::BumpPtrAllocator allocator;
  llvm::DenseMap<mlir::Type, IRValueType *> irValueTypeMap;
  llvm::DenseMap<StringRef, Identifier *> identifierMap;
  int typeVarCounter = 0;
};

} // namespace CPA
} // namespace Typing
} // namespace NPCOMP
} // namespace mlir

#endif // NPCOMP_TYPING_CPA_SUPPORT_H