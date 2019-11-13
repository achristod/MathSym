#include "parser.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <regex>
#include <stack>

//#define LOG_DEBUG

using namespace std;


const unordered_set<string> Parser::_binaryOperators = { "+", "-", "*", "/", "=" };


bool Parser::init(const string & configFile, const string & semanticsFile) {

    unordered_set<string> nonterminals;
    if (!_readConfigFile(configFile, nonterminals))
        return false;

    unordered_map<string, unordered_set<string>> FIRST;
    _computeFIRST(nonterminals, FIRST);

    unordered_map<string, unordered_set<string>> FOLLOW;
    _computeFOLLOW(nonterminals, FIRST, FOLLOW);

    vector<unordered_set<string>> FIRST_PLUS(_productions.size());
    _computeFIRST_PLUS(FIRST, FOLLOW, FIRST_PLUS);

    _constructLL1Table(FIRST_PLUS);

    if (semanticsFile != "" && !_readSemanticsFile(semanticsFile))
        return false;

    return true;
}


bool Parser::parse(vector<Token> & tokens, const string & line) {

    ASTNode * astTree = _parseAndCreateParseTree(tokens, line);
    if (!astTree)
        return false;

    astTree = _convertParseTreeToAST(astTree);

    _evalASTTree(astTree);

    return true;
}


bool Parser::_readConfigFile(const std::string & configFile,
    std::unordered_set<std::string> & nonterminals) {

    ifstream ifs(configFile);
    if (ifs.fail()) {
        cerr << "Error: Failed to open tokenizer config file " << configFile << endl;
        return false;
    }

    int lineCount = 0;
    while (ifs) {
        string line;
        getline(ifs, line);
        lineCount++;

        // Make sure the line is non-empty and non-comment
        auto itFirstChar = find_if(line.begin(), line.end(), [](char c) {return !isspace(c); });
        if (itFirstChar == line.end() || *itFirstChar == '#')
            continue;

        auto delimPos = line.find("->");
        if (delimPos == string::npos || delimPos == 0) {
            cerr << "Error: Malformed line " << lineCount << " in file "
                << configFile << endl;
            return false;
        }

        // Read the symbol on the left-hand side of the production (always nonterminal)
        string lhsSymbol = line.substr(0, delimPos);
        auto lineEnd = remove_if(lhsSymbol.begin(), lhsSymbol.end(), isspace);
        lhsSymbol.erase(lineEnd, lhsSymbol.end());

        // The first production by default contains the start symbol
        if (nonterminals.size() == 0)
            _startSymbol = lhsSymbol;
        nonterminals.insert(lhsSymbol);

        // Read all the symbols on the right-hand side of the production
        vector<Symbol> rhsSymbols;
        regex rhsRegex("[^ \\t\\n]+");
        for (auto it = sregex_iterator(line.begin() + delimPos + 2, line.end(), rhsRegex);
            it != sregex_iterator(); ++it) {
            const auto & match = *it;
            rhsSymbols.push_back(Symbol{
                (match.str() == "^e$" ? Symbol::EPSILON : Symbol::TERMINAL),
                match.str()
            });
        }

        if (rhsSymbols.size() == 0) {
            cerr << "Error: Malformed line " << lineCount << " in file "
                << configFile << endl;
            return false;
        }

        _productions.push_back({ lhsSymbol, rhsSymbols });
    }

    // Go through all productions and mark all the nonterminal symbols
    for (auto & production : _productions) {
        for (auto & symbol : production.rhsSymbols) {
            if (nonterminals.find(symbol.name) != nonterminals.end())
                symbol.type = Symbol::NONTERMINAL;
        }
    }

#ifdef LOG_DEBUG
    cout << "Productions" << endl;
    cout << "-----------" << endl;
    for (const auto & production : _productions) {
        cout << production.lhsSymbol << " -> ";
        for (const auto & s : production.rhsSymbols)
            cout << '(' << s.type << ',' << s.name << ") ";
        cout << endl;
    }
    cout << endl;
#endif LOG_DEBUG

    return true;
}

