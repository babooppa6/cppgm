// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#ifndef PA4
#pragma once
#endif

#include <list>
#include <string>
#include "utf8.cpp"
#include "utf16.cpp"
#include "pptoken.cpp"
#include "posttoken.cpp"
#include "ctrlexpr.cpp"

using namespace std;


class DirectiveHandlerException : public exception
{
  public:
    DirectiveHandlerException(const char* msg)
       : _errMsg(msg)
    {
    }

    virtual ~DirectiveHandlerException() throw() 
    {
    }

    virtual const char* what() const throw() 
    { 
        return _errMsg.c_str(); 
    }

  private:
    string _errMsg;
};


enum MacroPPTokenType
{
    DEFINE=1,
    UNDEF,
    IF,
    IFDEF,
    IFNDEF,
    ELIF,
    ELSE,
    ENDIF,
    INCLUDE,
    LINE,
    ERROR,
    PRAGMA,
    TXT
};


struct MacroPPToken
{
    MacroPPTokenType type;
    list<PPToken>    pplst;
};


struct Directive {

    enum {
        OBJ = 1,
        FUN
    };

    string          name;
    int             type;     //0: object , 1:func
    int             paraNum;

    map<string,int> paramLst;
    vector<PPToken> replaceLst;
};


const map<string, MacroPPTokenType> string2macroPPTokenTypeMap = 
{
    { "define", DEFINE },
    { "undef", UNDEF},
    { "if",  IF},
    { "ifdef",  IFDEF},
    { "ifndef",  IFNDEF},
    { "elif",   ELIF},
    { "else",   ELSE},
    { "endif",  ENDIF },
    { "include",  INCLUDE},
    { "line",  LINE},
    { "error",  ERROR},
    { "pragma",  PRAGMA},
    { "TXT",  TXT}
};


class DirectiveHandler {

  public:
    DirectiveHandler (vector<PPToken>& pps)
        : _pps(pps)
    {
    }

    ~DirectiveHandler () {}


    PPToken mergePPToken ( PPToken& p1, PPToken& p2)
    {
        vector<int> mergedUNC;
        mergedUNC.insert(mergedUNC.end(), p1.data.begin(), p1.data.end());
        mergedUNC.insert(mergedUNC.end(), p2.data.begin(), p2.data.end());

        PPTokenizer tokenizer;
        tokenizer.parse( mergedUNC );
        return tokenizer._elst[0];
    }

    bool processDirective(MacroPPToken& macro)
    {
        
        return true;
    }


