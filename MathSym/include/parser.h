#ifndef PARSER_H
#define PARSER_H

#include "token.h"

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Parser {
public:
    Parser() : _astNodePool(AST_NODE_POOL_SIZE) {}

    bool init(const std::string & configFile, const std::string & semanticsFile = "");

    bool parse(std::vector<Token> & tokens, const std::string & line);

private:
    struct Symbol {
        enum SymbolType {
            TERMINAL, NONTERMINAL, EPSILON, EOFL
        };

        SymbolType type;
        std::string name;
    };

    struct Production {
        std::string lhsSymbol;
        std::vector<Symbol> rhsSymbols;
    };

    struct ASTNode {
        enum ASTNodeType {
            EMPTY,
            BINARY_OPERATOR,
            UNARY_LEFT_OPERATOR,
            OPERAND
        };

        ASTNode * parent = NULL;
        std::vector<ASTNode *> children;
        ASTNodeType type = ASTNodeType::EMPTY;
        Token * token = NULL;
    };

    struct Monomial {
        double coefficient;
        int exponent;
    };

    struct _EvalException : std::runtime_error {
        _EvalException(const std::string & msg) : std::runtime_error(msg) {}
    };

    bool _readConfigFile(const std::string & configFile,
        std::unordered_set<std::string> & nonterminals);

    bool _readSemanticsFile(const std::string & configFile);

    void _computeFIRST(const std::unordered_set<std::string> & nonterminals,
        std::unordered_map<std::string, std::unordered_set<std::string>> & FIRST);
    void _computeFOLLOW(const std::unordered_set<std::string> & nonterminals,
        const std::unordered_map<std::string, std::unordered_set<std::string>> & FIRST,
        std::unordered_map<std::string, std::unordered_set<std::string>> & FOLLOW);
    void _computeFIRST_PLUS(
        const std::unordered_map<std::string, std::unordered_set<std::string>> & FIRST,
        std::unordered_map<std::string, std::unordered_set<std::string>> & FOLLOW,
        std::vector<std::unordered_set<std::string>> & FIRST_PLUS);

    void _constructLL1Table(const std::vector<std::unordered_set<std::string>> & FIRST_PLUS);

    ASTNode * _parseAndCreateParseTree(std::vector<Token> & tokens, const std::string & line);
    ASTNode * _convertParseTreeToAST(ASTNode * astTree);

    void _evalASTTree(ASTNode * astTree);

    void _pruneParseTree(ASTNode * root);
    bool _moveUpOperators(ASTNode * root);

    void _printASTTree(ASTNode * root, int depth);

    std::vector<Monomial> _evalASTNode(ASTNode * node);

    std::vector<Monomial> & _addPolynomial(std::vector<Monomial> & lhs, const std::vector<Monomial> & rhs);
    std::vector<Monomial> & _subtractPolynomial(std::vector<Monomial> & lhs, const std::vector<Monomial> & rhs);
    std::vector<Monomial> _multiplyPolynomials(const std::vector<Monomial> & lhs, const std::vector<Monomial> & rhs);
    std::vector<Monomial> _dividePolynomials(const std::vector<Monomial> & lhs, const std::vector<Monomial> & rhs);


    std::vector<Production> _productions;
    std::string _startSymbol;
    std::unordered_map<std::string, std::unordered_map<std::string, int>> _ll1Table;

    static const std::unordered_set<std::string> _binaryOperators;
    std::unordered_map<int, int> _unaryOperators;
    std::unordered_set<std::string> _unusedTerminals;


    inline ASTNode * _getASTNode() {
        if (_astNodePoolEnd == _astNodePool.size())
            return new ASTNode();
        _astNodePool[_astNodePoolEnd].children = std::vector<ASTNode *>();
        return &_astNodePool[_astNodePoolEnd++];
    }

    static const int AST_NODE_POOL_SIZE = 500;
    std::vector<ASTNode> _astNodePool;
    size_t _astNodePoolEnd = 0;
};

#endif // !PARSER_H