bool Parser::_readSemanticsFile(const std::string & configFile) {

    static const int SEMANTICS_UNARY_LEFT_OPERATOR = 0;
    static const int SEMANTICS_UNUSED_TERMINAL = 1;

    ifstream ifs(configFile);
    if (ifs.fail()) {
        cerr << "Error: Failed to open semantics config file " << configFile << endl;
        return false;
    }

    int lineCount = 0;
    while (ifs) {
        string line;
        getline(ifs, line);
        lineCount++;

        // Make sure the line is non-empty and non-comment
        auto itFirstChar = find_if(line.begin(), line.end(), [](char c) {return !isspace(c); });
        if (itFirstChar == line.end() || *itFirstChar == '#')
            continue;

        // Tokenize the line, split by whitespace
        regex regex("[^ \\t\\n]+");
        auto it = sregex_iterator(line.begin(), line.end(), regex);
        if (it == sregex_iterator())
            continue;

        const auto & match = *it;
        switch (atoi(match.str().c_str())) {
            case SEMANTICS_UNARY_LEFT_OPERATOR:
                {
                    vector<int> args;
                    for (++it; it != sregex_iterator(); ++it) {
                        const auto & match = *it;
                        args.push_back(atoi(match.str().c_str()));
                    }
                    _unaryOperators[args[0]] = args[1];
                }
                break;

            case SEMANTICS_UNUSED_TERMINAL:
                _unusedTerminals.insert((*(++it)).str());
                break;
        }
    }

    return true;
}

//
// Computes the set FIRST(A) for each nonterminal symbol A, i.e. the set of
// terminal symbols that can appear as the first symbol in some sequence
// derived from A. The special epsilon symbol is denoted by "^e$";
//
void Parser::_computeFIRST(const unordered_set<string> & nonterminals,
    unordered_map<string, unordered_set<string>> & FIRST) {

    for (const auto & nonterminal : nonterminals)
        FIRST[nonterminal] = unordered_set<string>();

    bool setsChanged = true;
    while (setsChanged) {
        setsChanged = false;
        for (const auto & production : _productions) {
            unordered_set<string> rhs;
            for (const auto & symbol : production.rhsSymbols) {

                rhs.erase("^e$");
                if (symbol.type == Symbol::EPSILON) {
                    rhs.insert(symbol.name);
                } else if (symbol.type == Symbol::TERMINAL) {
                    rhs.insert(symbol.name);
                    break;
                } else if (symbol.type == Symbol::NONTERMINAL) {
                    for (const auto & symbol : FIRST[symbol.name])
                        rhs.insert(symbol);
                    if (rhs.find("^e$") == rhs.end())
                        break;
                }
            }

            // FIRST(A) = FIRST(A) U rhs
            auto & FIRSTSet = FIRST[production.lhsSymbol];
            for (const auto & symbol : rhs) {
                if (FIRSTSet.find(symbol) == FIRSTSet.end())
                    setsChanged = true;
                FIRSTSet.insert(symbol);
            }
        }
    }

#ifdef LOG_DEBUG
    cout << "FIRST sets" << endl;
    cout << "----------" << endl;
    for (const auto & entry : FIRST) {
        cout << entry.first << " : ";
        for (const auto & symbol : entry.second)
            cout << symbol << " ";
        cout << endl;
    }
    cout << endl;
#endif LOG_DEBUG
}

