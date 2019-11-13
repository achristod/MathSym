#ifndef TOKENIZER_H
#define TOKENIZER_H

#include "token.h"

#include <regex>
#include <string>
#include <vector>

class Tokenizer {
public:
    bool init(const std::string & configFile);
    bool tokenize(std::string & line, std::vector<Token> & tokens);

private:
    struct Rule {
        std::string type;
        std::regex regex;
    };

    std::vector<Rule> _rules;
};

#endif // !TOKENIZER_H