    list<PPToken> replaceText( list<PPToken> tokens )
    {
        list<PPToken> result;
        list<PPToken>::iterator ppit; 
        while (tokens.size() != 0)
        { 
            ppit = tokens.begin();
           
            if (ppit->type == PP_IDENTIFIER) 
            {
                map<string, Directive*>::iterator dit = _directiveLst.find( ppit->utf8str );

                if (dit != _directiveLst.end() &&
                    ppit->blackLst.find( dit->second ) == ppit->blackLst.end())
                {
                    // macro replace
                    Directive* dir = dit->second;
                    if (dir->type == Directive::FUN)
                    {
                        //-----
                        // collect arguments
                        // and then run arguments replacement
                        //
                        PPToken curFun = *ppit;
                        int quoteStack = 0;
                        int argIdx = -1;
                        vector< list<PPToken> > args;
                        tokens.pop_front();
                        while (tokens.size() != 0)
                        {
                            ppit = tokens.begin();
                            if (ppit->type == PP_WHITESPACE)    
                            {
                                ;
                            }
                            else if (ppit->utf8str == "(")
                            {
                                quoteStack++;
                                if (quoteStack == 1)
                                {
                                    argIdx++;
                                    args.push_back( list<PPToken>() );
                                }
                                else
                                {
                                    args[argIdx].push_back( *ppit );
                                }
                            }
                            else if (ppit->utf8str == ")")
                            {
                                quoteStack--;
                                if (quoteStack != 0)
                                {
                                    args[argIdx].push_back( *ppit );
                                }
                                else
                                {
                                    tokens.pop_front();
                                    break;
                                }
                            }
                            else if (ppit->utf8str == ",")
                            {
                                if (quoteStack == 1)
                                {
                                    argIdx++;
                                    args.push_back( list<PPToken>() );
                                }
                                else
                                {
                                    args[argIdx].push_back( *ppit );
                                }
                            }
                            else
                            {
                                if (quoteStack > 0)
                                {
                                    args[argIdx].push_back( *ppit );
                                }
                                else
                                {
                                    // the token does not have argument list
                                    // treat it as normal identifier.
                                    //
                                    result.push_back( curFun );
                                    result.push_back( *ppit );
                                    tokens.pop_front();
                                    break;
                                }
                            }

                            tokens.pop_front();
                        }


                        if (quoteStack != 0)
                        {
                            throw DirectiveHandlerException("Unbalanced quotes");
                        }

                        if (args.size() == 0)
                        {
                            continue;
                        }

                        //-----
                        // scan through all the replacement list
                        //
                        for (unsigned i=dir->replaceLst.size() ; i>0; i--)
                        {
                            PPToken p = dir->replaceLst[i-1];

                            map<string,int>::iterator pmit = dir->paramLst.find( p.utf8str );
                            if (pmit != dir->paramLst.end())
                            {
                                int idx = pmit->second; 

                                // recursive replace arguments
                                list<PPToken> rarg = replaceText( args[idx] );

                                // put the corresponding argument here
                                tokens.insert(tokens.begin(), rarg.begin(), rarg.end());
                            }
                            else
                            {
                                p.blackLst.insert( dir );
                                tokens.push_front( p );
                            }
                        }
                        continue;
                    }
                    else  // Directive::OBJ
                    {
                        tokens.pop_front();  // remove the current token, 
                        for (unsigned i=dir->replaceLst.size(); i>0; i--)
                        {
                            PPToken p = dir->replaceLst[i-1];
                            if (p.type == PP_IDENTIFIER)
                            {
                                p.blackLst.insert( dir );
                            }
                            tokens.push_front( p );
                        }
                        continue;
                    }
                }
                else
                {
                    // normal identifier
                    result.push_back( *ppit );
                }
            }
            else
            {
                result.push_back( *ppit );
            }

            tokens.pop_front();
        }
        return result;
    }




    //
    // consumes all text lines  until the next directive start
    //
    bool processTextLines(MacroPPToken& macro)
    {

        list<PPToken> result = replaceText( macro.pplst );
        _result.insert(_result.end(), result.begin(), result.end());

        return true;
    }


