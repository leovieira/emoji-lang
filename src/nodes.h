#include <iostream>
#include <vector>
#include <map>
#include <set>

#include "backllvm.h"

using namespace std;

int errorcount = 0;

// symbol table
map<string, Value*> symbols;

class Node {
  protected:
    vector<Node*> children;

  public:
    void addChild(Node *n) {
      children.push_back(n);
    }

    vector<Node*> const& getChildren() {
      return children;
    }

    virtual string toStr() {
      return "node";
    }

    virtual Value* codegen() {
      for (Node *n : children) {
        n->codegen();
      }
      return NULL;
    }
};

class Program: public Node {
  public:
    virtual string toStr() override {
      return "program";
    }
};

class Stmts: public Node {
  public:
    virtual string toStr() override {
      return "stmts";
    }
};

class Ident: public Node {
  protected:
    string name;

  public:
    Ident(string name) {
      this->name = name;
    }

    virtual string toStr() override {
      return name;
    }

    const string getName() {
      return name;
    }

    virtual Value* codegen() override {
      Value *symbol = symbols[name];
      AllocaInst* ai = dyn_cast<AllocaInst>(symbol);
      Type *st = ai->getAllocatedType();
      return backend.CreateLoad(st, symbol, name);
    }
};

class Float: public Node {
  protected:
    double value;

  public:
    Float(double v) {
      value = v;
    }

    virtual string toStr() override {
      return to_string(value);
    }

    virtual Value* codegen() override {
      return ConstantFP::get(ctx, APFloat((double)value));
    }
};

class Int: public Node {
  protected:
    int value;

  public:
    Int(int v) {
      value = v;
    }

    virtual string toStr() override {
      return to_string(value);
    }

    virtual Value* codegen() override {
      return ConstantFP::get(ctx, APFloat((double)value));
    }
};

class Str: public Node {
  protected:
    string value;

  public:
    Str(string v) {
      value = v;
    }

    virtual string toStr() override {
      return "\\\"" + value + "\\\"";
    }

    virtual Value* codegen() override {
      return backend.CreateGlobalStringPtr(value);
    }
};

class Attr: public Node {
  protected:
    string ident;

  public:
    Attr(string ident, Node *d) {
      this->ident = ident;
      children.push_back(d);
    }

    virtual string toStr() override {
      string r(ident);
      r.append("=");
      return r;
    }

    const string getIdent() {
      return ident;
    }

    virtual Value* codegen() override {
      Value *dv = children[0]->codegen();
      Value *address = symbols[ident];
      return backend.CreateStore(dv, address);          
    }
};

class Decl: public Node {
  public:
    enum IdType { NUM = 1, STR };

  protected:
    IdType type;
    string ident;

  public:
    Decl(string ident, IdType type) {
      this->type = type;
      this->ident = ident;
    }

    virtual string toStr() override {
      map<IdType,string> list = {{NUM, "NUM"}, {STR, "STR"}};
      return list[type] + " " + ident;
    }

    const string getIdent() {
      return ident;
    }

    const IdType getType() {
      return type;
    }

    virtual Value* codegen() override {
      Type *ty = NULL;
      Value *dv = NULL;

      switch (type) {
        case NUM:
          ty = Type::getDoubleTy(ctx);
          dv = (new Float(0))->codegen();
          break;
        case STR:
          ty = Type::getInt8PtrTy(ctx);
          dv = (new Str(""))->codegen();
          break;
      }

      Value *address = backend.CreateAlloca(ty, 0, NULL, ident);
      symbols[ident] = address;
      return backend.CreateStore(dv, address);
    }
};

class Print: public Node {
  public:
    Print(Node *expr) {
      children.push_back(expr);
    }

    virtual string toStr() override {
      return "print";
    }

    virtual Value* codegen() override {
      Value *exprv = children[0]->codegen();
      vector<Value*> args;
      args.push_back(exprv);
      if (exprv->getType()->isDoubleTy()) {
        return backend.CreateCall(printfloat, args);
      } else if (exprv->getType()->isPointerTy()) {
        return backend.CreateCall(printstr, args);
      } else {
        cerr << "Unable to print" << endl;
        return NULL;
      }
    }
};

class Arit: public Node {
  protected:
    char oper;

  public:
    Arit(Node *left, Node *right, char oper) {
      children.push_back(left);
      children.push_back(right);
      this->oper = oper;
    }

    virtual string toStr() override {
      string r;
      r.push_back(oper);
      return r;
    }

    virtual Value* codegen() override {
      Value *lv = children[0]->codegen();
      Value *rv = children[1]->codegen();

      switch (oper) {
        case '+': return backend.CreateFAdd(lv, rv);
        case '-': return backend.CreateFSub(lv, rv);
        case '*': return backend.CreateFMul(lv, rv);
        case '/': return backend.CreateFDiv(lv, rv);

        default: cerr << "Failed! Operator not implemented: " << oper << endl;
      }

      return NULL;
    }
};