//
// Computes the set FOLLOW(A) set for each nonterminal symbol A, i.e. the set
// of terminal symbols that can appear to the immediate right of a sequence
// derived from A. The special EOF symbol is denoted by "^\0$";
//
void Parser::_computeFOLLOW(const unordered_set<string> & nonterminals,
    const unordered_map<string, unordered_set<string>> & FIRST,
    unordered_map<string, unordered_set<string>> & FOLLOW) {

    for (const auto & nonterminal : nonterminals)
        FOLLOW[nonterminal] = unordered_set<string>();
    FOLLOW[_startSymbol].insert("^\0$");

    bool setsChanged = true;
    while (setsChanged) {
        setsChanged = false;
        for (const auto & production : _productions) {
            unordered_set<string> trailer = FOLLOW[production.lhsSymbol];
            for (int i = production.rhsSymbols.size(); i > 0; i--) {

                const auto & symbol = production.rhsSymbols[i - 1];
                if (symbol.type != Symbol::NONTERMINAL) {
                    trailer.clear();
                    trailer.insert(symbol.name);
                } else {
                    // FOLLOW(A) = FOLLOW(A) U trailer
                    auto & FOLLOWSet = FOLLOW[symbol.name];
                    for (const auto & tsymbol : trailer) {
                        if (FOLLOWSet.find(tsymbol) == FOLLOWSet.end())
                            setsChanged = true;
                        FOLLOWSet.insert(tsymbol);
                    }

                    const auto & FIRSTSet = FIRST.find(symbol.name)->second;
                    if (FIRSTSet.find("^e$") != FIRSTSet.end()) {
                        // trailer = trailer U (FIRST(A) - epsilon)
                        for (const auto & symbol : FIRSTSet)
                            trailer.insert(symbol);
                        trailer.erase("^e$");
                    } else {
                        trailer = FIRSTSet;
                    }
                }
            }
        }
    }

#ifdef LOG_DEBUG
    cout << "FOLLOW sets" << endl;
    cout << "-----------" << endl;
    for (const auto & entry : FOLLOW) {
        cout << entry.first << " : ";
        for (const auto & symbol : entry.second)
            cout << (symbol != "^\0$" ? symbol : "EOF") << " ";
        cout << endl;
    }
    cout << endl;
#endif LOG_DEBUG
}

//
// Computes the set FIRST+ for each production, defined as:
//
//     FIRST+(A -> b) = FIRST(b)              , if epsilon not in FIRST(b)
//                      FIRST(b) U FOLLOW(A)  , otherwise
//
void Parser::_computeFIRST_PLUS(const unordered_map<string, unordered_set<string>> & FIRST,
    unordered_map<string, unordered_set<string>> & FOLLOW,
    vector<unordered_set<string>> & FIRST_PLUS) {

    for (size_t i = 0; i < _productions.size(); i++) {
        const auto & production = _productions[i];
        auto & FIRSTPSet = FIRST_PLUS[i];

        // FIRST+(A -> b) = FIRST(b)
        for (const auto & symbol : production.rhsSymbols) {
            FIRSTPSet.erase("^e$");
            if (symbol.type == Symbol::EPSILON) {
                FIRSTPSet.insert(symbol.name);
            } else if (symbol.type == Symbol::TERMINAL) {
                FIRSTPSet.insert(symbol.name);
                break;
            } else if (symbol.type == Symbol::NONTERMINAL) {
                const auto & FIRSTSet = FIRST.find(symbol.name)->second;
                for (const auto & symbol : FIRSTSet)
                    FIRSTPSet.insert(symbol);
                if (FIRSTPSet.find("^e$") == FIRSTPSet.end())
                    break;
            }
        }

        if (FIRSTPSet.find("^e$") != FIRSTPSet.end()) {
            // FIRST+(A -> b) = FIRST+(A -> b) U FOLLOW(A)
            for (const auto & symbol : FOLLOW[production.lhsSymbol])
                FIRSTPSet.insert(symbol);
        }
    }

#ifdef LOG_DEBUG
    cout << "FIRST+ sets" << endl;
    cout << "-----------" << endl;
    for (size_t i = 0; i < FIRST_PLUS.size(); i++) {
        const auto & FIRSTPSet = FIRST_PLUS[i];
        cout << i << " : ";
        for (const auto & symbol : FIRSTPSet)
            cout << (symbol != "^\0$" ? symbol : "EOF") << " ";
        cout << endl;
    }
    cout << endl;
#endif LOG_DEBUG
}

