#include "tokenizer.h"
#include "parser.h"

#include <iostream>
#include <string>

using namespace std;

const string TOKENIZER_CONFIG = "tokenizer_config.txt";
const string PARSER_CONFIG = "parser_config.txt";
const string SEMANTICS_CONFIG = "semantics_config.txt";


int main()
{
    Tokenizer tokenizer;
    Parser parser;

    if (!tokenizer.init(TOKENIZER_CONFIG))
        return 0;

    if (!parser.init(PARSER_CONFIG, SEMANTICS_CONFIG))
        return 0;
    
    while (true) {
        // Read a line from the standard input
        cout << ">> ";
        string line;
        getline(cin, line);
        if (line == "exit")
            break;

        vector<Token> tokens;
        if (!tokenizer.tokenize(line, tokens))
            continue;

        parser.parse(tokens, line);
    }

    return 0;
}