class Inc: public Node {
  protected:
    string ident;

  public:
    Inc(string id) {
      ident = id;
      children.push_back(new Ident(id));
    }

    virtual string toStr() override {
      return "++";
    }

    virtual Value* codegen() override {
      Value *idv = children[0]->codegen();
      Value *exprv = backend.CreateFAdd(idv, (new Int(1))->codegen());
      Value *symbol = symbols[ident];
      return backend.CreateStore(exprv, symbol);
    }
};

class Dec: public Node {
  protected:
    string ident;

  public:
    Dec(string id) {
      ident = id;
      children.push_back(new Ident(id));
    }

    virtual string toStr() override {
      return "--";
    }

    virtual Value* codegen() override {
      Value *idv = children[0]->codegen();
      Value *exprv = backend.CreateFSub(idv, (new Int(1))->codegen());
      Value *symbol = symbols[ident];
      return backend.CreateStore(exprv, symbol);
    }
};

/*

int i = 0;
while (i < 50) {
  i++;
}
print...


entry:
  ldo i,0
  goto lt

lt:
  cmp i,50
  jmplt body,continue

body:
  add i,1
  jmp lt

continue:
  call print...

*/

class While: public Node {
  public:
    While(Node *logical, Node *stmts) {
      children.push_back(logical);
      children.push_back(stmts);
    }

    virtual string toStr() override {
      return "while";
    }

    virtual Value* codegen() override {
      BasicBlock *condition = BasicBlock::Create(ctx, "cond", current_func);
      BasicBlock *body = BasicBlock::Create(ctx, "body", current_func);
      BasicBlock *contin = BasicBlock::Create(ctx, "contin", current_func);

      // setup entry block, goto condition
      backend.CreateBr(condition);

      // setup condition block
      backend.SetInsertPoint(condition);
      Value *expr = children[0]->codegen();
      backend.CreateCondBr(expr, body, contin);

      // setup body block
      backend.SetInsertPoint(body);
      children[1]->codegen();
      backend.CreateBr(condition);

      backend.SetInsertPoint(contin);
      return contin;
    }
};

class If: public Node {
  public:
    If(Node *logical, Node *stmts) {
      children.push_back(logical);
      children.push_back(stmts);
    }

    virtual string toStr() override {
      return "if";
    }

    virtual Value* codegen() override {
      BasicBlock *condition = BasicBlock::Create(ctx, "cond", current_func);
      BasicBlock *body = BasicBlock::Create(ctx, "body", current_func);
      BasicBlock *contin = BasicBlock::Create(ctx, "contin", current_func);

      // setup entry block, goto condition
      backend.CreateBr(condition);

      // setup condition block
      backend.SetInsertPoint(condition);
      Value *expr = children[0]->codegen();
      backend.CreateCondBr(expr, body, contin);

      // setup body block
      backend.SetInsertPoint(body);
      children[1]->codegen();
      backend.CreateBr(contin);

      backend.SetInsertPoint(contin);
      return contin;
    }
};

class IfElse: public Node {
  public:
    IfElse(Node *logical, Node *ifStmts, Node *elseStmts) {
      children.push_back(logical);
      children.push_back(ifStmts);
      children.push_back(elseStmts);
    }

    virtual string toStr() override {
      return "ifElse";
    }

    virtual Value* codegen() override {
      BasicBlock *condition = BasicBlock::Create(ctx, "cond", current_func);
      BasicBlock *ifBody = BasicBlock::Create(ctx, "ifBody", current_func);
      BasicBlock *elseBody = BasicBlock::Create(ctx, "elseBody", current_func);
      BasicBlock *contin = BasicBlock::Create(ctx, "contin", current_func);

      // setup entry block, goto condition
      backend.CreateBr(condition);

      // setup condition block
      backend.SetInsertPoint(condition);
      Value *expr = children[0]->codegen();
      backend.CreateCondBr(expr, ifBody, elseBody);

      // setup ifBody block
      backend.SetInsertPoint(ifBody);
      children[1]->codegen();
      backend.CreateBr(contin);

      // setup elseBody block
      backend.SetInsertPoint(elseBody);
      children[2]->codegen();
      backend.CreateBr(contin);

      backend.SetInsertPoint(contin);
      return contin;
    }
};

class Relational: public Node {
  public:
    enum Oper { EQUAL = 1, DIFF, GREATER, LESS, GREATER_EQUAL, LESS_EQUAL };

  protected:
    Oper oper;

  public:
    Relational(Node *le, Oper oper, Node *re) {
      children.push_back(le);
      children.push_back(re);
      this->oper = oper;
    }

