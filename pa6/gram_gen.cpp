

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>
#include <algorithm>

using namespace std;


class RuleTerm {
  public:
    enum RuleTermType {
        NONE,
        PLUS,  
        QUES,
        STAR
    };

    string           name;
    RuleTermType     type;
    vector<RuleTerm> terms;

    void dump() {
        if (terms.size() > 0) {
            cout << "(";
            for (unsigned i=0; i<terms.size(); i++) {
                terms[i].dump();
                if (i<terms.size()-1) {
                    cout << " ";
                }
            }
            cout << ")";
        }
        else {
            cout << name;
        }
        
        if (type == PLUS) {
            cout << "+";
        }
        else if (type == STAR) {
            cout << "*";
        }    
        else if (type == QUES) {
            cout << "?";
        }
    }



};


class Rule {
  public:
    string                      name; 
    vector< vector<RuleTerm> >  derives;
    int                         canBeEmpty;

    Rule () {
        canBeEmpty = -1;
    }
    
    void dump() {
        cout << name << ":" << endl;
        for (unsigned i=0 ; i<derives.size() ; i++) 
        {
            cout << "\t";
            for (unsigned j=0; j<derives[i].size(); j++) 
            {
                derives[i][j].dump();
                if (j<derives[i].size()-1) {
                    cout << " ";
                }
            } 
            cout << endl;
        }
    }
};


struct deriveSort {
  bool operator() ( const vector<RuleTerm> v1, const vector<RuleTerm> v2)
  {
      return (v1.size() >= v2.size());
  }
} derive_sort;


class GramGen 
{
  public:

    string match() {
        string r = *mit;
        mit++;
        return r;
    }

 
    string match(string s) {
        if (*mit == s) {
            string r = *mit;
            mit++;
            return r;
        }
        else {
            cout << "error!! matching " << s << endl;
            exit(1);
            return "";
        }
    }


    RuleTerm parseTerm() {
        RuleTerm rt;
        rt.type = RuleTerm::NONE;

        if ( *mit == "(" ) {
            match("(");
            while ( *mit != ")" ) {
                rt.terms = parseRuleTerms();
            }
            match(")");
        }
        else {
            rt.name = match();
            rt.type = RuleTerm::NONE;
        }

        if ( *mit == "+") {
            rt.type = RuleTerm::PLUS;
            mit++;
        }
        else if (*mit == "*") {
            rt.type = RuleTerm::STAR;
            mit++;
        }
        else if (*mit == "?") {
            rt.type = RuleTerm::QUES;
            mit++;
        }
        else {
            // cout << "bad qualifier : " << *mit << endl;
            // exit(1);
        }

        return rt;
    }



    vector<RuleTerm> parseRuleTerms() {
        vector<RuleTerm> rv;

        while ( *mit != "\n" && *mit != ")" ) {
            RuleTerm rt = parseTerm();
            rv.push_back(rt);
        }

        return rv;
    }


    Rule parseRule() {
        Rule rule;
        
        rule.name = match();
        match(":");    
        match("\n");

        while ( *mit == "\t") {
            match("\t");
            vector<RuleTerm> terms = parseRuleTerms();
            match("\n");
            rule.derives.push_back( terms );
        }

        return rule;
    }


    bool can_be_empty(Rule& rule) 
    {
        if (rule.canBeEmpty >= 0) 
        {
            return rule.canBeEmpty == 0 ? false : true;
        }

        for (unsigned i=0; i<rule.derives.size() ; i++) 
        {
            bool canBeEmpty = true;

            for (unsigned j=0; j<rule.derives[i].size(); j++)
            {
                canBeEmpty = canBeEmpty && can_be_empty( rule.derives[i][j] ); 
            }

            if (canBeEmpty == true) 
            {
                rule.canBeEmpty = 1;
                return true;
            }
        }
        rule.canBeEmpty = 0;
        return false;
    }


    bool can_be_empty(RuleTerm& term)
    {
        if (term.type == RuleTerm::NONE)
        {
            for (unsigned i=0; i<rules.size() ; i++) 
            {
                if (rules[i].name == term.name) 
                {
                    return can_be_empty( rules[i] );
                }        
            }
            return false;
        }
        else if (term.type == RuleTerm::STAR || term.type == RuleTerm::QUES)
        {
            return true;
        }
        else if (term.type == RuleTerm::PLUS)
        {
            if (term.terms.size() == 0) 
            {
                for (unsigned i=0; i<rules.size() ; i++) 
                {
                    if (rules[i].name == term.name) 
                    {
                        return can_be_empty( rules[i] );
                    }        
                }
                return false;
            }
            else 
            {
                for (unsigned i=0; i<term.terms.size(); i++) 
                {
                    if ( can_be_empty( term.terms[i] ) == false) 
                    {
                        return false;
                    } 
                }
            }
        }
    }