void Parser::_constructLL1Table(const vector<unordered_set<string>> & FIRST_PLUS) {
    for (size_t i = 0; i < _productions.size(); i++) {
        const auto & lhsSymbol = _productions[i].lhsSymbol;
        if (_ll1Table.find(lhsSymbol) == _ll1Table.end())
            _ll1Table[lhsSymbol] = unordered_map<string, int>();
        for (const auto & symbol : FIRST_PLUS[i])
            if (symbol != "^e$")
                _ll1Table[lhsSymbol][symbol] = i;
    }

#ifdef LOG_DEBUG
    cout << "LL(1) Table" << endl;
    cout << "-----------" << endl;
    for (const auto & nonterminalEntry : _ll1Table) {
        cout << nonterminalEntry.first << " : ";
        for (const auto & terminalEntry : nonterminalEntry.second)
            cout << '(' << (terminalEntry.first != "^\0$" ? terminalEntry.first : "EOF")
            << ',' << terminalEntry.second << ')';
        cout << endl;
    }
    cout << endl;
#endif LOG_DEBUG
}


Parser::ASTNode * Parser::_parseAndCreateParseTree(vector<Token> & tokens, const string & line) {

    tokens.push_back({ "^\0$", "^\0$" });
    int nextInputToken = 0;

    vector<Symbol> parseStack;
    parseStack.reserve(tokens.size());
    parseStack.push_back(Symbol{ Symbol::EOFL, "^\0$" });
    parseStack.push_back(Symbol{ Symbol::NONTERMINAL, _startSymbol });

    _astNodePoolEnd = 0;
    ASTNode * astTree = _getASTNode();
    astTree->type = ASTNode::EMPTY;
    std::stack<ASTNode*> astStack;
    astStack.push(astTree);

    size_t linePos = 0;
    while (true) {

#ifdef LOG_DEBUG
        cout << "Parse stack: ";
        for (size_t i = parseStack.size(); i > 0; i--)
            cout << '(' << parseStack[i - 1].type << ','
            << (parseStack[i - 1].name != "^\0$" ? parseStack[i - 1].name : "EOF") << ") ";
        cout << "    Next token: " << '(' << tokens[nextInputToken].type << ','
            << (tokens[nextInputToken].value != "^\0$" ? tokens[nextInputToken].value : "EOF")
            << ") " << endl;
#endif LOG_DEBUG

        Symbol stackTop = parseStack.back();
        if (stackTop.type == Symbol::EPSILON) {
            parseStack.pop_back();

        } else if (stackTop.type == Symbol::EOFL) {
            if (tokens[nextInputToken].value == "^\0$") {
                break;
            } else {
                cerr << line << endl;
                for (size_t i = 0; i < linePos; i++)  cerr << ' ';
                cerr << '|' << endl << "Error: Wrong syntax" << endl << endl;
                return NULL;
            }

        } else if (stackTop.type == Symbol::TERMINAL) {
            if (stackTop.name == tokens[nextInputToken].type) {
                parseStack.pop_back();
                linePos += tokens[nextInputToken].value.size();

                if (_unusedTerminals.find(stackTop.name) == _unusedTerminals.end()) {
                    auto * astStackTop = astStack.top();
                    astStackTop->token = &tokens[nextInputToken];
                    astStack.pop();
                }
                nextInputToken++;
            } else {
                cerr << line << endl;
                for (size_t i = 0; i < linePos; i++)  cerr << ' ';
                cerr << '|' << endl << "Error: Wrong syntax" << endl << endl;
                return NULL;
            }

        } else {   // Top of stack is nonterminal
            auto it = _ll1Table[stackTop.name].find(tokens[nextInputToken].type);
            if (it == _ll1Table[stackTop.name].end()) {
                cerr << line << endl;
                for (size_t i = 0; i < linePos; i++)  cerr << ' ';
                cerr << '|' << endl << "Error: Wrong syntax" << endl << endl;
                return NULL;
            } else {
                parseStack.pop_back();
                const auto & production = _productions[it->second];
                for (int i = production.rhsSymbols.size(); i > 0; i--)
                    parseStack.push_back(production.rhsSymbols[i - 1]);

                // Parse tree construction
                auto astStackTop = astStack.top();
                astStack.pop();
                for (size_t i = 0; i < production.rhsSymbols.size(); i++) {
                    const auto & symbol = production.rhsSymbols[i];
                    if (symbol.type == Symbol::EPSILON)
                        continue;
                    else if (symbol.type == Symbol::NONTERMINAL) {
                        ASTNode * newAstNode = _getASTNode();
                        newAstNode->type = ASTNode::EMPTY;
                        astStackTop->children.push_back(newAstNode);
                    } else if (symbol.type == Symbol::TERMINAL) {
                        if (_unusedTerminals.find(symbol.name) != _unusedTerminals.end())
                            continue;

                        ASTNode * newAstNode = _getASTNode();
                        if (_unaryOperators.find(it->second) != _unaryOperators.end()
                            && _unaryOperators[it->second] == i) {
                            newAstNode->type = ASTNode::UNARY_LEFT_OPERATOR;
                        } else if (_binaryOperators.find(symbol.name) != _binaryOperators.end()) {
                            newAstNode->type = ASTNode::BINARY_OPERATOR;
                        } else {
                            newAstNode->type = ASTNode::OPERAND;
                        }
                        astStackTop->children.push_back(newAstNode);
                    }
                }

                for (int i = astStackTop->children.size(); i > 0; i--)
                    astStack.push(astStackTop->children[i - 1]);
            }
        }
    }

#ifdef LOG_DEBUG
    cout << endl;
#endif LOG_DEBUG

    return astTree;
}