    virtual string toStr() override {
      map<Oper,string> list = {
        {EQUAL, "="},
        {DIFF, "!="},
        {GREATER, ">"},
        {LESS, "<"},
        {GREATER_EQUAL, ">="},
        {LESS_EQUAL, "<="}
      };
      return list[oper];
    }

    virtual Value* codegen() override {
      Value *lv = children[0]->codegen();
      Value *rv = children[1]->codegen();

      switch (oper) {
        case EQUAL: return backend.CreateFCmpOEQ(lv, rv);
        case DIFF: return backend.CreateFCmpONE(lv, rv);
        case GREATER: return backend.CreateFCmpOGT(lv, rv);
        case LESS: return backend.CreateFCmpOLT(lv, rv);
        case GREATER_EQUAL: return backend.CreateFCmpOGE(lv, rv);
        case LESS_EQUAL: return backend.CreateFCmpOLE(lv, rv);

        default: cerr << "Failed! Operator not implemented: " << oper << endl;
      }

      return NULL;
    }
};

class Logical: public Node {
  public:
    enum Oper { AND = 1, OR, NOT };

  protected:
    Oper oper;

  public:
    Logical(Node *lr, Oper oper, Node *rr) {
      children.push_back(lr);
      children.push_back(rr);
      this->oper = oper;
    }

    Logical(Node *r, Oper oper) {
      children.push_back(r);
      this->oper = oper;
    }

    virtual string toStr() override {
      map<Oper,string> list = {
        {AND, "&&"},
        {OR, "||"},
        {NOT, "!"}
      };
      return list[oper];
    }

    virtual Value* codegen() override {
      Value *lv = children[0]->codegen();
      Value *rv = NULL;
      if (children.size() == 2) {
        rv = children[1]->codegen();
      }

      switch (oper) {
        case AND: return backend.CreateAnd(lv, rv);
        case OR: return backend.CreateOr(lv, rv);
        case NOT: return backend.CreateNot(lv);

        default: cerr << "Failed! Operator not implemented: " << oper << endl;
      }

      return NULL;
    }
};

class PrintTree {
  public:
    void printRecursive(Node *n) {
      for (Node *c : n->getChildren()) {
        printRecursive(c);
      }

      cout << "n" << (long)n;
      cout << "[label=\"" << n->toStr() << "\"]";
      cout << ";" << endl;

      for (Node *c : n->getChildren()) {
        cout << "n" << (long)n << " -- " << "n" << (long)c << ";" << endl;
      }
    }

    void print(Node *n) {
      cout << "graph {\n";
      printRecursive(n);
      cout << "}\n";
    }
};

class CheckVars {
  private:
    map<string, Decl::IdType> vars;

  public:
    void checkRecursive(Node *n) {
      // visit left and right
      for (Node *c : n->getChildren()) {
        checkRecursive(c);
      }

      // visit root
      // is Decl node
      Decl *d = dynamic_cast<Decl*>(n);
      if (d) {
        if (vars.count(d->getIdent()) == 0) {
          // new var
          vars[d->getIdent()] = d->getType();
        } else {
          // redeclared var
          cerr << "Redeclared var " << d->getIdent() << endl;
          errorcount++;
        }
      }

      // is Attr node
      Attr *a = dynamic_cast<Attr*>(n);
      if (a) {
        if (vars.count(a->getIdent()) == 0) {
          // undeclared var
          cerr << "Undeclared var " << a->getIdent() << endl;
          errorcount++;
        } else {
          Decl::IdType cTy;
          if (dynamic_cast<Ident*>(a->getChildren()[0])) {
            Ident *i = dynamic_cast<Ident*>(a->getChildren()[0]);
            cTy = vars[i->getName()];
          } else if (dynamic_cast<Str*>(a->getChildren()[0])) {
            cTy = Decl::STR;
          } else {
            cTy = Decl::NUM;
          }

          if (vars[a->getIdent()] != cTy) {
            // incorrect type assigned
            cerr << "Incorrect type assigned to " << a->getIdent() << endl;
            errorcount++;
          }
        }
      }

      Ident *i = dynamic_cast<Ident*>(n);
      if (i) {
        if (vars.count(i->getName()) == 0) {
          // undeclared var
          cerr << "Undeclared var " << i->getName() << endl;
          errorcount++;
        }
      }
    }

    void check(Node *n) {
      checkRecursive(n);
    }
};

class CodeGen {
  public:
    void generate(Node *p, const std::string &outfilename) {
      setup_llvm();
      p->codegen();

      // terminate main function
      Value *retv = ConstantInt::get(ctx, APInt(16, 0));
      backend.CreateRet(retv);

      module->print(outs(), nullptr);
      generate_object(outfilename);
    }
};