    void update_empty()
    {
        bool bEmpty = true;

        for (unsigned i=0; i<rules.size(); i++) 
        {
            bool canBeEmpty = can_be_empty( rules[i] );

            // update the copy in hash
            map<string, Rule>::iterator mit = this->nonTerminalMap.find( rules[i].name );
            mit->second.canBeEmpty = canBeEmpty;
        }
    }
    
    void parse(vector<string> ts) 
    {
        tokens = ts;
        mit = tokens.begin();
        while (mit != tokens.end()) 
        {
            if (*mit == "\n") {
                mit++;
                continue;
            }
            Rule rule = parseRule();     
            sort( rule.derives.begin(), rule.derives.end(), derive_sort );
            rules.push_back(rule);
            nonTerminalMap[rule.name] = rule;
        }

        update_empty();

        // start to generate code
        //
	    generateCode(); 
    }


    string replaceStr(string s, char a, char b) 
    {
        for (unsigned i=0; i<s.size(); i++) {
            if (s[i] == a) {
                s[i] = b;
            }
        }
        return s;
    }

    string generateTokenName( string t )
    {
        if (t == "identifier") 
        {
            return "PT_SIMPLE";
        }
        else {
            string s = "PT_";
            s += t;
            return s;
        }
    }


    string generateCodeToken(string rID, string token, string indent)
    {
        stringstream code; 
        stringstream tmpss;

        if ( isNonTerminal( token ) ) 
        {
            tmpss << "parse__" << replaceStr( token , '-' , '_') << "();"; 
            code << indent << "CppAstPtr " << rID << " = " << tmpss.str() << endl;

            map<string,Rule>::iterator mit = this->nonTerminalMap.find( token );
            if (mit->second.canBeEmpty == false)    
            {
                code << indent << "if (" << rID << "->size()==0) {" << endl;
                code << indent << "    break;" << endl;
                code << indent << "}" << endl;
            }
        }
        else 
        {
            tmpss << "match(" << generateTokenName( token ) << ");"; 
            code << indent << "CppAstPtr " << rID << " = " << tmpss.str() << endl;
            code << indent << "if (" << rID << "->size()==0) {" << endl;
            code << indent << "    break;" << endl;
            code << indent << "}" << endl;
        }

        return code.str();
    }


    string generateCodeTokenTerm(string rID, RuleTerm& ruleTerm, string indent)
    {
        stringstream code; 

        if (ruleTerm.type == RuleTerm::NONE)
        {
            code << generateCodeToken( rID, ruleTerm.name, indent);
        }
        else if (ruleTerm.type == RuleTerm::QUES)
        {
            stringstream tmpss;
            string token = ruleTerm.name;
            if ( isNonTerminal( token ) ) 
            {
                tmpss << "parse__" << replaceStr( token , '-' , '_') << "();"; 
            }
            else 
            {
                tmpss << "match(" << generateTokenName( token ) << ");"; 
            }

            code << indent << "CppAstPtr " << rID << " = " << tmpss.str() << endl;
        }
        else if (ruleTerm.type == RuleTerm::PLUS)
        {
            stringstream tmpss;
            string token = ruleTerm.name;
            bool canBeEmpty = false;

            if ( isNonTerminal( token ) ) 
            {
                tmpss << "parse__" << replaceStr( token , '-' , '_') << "();"; 

                map<string,Rule>::iterator mit = this->nonTerminalMap.find( token );
                canBeEmpty = mit->second.canBeEmpty;
            }
            else 
            {
                tmpss << "match(" << generateTokenName( token ) << ");"; 
            }


            string ptrStr = rID;
            ptrStr += "_ptr";

            string idxStr = "idx_";
            idxStr += rID;

            code << indent << "CompAst* " << ptrStr << " = new CompAst();" << endl;
            code << indent << "int " << idxStr << " = 0;" << endl;
            code << indent << "CppAstPtr firstPtr = " << tmpss.str() << endl;
            if (canBeEmpty == false) {
                code << indent << "if (firstPtr->size() == 0) {" << endl;
                code << indent << "    break;" << endl;
                code << indent << "}" << endl;
            }
            code << indent << ptrStr << "->astMap[to_string(" << idxStr << ")] = firstPtr;" << endl;
            code << indent << idxStr << "++;" << endl;
            code << indent << "while (true) {" << endl;
            code << indent << "    CppAstPtr iterPtr = " << tmpss.str() << endl;
            code << indent << "    if (iterPtr->size() == 0) {" << endl; 
            code << indent << "         break;" << endl; 
            code << indent << "    }" << endl; 
            code << indent << "    " << ptrStr << "->astMap[to_string(" << idxStr << ")] = iterPtr;" << endl; 
            code << indent << "    "<< idxStr << "++;" << endl; 
            code << indent << "    continue;" << endl; 
            code << indent << "}" << endl;
            code << indent << "CppAstPtr "<< rID << "(" << ptrStr << ");" << endl;
        }
        else if (ruleTerm.type == RuleTerm::STAR)
        {
            stringstream tmpss;
            string token = ruleTerm.name;

            if ( isNonTerminal( token ) ) 
            {
                tmpss << "parse__" << replaceStr( token , '-' , '_') << "();"; 
            }
            else 
            {
                tmpss << "match(" << generateTokenName( token ) << ");" ; 
            }

            string ptrStr = rID;
            ptrStr += "_ptr";

            string idxStr = "idx_";
            idxStr += rID;

            code << indent << "CompAst* " << ptrStr << " = new CompAst();" << endl;
            code << indent << "int " << idxStr << " = 0;" << endl;
            code << indent << "while (true) {" << endl;
            code << indent << "    CppAstPtr iterPtr = " << tmpss.str() << endl;
            code << indent << "    if (iterPtr->size() == 0) {" << endl; 
            code << indent << "         break;" << endl; 
            code << indent << "    }" << endl; 
            code << indent << "    " << ptrStr << "->astMap[to_string(" << idxStr << ")] = iterPtr;" << endl; 
            code << indent << "    "<< idxStr << "++;" << endl; 
            code << indent << "    continue;" << endl; 
            code << indent << "}" << endl;
            code << indent << "CppAstPtr "<< rID << "(" << ptrStr << ");" << endl;
        }

        return code.str();
    }