Parser::ASTNode * Parser::_convertParseTreeToAST(ASTNode * astTree) {

#ifdef LOG_DEBUG
    cout << "AST Tree" << endl;
    cout << "--------" << endl;
    _printASTTree(astTree, 0);
    cout << endl;
#endif LOG_DEBUG

    // Prune the initial parse tree
    _pruneParseTree(astTree);
    if (astTree->children.size() == 1)
        astTree = astTree->children[0];
    astTree->parent = NULL;

    // Basic step. Move up the operators in the tree until their children match
    // the number of their operands, in the right order (for example, a unary
    // left operator will move up the tree until its parent has a right child)
    _moveUpOperators(astTree);

    // Another final pruning is necessary
    _pruneParseTree(astTree);
    if (astTree->children.size() == 1)
        astTree = astTree->children[0];
    astTree->parent = NULL;

#ifdef LOG_DEBUG
    cout << "AST Tree" << endl;
    cout << "--------" << endl;
    _printASTTree(astTree, 0);
    cout << endl;
#endif LOG_DEBUG

    return astTree;
}


void Parser::_pruneParseTree(ASTNode * root) {
    root->parent = NULL;

    if (root->children.size() == 0)
        return;

    // Recursively prune the children first
    for (auto * child : root->children)
        _pruneParseTree(child);

    // Delete the epsilon and all the unnecessary terminals, and all the
    // nonterminals with no children
    auto it = remove_if(root->children.begin(), root->children.end(),
        [](const auto * node) {
        const auto * token = node->token;
        if ((node->type != ASTNode::EMPTY && token->type == "(")//TODO
            || (node->type != ASTNode::EMPTY && token->type == ")"))
            return true;

        return (node->type == ASTNode::EMPTY && node->children.size() == 0);
    });
    root->children.erase(it, root->children.end());

    // Collapse node with only one child
    for (size_t i = 0; i < root->children.size(); i++) {
        auto * child = root->children[i];
        if (child->children.size() == 1 && child->type == ASTNode::EMPTY)
            root->children[i] = child->children[0];
    }

    // Set the parent pointer at each child
    for (auto * child : root->children)
        child->parent = root;
}


