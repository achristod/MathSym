#include "tokenizer.h"

#include <algorithm>
#include <fstream>
#include <iostream>

using namespace std;

bool Tokenizer::init(const string & configFile) {
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

        // Remove all whitespace from the config line
        auto truncEnd = remove_if(line.begin(), line.end(), isspace);
        line.erase(truncEnd, line.end());

        auto delimPos = line.find(':');
        if (delimPos == string::npos || delimPos == 0 || delimPos == line.size() - 1) {
            cerr << "Error: Malformed line " << lineCount << " in file "
                << configFile << endl;
            return false;
        }

        // Create a new rule for the regular expression of the line
        try {
            _rules.push_back({ line.substr(0, delimPos), regex(line.substr(delimPos + 1)) });
        } catch (const regex_error & e) {
            cerr << "Error: Malformed regular expression in line " << lineCount
                << " of file " << configFile << endl;
            cout << "       " << e.what() << endl;
            return false;
        }
    }

    return true;
}

bool Tokenizer::tokenize(string & line, vector<Token> & tokens) {
    // Remove all whitespace from the input command
    auto lineEnd = remove_if(line.begin(), line.end(), isspace);
    line.erase(lineEnd, line.end());

    // Go through the whole command line and tokenize it fully, i.e. making sure that each
    // character belongs to one token, specified by the rules read from the config file
    size_t inPos = 0;
    while (inPos < line.size()) {
        bool matched = false;
        for (size_t i = 0; i < _rules.size(); i++) {
            const Rule & rule = _rules[i];

            smatch match;
            try {
                if (!regex_search(line.cbegin() + inPos, line.cend(), match, rule.regex,
                    regex_constants::match_continuous))
                    continue;

                // Found the next token
                tokens.push_back({ rule.type, match.str() });
                inPos += match.length();
                matched = true;
                break;

            } catch (const regex_error & e) {
                cerr << "Error: Failed to run regular expression" << endl;
                cout << "       " << e.what() << endl;
                return false;
            }
        }

        if (matched == false) {
            cerr << line << endl;
            for (size_t i = 0; i < inPos; i++)  cerr << ' ';
            cerr << '|' << endl;
            cerr << "Error: Invalid character in input" << endl << endl;
            return false;
        }
    }

    return true;
}