    string generateCodeTerm(string rID, RuleTerm& ruleTerm, string indent)
    {
        stringstream code;

        if (ruleTerm.terms.size() == 0) 
        {
            return generateCodeTokenTerm(rID, ruleTerm, indent);
        }
        else if (ruleTerm.type == RuleTerm::QUES)
        {
            vector<string> subIDs;

            string ptrStr = rID;
            ptrStr += "_ptr";

            string idxStr = "idx_";
            idxStr += rID;

            string bakStr = "bakPos_";
            bakStr += rID;

            string sub_indent = indent;                 
            sub_indent += "    ";

            code << indent << "PtIt " << bakStr << " = _ptIt;" << endl; 
            code << indent << "CompAst* " << ptrStr << " = new CompAst();"  << endl; 
            code << indent << "do {" << endl; 
            //code << indent << "for (int rich=0; rich<1 ; rich++) {" << endl; 
            for (unsigned i=0; i<ruleTerm.terms.size(); i++) 
            {
                string sub_rID = rID;
                sub_rID += to_string(i);
                                
                code << generateCodeTokenTerm(sub_rID, ruleTerm.terms[i], sub_indent);

                subIDs.push_back( sub_rID );
            }

            for (unsigned j=0; j<subIDs.size(); j++) 
            {
                code << sub_indent << ptrStr << "->astMap[\"" << subIDs[j] << "\"] = " << subIDs[j] << ";" << endl;
            }
            code << sub_indent <<  bakStr << " = _ptIt;" << endl;
            code << sub_indent <<  "cout << \"BACKTO :\" <<  _ptIt->source << endl;" << endl;

            code << indent << "}" << endl; 
            code << indent << "while (false);" << endl; 
            code << indent << "_ptIt = " << bakStr << ";" << endl;
            code << indent << "CppAstPtr " << rID << "( " << ptrStr << " );" << endl;

        }
        else if (ruleTerm.type == RuleTerm::STAR || ruleTerm.type == RuleTerm::PLUS)
        {
            vector<string> subIDs;

            string ptrStr = rID;
            ptrStr += "_ptr";

            string idxStr = "idx_";
            idxStr += rID;

            string bakStr = "bakPos_";
            bakStr += rID;

            string sub_indent = indent;                 
            sub_indent += "    ";

            code << indent << "PtIt " << bakStr << " = _ptIt;" << endl; 
            code << indent << "CompAst* " << ptrStr << " = new CompAst();"  << endl; 
            code << indent << "while (true) {" << endl; 
            for (unsigned i=0; i<ruleTerm.terms.size(); i++) 
            {
                string sub_rID = rID;
                sub_rID += to_string(i);
                                
                code << generateCodeTokenTerm(sub_rID, ruleTerm.terms[i], sub_indent);

                subIDs.push_back( sub_rID );
            }

            for (unsigned j=0; j<subIDs.size(); j++) 
            {
                code << sub_indent << ptrStr << "->astMap[\"" << subIDs[j] << "\"] = " << subIDs[j] << ";" << endl;
            }
            code << sub_indent <<  bakStr << " = _ptIt;" << endl; 

            code << indent << "}" << endl; 
            code << indent << "_ptIt = " << bakStr << ";" << endl; 
            code << indent <<  "cout << \"BACKTO :\" <<  _ptIt->source << endl;" << endl;

            if (ruleTerm.type == RuleTerm::PLUS)
            {
                code << indent << "if (" << ptrStr << "->size()==0) {"  << endl;
                code << indent << "    break;"  << endl;
                code << indent << "}"  << endl;
            }
            code << indent << "CppAstPtr " << rID << "( " << ptrStr << " );" << endl;
        }

        return code.str();
    }