    bool processDirectiveDefine(MacroPPToken& macro)
    {
        Directive* dir = new Directive;
        //-----
        // 0 -> # -> 1 -> define -> 2 -> id -> 3 -> !( -> 5 -> replacelst
        //                                       -> (  -> 4 -> paramlst -> )  -> 5 -> replacelst
        //
        list<PPToken>::iterator ppit;
        int state = 0;
        int argIdx = 0;

        while (macro.pplst.size() > 0)
        {
            ppit = macro.pplst.begin();

            if (ppit->type == PP_WHITESPACE)
            {
                // skip whitespace
            }
            else
            {
                switch (state)
                {
                    case 0:
                        if (ppit->utf8str == "#")
                        {
                            state = 1;
                        }
                        break;
                    case 1:
                        if (ppit->utf8str == "define")
                        {
                            state = 2;
                        }
                        break;
                    case 2:
                        if (ppit->type == PP_IDENTIFIER)
                        {
                            state = 3;
                            dir->name = ppit->utf8str;
                        }
                        break;
                    case 3:
                        if (ppit->type == PP_OP && ppit->utf8str == "(")
                        {
                            dir->type = Directive::FUN;
                            state = 4;
                        }
                        else 
                        {
                            dir->type = Directive::OBJ;
                            state = 5;
                            continue;
                        }
                        break;
                    case 4:
                        while (ppit->utf8str != ")")
                        {
                            if (ppit->type == PP_IDENTIFIER)
                            {
                                dir->paramLst[ ppit->utf8str ] = argIdx;
                                argIdx++;
                                // dir->paramLst.push_back(ppit->utf8str);
                            }
                            else if (ppit->type == PP_OP)
                            {
                                if (ppit->utf8str == ",")
                                {
                                    // do nothing
                                }
                                else if (ppit->utf8str == "...")
                                {
                                    dir->paramLst[ ppit->utf8str ] = argIdx;
                                    argIdx++;
                                    // dir->paramLst.push_back(ppit->utf8str);
                                }
                            }
                            else
                            {
                                throw DirectiveHandlerException("Bad directive type");
                            }
                            macro.pplst.pop_front();
                            ppit = macro.pplst.begin();
                        }
                        state = 5;
                        break;
                    case 5:
                        dir->replaceLst.push_back( *ppit ); 
                        break;
                    default:
                        throw DirectiveHandlerException("Bad define syntax");
                        break;
                }
            }

            macro.pplst.pop_front();
        }

        _directiveLst[ dir->name ] = dir;

        return true;
    }


    bool processDirectiveUndefine (MacroPPToken& macro)
    {
        list<PPToken>::iterator ppit;
        int state = 0;
        while (macro.pplst.size() > 0)
        {
            ppit = macro.pplst.begin();

            if (ppit->type == PP_WHITESPACE)
            {
                // skip whitespace
            }
            else
            {
                switch (state)
                {
                    case 0:
                        if (ppit->utf8str == "#")
                        {
                            state = 1;
                        }
                        break;
                    case 1:
                        if (ppit->utf8str == "undef")
                        {
                            state = 2;
                        }
                        break;
                    case 2:
                        if (ppit->type == PP_IDENTIFIER)
                        {
                            state = 3;
                            map<string, Directive*>::iterator mit = _directiveLst.find(ppit->utf8str);
                            _directiveLst.erase( mit );
                        }
                        else
                        {
                            throw DirectiveHandlerException("Bad undef argument");
                        }
                        break;
                    default:
                        throw DirectiveHandlerException("Bad undef syntax");
                        break;
                }
            }

            macro.pplst.pop_front();
        }

        return true;
    }




    void process()
    {
        //-----
        // group tokens to directive macro token  or text line macro token
        //
        vector<PPToken>::iterator it = _pps.begin();        
        while (it != _pps.end())
        {
            PPTokenType type = it->type;
            string str = it->utf8str; 
            if (type == PP_OP && str=="#")
            {
                // directive start
                MacroPPToken macro;
                macro.pplst.push_back( *it );
                it++;
                while (it->type == PP_WHITESPACE)
                {
                    it++;
                }
                if (it->type == PP_IDENTIFIER)
                {
                    map<string, MacroPPTokenType>::const_iterator mit = string2macroPPTokenTypeMap.find(it->utf8str);
                    if (mit == string2macroPPTokenTypeMap.end())
                    {
                        throw DirectiveHandlerException("Bad directive type");
                    }
                    else
                    {
                        macro.type = mit->second;
                    }
                }
                else if (it->type == PP_NEWLINE)
                {
                    // do nothing, empty directive is allowed
                }
                else
                {
                    throw DirectiveHandlerException("Bad directive type");
                }

                while (it->type != PP_NEWLINE)
                {
                    macro.pplst.push_back( *it );
                    it++;
                }
                it++; // skip new line
                _list.push_back( macro );
                continue;
            }
            else if (type == PP_WHITESPACE)
            {
                it++;
                continue;
            }
            else if (type == PP_NEWLINE)
            {
                it++;
                continue;
            }
            else
            {
                bool foundDirective = false;
                MacroPPToken macro; 
                macro.type = TXT;

                // text line
                while (foundDirective == false && it->type != PP_EOF)
                {
                    while (it->type != PP_NEWLINE)
                    {
                        macro.pplst.push_back( *it ); 
                        it++;
                    }

                    // test if the next line is a directive statement
                    //
                    vector<PPToken>::iterator it2 = it;  // it is now newline
                    it2++;
                    while ( it2->type == PP_WHITESPACE )
                    {
                        it2++;
                    }
                    if (it2->type == PP_OP && str == "#")
                    {
                        foundDirective = true;
                    }

                    macro.pplst.push_back( *it );
                    it ++;
                }

                if (it->type == PP_EOF)
                {
                    macro.pplst.push_back( *it );
                    it++;
                }

                _list.push_back( macro );
                continue;
            }
        }

 
        //----
        // PA5 goes here 
        //


        //----- 
        // loop through all macro lines
        //
        list<MacroPPToken>::iterator lit = _list.begin();

        while (lit != _list.end())
        {
            if (lit->type == DEFINE)
            {
                processDirectiveDefine( *lit );
            } 
            else if (lit->type == UNDEF)
            {
                processDirectiveUndefine( *lit );
            }
            else if (lit->type == TXT)
            {
                processTextLines( *lit );
            }
            lit++;
        }
    }