void Parser::_evalASTTree(ASTNode * astTree) {

    if (astTree->token->type != "=") {
        // Compute the expression recursively using the AST tree
        vector<Monomial> result;
        try { 
            result = _evalASTNode(astTree);
        } catch (const _EvalException & e) {
            cerr << "Error: " << e.what() << endl;
            return;
        }

        // Print the result, formatted
        string delimiter = "";
        cout << "ans = ";
        if (result.size() != 0) {
            if (result[0].coefficient != 1 || result[0].exponent == 0)
                cout << result[0].coefficient;
            cout << (result[0].exponent != 0 ? "x" : "");
            if (result[0].exponent != 0 && result[0].exponent != 1)
                cout << "^" << result[0].exponent;
            for (size_t i = 1; i < result.size(); i++) {
                const auto & term = result[i];
                cout << (term.coefficient > 0 ? " + " : " - ");
                if (abs(term.coefficient) != 1 || abs(term.exponent) == 0)
                    cout << abs(term.coefficient);
                cout << (term.exponent != 0 ? "x" : "");
                if (term.exponent != 0 && term.exponent != 1)
                    cout << "^" << term.exponent;
            }
        } else {
            cout << 0;
        }
        cout << endl;

    } else {   // astTree->token->type == "="
             // We have an equation. Compute the expression on each side recursively as above, and then
             // subtract the right-hand side from the left-hand side
        vector<Monomial>  lhs, rhs;
        try {
            lhs = _evalASTNode(astTree->children[0]);
            rhs = _evalASTNode(astTree->children[1]);
        }
        catch (const _EvalException & e) {
            cerr << "Error: " << e.what() << endl;
            return;
        }
        _subtractPolynomial(lhs, rhs);

        if (lhs.size() > 0 && lhs[0].exponent > 2) {
            cout << "Error: Equations of degree > 2 are not supported" << endl;
            return;
        }

        if (lhs.size() == 0) {
            cout << "Infinitely many solutions" << endl;
            return;
        }

        int degree = lhs[0].exponent;
        double a[3] = { lhs[0].coefficient, 0, 0 };   // The coefficients of the equation
        for (size_t i = 1; i < lhs.size(); i++)
            a[degree - lhs[i].exponent] = lhs[i].coefficient;

        switch (degree) {
            case 0:
                // Trivial equation of scalars (no polynomials)
                if (a[0] != 0)
                    cout << "No solutions" << endl;
                else
                    cout << "Infinitely many solutions" << endl;
                break;

            case 1:
                // Linear equation
                cout << "x = " << -a[1] / a[0] << endl;
                break;

            case 2:
                // Quadratic equation
                double D = (a[1] * a[1] - 4 * a[0] * a[2]);
                if (D > 0)
                    cout << "x = " << ((-a[1] + sqrt(D)) / (2 * a[0]))
                    << " or x = " << ((-a[1] - sqrt(D)) / (2 * a[0])) << endl;
                else if (D == 0)
                    cout << "x = " << -a[1] / (2 * a[0]) << endl;
                else
                    cout << "No solutions" << endl;
                break;
        }
    }
}


bool Parser::_moveUpOperators(ASTNode * root) {
    for (auto * child : root->children)
        if (!_moveUpOperators(child))
            return false;

    bool hasLeft = root->children.size() >= 1;
    bool hasRight = root->children.size() >= 2;

    ASTNode * current = root;
    while ( (current->type == ASTNode::BINARY_OPERATOR && (!hasLeft || !hasRight))
        || (current->type == ASTNode::UNARY_LEFT_OPERATOR && !hasRight) ) {
        ASTNode * parent = current->parent;
        if (parent == NULL) {
            cerr << "Error: Invalid parse tree construction" << endl;
            return false;
        }

        // Which child of the parent are we?
        size_t childIndx = 0;
        for (; childIndx < parent->children.size(); childIndx++) {
            if (parent->children[childIndx] == current)
                break;
        }

        if (childIndx > 0)
            hasLeft = true;
        if (childIndx < parent->children.size() - 1)
            hasRight = true;

        swap(current->type, parent->type);
        swap(current->token, parent->token);
        current = current->parent;
    }

    return true;
}


//
// Dumps the AST tree in preorder traversal
//
void Parser::_printASTTree(ASTNode * node, int depth) {
    for (int i = 0; i < depth; i++) cout << ' ';
    cout << '(' << node->type;
    if (node->type != ASTNode::EMPTY && node->token)
        cout << ',' << node->token->type << ',' << node->token->value;
    cout << ")" << endl;
    for (const auto child : node->children)
        _printASTTree(child, depth + 2);
}