    void generateCode() 
    {
        stringstream code; 

        for (unsigned i=0 ; i<rules.size(); i++) 
        {
            string nonTerminal = rules[i].name;
            string indent1 = "    ";
            string indent2 = "        ";
            string indent3 = "            ";
            nonTerminal = replaceStr( nonTerminal, '-', '_'); 

            code << indent1 << "CppAstPtr parse__" << nonTerminal << " ()" << endl;
            code << indent1 << "{" << endl;
            code << indent1 << "    PtIt bakPos = _ptIt;" << endl;
            code << indent1 << "    Autocat ac( \"" << rules[i].name << "\" );" << endl;

            for (unsigned j=0; j<rules[i].derives.size() ; j++)
            {
                vector<string> termIds;
                vector<RuleTerm> derive = rules[i].derives[j]; 

                code << indent1 << "    do {" << endl;
                // code << indent1 << "    for (int rich=0; rich<1; rich++) {" << endl;
                for (unsigned k=0; k<derive.size(); k++)  
                {
                    stringstream tID;  
                    tID << "t_" << j << "_" << k; 
                    code << generateCodeTerm(tID.str(), derive[k], indent3) << endl;
                    termIds.push_back( tID.str() );

                    // when the last term is done, prepare to return value
                    //
                    if (k==derive.size()-1) 
                    {
                        code << indent3 << "CompAst* ret = new CompAst();" << endl;;
                        for (vector<string>::iterator idIt = termIds.begin(); idIt != termIds.end(); idIt++)
                        {
                            code << indent3 << "ret->astMap[\"" << *idIt << "\"] = " << *idIt << ";" << endl;
                        }
                        code << indent3 << "return CppAstPtr( ret );" << endl;;
                    }
                }
                code << indent1 << "    }" << endl; 
                code << indent1 << "    while (false);" << endl;
                code << indent1 << "    _ptIt = bakPos;" << endl;
                code << indent1 << "    cout << \"BACKTO :\" <<  _ptIt->source << endl;" << endl;
            }

            code << indent1 << "    return CppAstPtr( new EmptyAst() );" << endl;
            code << indent1 << "}" << endl;
            code << endl << endl;
        }

        ofstream myfile;
        myfile.open("code.cpp");
        myfile << code.str();
        myfile.close();
    }

    bool isNonTerminal(string s) 
    {
        map<string, Rule>::iterator mit = nonTerminalMap.find(s);
        if (mit == nonTerminalMap.end()) {
            return false;
        }
        else {
            return true;
        }
    }

    void dump() {
        for (unsigned i=0; i<rules.size(); i++) {
            rules[i].dump();
            cout << endl;
        }
    }


    
    

    map<string, Rule>         nonTerminalMap;         
    vector<string>            tokens;
    vector<string>::iterator  mit;
    vector<Rule>              rules;
};



void process(string& gramTxt) {
    vector<string> tokens;

    bool prevChar = true;

    string t = "";
    for (int i=0; i<gramTxt.size(); i++) {
        if (gramTxt[i] == ' ' ) {
            if (t.size() != 0) {
                tokens.push_back( t );
                t = "";
            } 
        }
        else if (gramTxt[i]== '?'  || gramTxt[i]== '+' || gramTxt[i]=='*' || 
                 gramTxt[i]== '('  || gramTxt[i]== ')' ||
                 gramTxt[i]== '\t' || gramTxt[i] == '\n' || gramTxt[i]==':') 
        {
            if (t.size() != 0) {
                tokens.push_back( t );
            }
            t = gramTxt[i];
            tokens.push_back( t );
            t = "";
        }
        else if (gramTxt[i] == '\\') {
            if (gramTxt[i+1] == '\n') {
                i++;
            }
        }
        else {
            t += gramTxt[i];
        }
    }


    GramGen gramgen;
    gramgen.parse( tokens );
    gramgen.dump();    



    
}



int main(int argc, char** args) {
    
    if (argc < 2) {
        cout << "Usage: Input the grammar file." << endl;
    }

    fstream fs; 
    fs.open(args[1], fstream::in);
    stringstream ss;
    ss << fs.rdbuf();
    string gramTxt = ss.str(); 
    fs.close();

    process( gramTxt );

    return 0;
}