  public:
    PostTokenizer             _postTokenizer;
    vector<PostToken>         _pts;
    vector<PPToken>           _pps;
    vector<PPToken>           _result;
    list<MacroPPToken>        _list;

    map<string, Directive*>   _directiveLst;
};

#ifdef PA4
int main()
{
	// TODO:
	// 1. apply your code from PA1 to produce `preprocessing-tokens`
	// 2. "post-tokenize" the `preprocessing-tokens` as described in PA2
	// 3. write them out in the PA2 output format specifed
    try
    {
        ostringstream oss;
        oss << cin.rdbuf();
        string input = oss.str();

        // Decode input stream (UTF-8) to UNC
        vector<int> uncTokens;
        int code_unit;
        UTF8Decoder utf8Decoder(&input);
        while ((code_unit = utf8Decoder.nextCode()) > 0)
        {
            uncTokens.push_back(code_unit);
        }
        if (uncTokens.size()>0 && uncTokens[uncTokens.size()-1]!='\n')
        {
            uncTokens.push_back('\n');
        }

        PPTokenizer ppTokenizer;
        ppTokenizer.parse(uncTokens);

        DirectiveHandler directiveHandler(ppTokenizer._elst);
        directiveHandler.process();
             

        // PA2 start
        PostTokenizer postTokenizer(directiveHandler._result);         
        postTokenizer.parse();

        // // PA3 start
        // vector<PostToken>::iterator it = postTokenizer._tokens.begin(); 
        // vector<PostToken>::iterator lstart=it, lend; 
        // while (it != postTokenizer._tokens.end())
        // {
        //     if (it != lstart)
        //     {
        //         if (it->type == PT_NEWLINE)
        //         {
        //             PPCtrlExprEvaluator peval(lstart, it);
        //             peval.startEval();
        //             it++;
        //             lstart = it;
        //             continue;
        //         }
        //     } 
        //     else
        //     {
        //         if (it->type == PT_NEWLINE)
        //         {
        //             it++;
        //             lstart = it;
        //             continue;
        //         }
        //         else if (it->type == PT_EOF)
        //         {
        //             cout << "eof" << endl;
        //             break;
        //         }
        //     }
        //     it++;
        // }

        // PA2 string concat
        PostToken concatStr;
        bool bPrevStr = false;
        vector<int> strCodes;

        for (unsigned i=0 ; i<postTokenizer._tokens.size() ; i++)
        {
            PostToken pt = postTokenizer._tokens[i];

            if (pt.type == PT_LITERAL_ARRAY || pt.type == PT_UD_LITERAL_ARRAY)
            {
                if (bPrevStr==true)
                {
                    concatStr.source += " ";
                    concatStr.source += pt.source;

                    if (concatStr.type == PT_INVALID)
                    {
                        continue;
                    }

                    int code_unit;
                    UTF8Decoder utf8Decoder(&pt.udPrefix);
                    while ((code_unit = utf8Decoder.nextCode()) > 0)
                    {
                        strCodes.push_back(code_unit);
                    }

                    if (concatStr.udSuffix != "" && pt.udSuffix != "" && concatStr.udSuffix != pt.udSuffix)
                    {
                        // error
                        concatStr.type = PT_INVALID;
                        continue;
                    }
                    else
                    {
                        concatStr.udSuffix = (concatStr.udSuffix == "") ? pt.udSuffix : concatStr.udSuffix;
                    }

                    if (concatStr.udSuffix != "")
                    {
                        if (concatStr.udSuffix[0] != '_' || concatStr.type == PT_INVALID)
                        {
                            concatStr.type = PT_INVALID;
                            continue;
                        }
                        else
                        {
                            concatStr.type = PT_UD_LITERAL_ARRAY;
                        }
                    }


                    if (concatStr.ltype != FT_CHAR && pt.ltype != FT_CHAR && concatStr.ltype != pt.ltype)
                    {
                        // different char type
                        concatStr.type = PT_INVALID;
                        continue;
                    } 
                    else
                    {
                        concatStr.ltype = (concatStr.ltype == FT_CHAR) ? pt.ltype : concatStr.ltype;
                    }

                    bPrevStr = true;
                } 
                else
                {
                    concatStr = pt;

                    int code_unit;
                    UTF8Decoder utf8Decoder(&pt.udPrefix);
                    while ((code_unit = utf8Decoder.nextCode()) > 0)
                    {
                        strCodes.push_back(code_unit);
                    }

                    if (concatStr.udSuffix != "" && concatStr.udSuffix[0] != '_')
                    {
                        concatStr.type = PT_INVALID;
                    }
                    bPrevStr = true;
                }
                continue;
            }

            if (bPrevStr == true)
            {
                char* mem;
                int size;
                strCodes.push_back(0);

                if (concatStr.ltype == FT_CHAR || concatStr.ltype == FT_UNSIGNED_CHAR || concatStr.ltype == FT_SIGNED_CHAR)
                {
                    string es = UTF8Encoder::encode(strCodes);
                    mem = new char[es.size()];
                    memcpy(mem, es.c_str(), es.size());
                    size = es.size();
                }
                else if (concatStr.ltype == FT_CHAR16_T)
                {
                    vector<short> utf16_codes = UTF16Encoder::encode( strCodes );
                    char16_t* data = new char16_t[utf16_codes.size()];
                    for (unsigned int i=0; i<utf16_codes.size(); ++i)
                    {
                        data[i] = utf16_codes[i];
                    }
                    mem = (char*)data;
                    size = utf16_codes.size();
                }
                else if (concatStr.ltype == FT_CHAR32_T)
                {
                    char32_t* data = new char32_t[strCodes.size()];
                    for (unsigned int i=0; i<strCodes.size(); ++i)
                    {
                        data[i] = strCodes[i];
                    }
                    mem = (char*)data;
                    size = strCodes.size();
                }
                else if (concatStr.ltype == FT_WCHAR_T)
                {
                    wchar_t* data = new wchar_t[strCodes.size()];
                    for (unsigned int i=0; i<strCodes.size(); ++i)
                    {
                        data[i] = strCodes[i];
                    }
                    mem = (char*)data;
                    size = strCodes.size();
                }

                concatStr.data = mem;
                concatStr.size = size;
                concatStr.emit();
                bPrevStr = false;
                strCodes.clear();
            }
            pt.emit();
        }  // end string concat

    }
    catch (exception& e)
    {
        cerr << "ERROR: " << e.what() << endl;
        return EXIT_FAILURE;
    }
}
#endif