vector<Parser::Monomial> Parser::_evalASTNode(ASTNode * node) {
    const auto & children = node->children;
    if (children.size() == 0) {
        if (node->token->value[0] == 'x')
            return vector<Monomial> { { 1, 1 } };
        else
            return vector<Monomial> { { (double)atof(node->token->value.c_str()), 0 } };
    }

    switch (node->token->value[0]) {
        case '+':
            {
                auto lhs = _evalASTNode(children[0]);
                return _addPolynomial(lhs, _evalASTNode(children[1]));
            }
        case '-':
            if (node->type == ASTNode::BINARY_OPERATOR) {
                auto lhs = _evalASTNode(children[0]);
                return _subtractPolynomial(lhs, _evalASTNode(children[1]));
            } else if (node->type == ASTNode::UNARY_LEFT_OPERATOR) {
                vector<Monomial> zero = vector<Monomial>{ { 0, 0 } };
                return _subtractPolynomial(zero, _evalASTNode(children[0]));
            }
        case '*':
            return _multiplyPolynomials(_evalASTNode(children[0]), _evalASTNode(children[1]));
        case '/':
            return _dividePolynomials(_evalASTNode(children[0]), _evalASTNode(children[1]));
    }

    return vector<Monomial>();
}


vector<Parser::Monomial> & Parser::_addPolynomial(vector<Monomial> & lhs, const vector<Monomial> & rhs) {
    for (const auto & term : rhs)
        lhs.push_back(term);

    sort(lhs.begin(), lhs.end(), [](const Monomial & a, const Monomial & b) {
        return a.exponent > b.exponent;
    });

    for (size_t i = 0; i < lhs.size();) {
        size_t j;
        for (j = i + 1; j < lhs.size() && lhs[j].exponent == lhs[i].exponent; j++)
            lhs[i].coefficient += lhs[j].coefficient;
        i = j;
    }

    auto it = unique(lhs.begin(), lhs.end(), [](const Monomial & terml, const Monomial & termr) {
        return terml.exponent == termr.exponent;
    });
    it = remove_if(lhs.begin(), it, [](const Monomial & term) { return term.coefficient == 0; });
    lhs.erase(it, lhs.end());

    return lhs;
}


vector<Parser::Monomial> & Parser::_subtractPolynomial(vector<Monomial> & lhs, const vector<Monomial> & rhs) {

    for (const auto & term : rhs) {
        Monomial invTerm = term;
        invTerm.coefficient *= -1;
        lhs.push_back(invTerm);
    }

    sort(lhs.begin(), lhs.end(), [](const Monomial & a, const Monomial & b) {
        return a.exponent > b.exponent;
    });

    for (size_t i = 0; i < lhs.size();) {
        size_t j;
        for (j = i + 1; j < lhs.size() && lhs[j].exponent == lhs[i].exponent; j++)
            lhs[i].coefficient += lhs[j].coefficient;
        i = j;
    }

    auto it = unique(lhs.begin(), lhs.end(), [](const Monomial & terml, const Monomial & termr) {
        return terml.exponent == termr.exponent;
    });
    it = remove_if(lhs.begin(), it, [](const Monomial & term) { return term.coefficient == 0; });
    lhs.erase(it, lhs.end());

    return lhs;
}


vector<Parser::Monomial> Parser::_multiplyPolynomials(const vector<Monomial> & lhs, const vector<Monomial> & rhs) {
    vector<Monomial> result;
    vector<Monomial> temp;
    for (const auto & termr : rhs) {
        temp.clear();
        temp = lhs;
        for (auto & terml : temp) {
            terml.coefficient *= termr.coefficient;
            terml.exponent += termr.exponent;
        }

        _addPolynomial(result, temp);
    }

    return result;
}


vector<Parser::Monomial> Parser::_dividePolynomials(const vector<Monomial> & lhs, const vector<Monomial> & rhs) {
    if (rhs.size() == 0 || (rhs.size() == 1 && rhs[0].coefficient == 0)) {
        throw _EvalException("Division by 0");
    }

    if (rhs.size() != 1 || rhs[0].exponent != 0) {
        throw _EvalException("Polynomial division is not supported");
    }

    double divisor = rhs[0].coefficient;
    vector<Monomial> result = lhs;
    for (auto & term : result) {
        term.coefficient /= divisor;
    }

    return result;
}
