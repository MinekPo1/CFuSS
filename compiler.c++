#include <fstream>
#include <cstdarg>
#include <stdio.h>
#include "argparse/argparse-2.2/include/argparse/argparse.hpp"
#include <vector>
#include <regex>

std::string escape(std::string str) {
	std::string str2 = "";
	for (int i = 0; i < str.size(); i++){
		switch (str[i]){
			case '\n':
				str2 += "\\n";
				break;
			case '\t':
				str2 += "\\t";
				break;
			case '\"':
				str2 += "\\\"";
				break;
			default:
				str2 += str[i];
				break;
		}
	}
	return str2;
}

// From https://stackoverflow.com/a/13172514/12469275
std::vector<std::string> split_string(
	const std::string& str,
	const std::string& delimiter
)
{
	std::vector<std::string> strings;

	std::string::size_type pos = 0;
	std::string::size_type prev = 0;
	while ((pos = str.find(delimiter, prev)) != std::string::npos)
	{
		strings.push_back(str.substr(prev, pos - prev));
		prev = pos + 1;
	}

	// To get the last substring (or only, if delimiter is not found)
	strings.push_back(str.substr(prev));

	return strings;
}

namespace Lexer{

	enum TokenType{
		TOKEN_OPERATOR,
		TOKEN_NULL,
		TOKEN_NUMBER,
		TOKEN_MULTILINE_STRING_START,
		TOKEN_QUOTE,
		TOKEN_MULTILINE_STRING_END,
		TOKEN_TYPE,
		TOKEN_IDENTIFIER,
		TOKEN_KEYWORD,
		TOKEN_NEWLINE,
		TOKEN_SQ_BRACKET_O,
		TOKEN_SQ_BRACKET_C,
		TOKEN_BRACKET_O,
		TOKEN_BRACKET_C,
		TOKEN_COMMA,
		TOKEN_UNKNOWN
	};

	// bitmap for which keywords start whitespace ignoring
	const int start_ws_ignore = (1 << TOKEN_MULTILINE_STRING_START) + (1 << TOKEN_QUOTE);

	// bitmap for which keywords end whitespace ignoring
	const int stop_ws_ignore = (1 << TOKEN_QUOTE) + (1 << TOKEN_MULTILINE_STRING_END) + (1 << TOKEN_NEWLINE);

	struct Token{
		TokenType type;
		std::string value;
		std::string debug;
		int line;
	};

	namespace RegexPatterns{
		std::basic_regex<char> LIT_NUMBER(R"(\d+(\.\d+)?)");
		std::basic_regex<char> IDENTIFIER(R"([a-zA-Z_][a-zA-Z0-9_]*(\\.[a-zA-Z_][a-zA-Z0-9_]*)*)");

		std::basic_regex<char> ALL_PATTERNS[] = {
			LIT_NUMBER,
			IDENTIFIER
		};

		int PATTERN_COUNT = 2;
		//map of keywords to token types
		TokenType map[] = {
			TOKEN_NUMBER,
			TOKEN_IDENTIFIER
		};
	}

	namespace Keywords{
		std::string VOID      =      "void";
		std::string NUM       =       "num";
		std::string STR       =       "str";
		std::string NULL_U    =      "NULL";
		std::string NULL_L    =      "null";
		std::string TRUE      =      "true";
		std::string FALSE     =     "false";
		std::string STATIC    =    "static";
		std::string MACRO     =     "macro";
		std::string FUNC      =      "func";
		std::string NAMESPACE = "namespace";
		std::string MERGE     =     "merge";
		std::string TO        =        "to";
		std::string SERVE     =     "serve";
		std::string STRUCT    = "structure";

		std::string ALL_KEYWORDS[] = {
			VOID,
			NUM,
			STR,
			NULL_U,
			NULL_L,
			TRUE,
			FALSE,
			STATIC,
			MACRO,
			FUNC,
			NAMESPACE,
			MERGE,
			TO,
			SERVE,
			STRUCT
		};

		int KEYWORD_COUNT = 13;
		//map of keywords to token types
		TokenType map[] = {
			TOKEN_TYPE,
			TOKEN_TYPE,
			TOKEN_TYPE,
			TOKEN_NULL,
			TOKEN_NULL,
			TOKEN_NUMBER,
			TOKEN_NUMBER,
			TOKEN_KEYWORD,
			TOKEN_KEYWORD,
			TOKEN_KEYWORD,
			TOKEN_KEYWORD,
			TOKEN_KEYWORD,
			TOKEN_KEYWORD,
			TOKEN_KEYWORD
		};

	}

	namespace SCTokens{
		char OPERATORS[] = {'+','-','*',':',';','&','|','^','~','<','>','=','%','#','{','}','@','?','\\'};
		char NEWLINE = '\n';
		char SQ_BRACKET_OPEN = '[';
		char SQ_BRACKET_CLOSE = ']';
		char BRACKET_OPEN = '(';
		char BRACKET_CLOSE = ')';
		char COMMA = ',';
		char QUOTE = '"';

		char ALL_SCTokens[] = {
			OPERATORS[0],     // 0
			OPERATORS[1],     // 1
			OPERATORS[2],     // 2
			OPERATORS[3],     // 3
			OPERATORS[4],     // 4
			OPERATORS[5],     // 5
			OPERATORS[6],     // 6
			OPERATORS[7],     // 7
			OPERATORS[8],     // 8
			OPERATORS[9],     // 9
			OPERATORS[10],    // 10
			OPERATORS[11],    // 11
			OPERATORS[12],    // 12
			OPERATORS[13],    // 13
			OPERATORS[14],    // 14
			NEWLINE,          // 15
			SQ_BRACKET_OPEN,  // 16
			SQ_BRACKET_CLOSE, // 17
			BRACKET_OPEN,     // 18
			BRACKET_CLOSE,    // 19
			COMMA,            // 20
			QUOTE,            // 21
		};

		int SCToken_COUNT = 22;

		//map of keywords to token types
		TokenType map[] = {
			TOKEN_OPERATOR,     // 0
			TOKEN_OPERATOR,     // 1
			TOKEN_OPERATOR,     // 2
			TOKEN_OPERATOR,     // 3
			TOKEN_OPERATOR,     // 4
			TOKEN_OPERATOR,     // 5
			TOKEN_OPERATOR,     // 6
			TOKEN_OPERATOR,     // 7
			TOKEN_OPERATOR,     // 8
			TOKEN_OPERATOR,     // 9
			TOKEN_OPERATOR,     // 10
			TOKEN_OPERATOR,     // 11
			TOKEN_OPERATOR,     // 12
			TOKEN_OPERATOR,     // 13
			TOKEN_OPERATOR,     // 14
			TOKEN_NEWLINE,      // 15
			TOKEN_SQ_BRACKET_O, // 16
			TOKEN_SQ_BRACKET_C, // 17
			TOKEN_BRACKET_O,    // 18
			TOKEN_BRACKET_C,    // 19
			TOKEN_COMMA,        // 20
			TOKEN_QUOTE, // 21
		};
	}

	namespace MCTokens{

		std::string ALL_TCTokens[] = {
			"*\"",
			"\"*",
			"->"
		};

		int TCToken_COUNT = 2;

		// map of keywords to token types
		TokenType map[] = {
			TOKEN_MULTILINE_STRING_START, // 0
			TOKEN_MULTILINE_STRING_END,   //
			TOKEN_OPERATOR,               // 1
		};
	}

	std::vector<Token> tokenize(std::string code){
		std::vector<Token> tokens;

		code += ' '; //ensure that no trailing tokens are missed
		int line = 1;
		int column = 0;

		std::string current_token = "";
		// if false all whitespace is considered token separators
		bool catch_whitespace = false;


		for (int i = 0; i < code.size(); i++){
			column++;
			Token SpecialToken;
			SpecialToken.type = TOKEN_UNKNOWN;
			SpecialToken.value = {code[i]};
			SpecialToken.line = line;
			for (int j2 = 0; j2 < MCTokens::TCToken_COUNT; j2++)
			{
				// check multi char tokens
				if (code.substr(i, MCTokens::ALL_TCTokens[j2].size()) == MCTokens::ALL_TCTokens[j2])
				{
					SpecialToken.type = MCTokens::map[j2];
					SpecialToken.value = MCTokens::ALL_TCTokens[j2];
					i += MCTokens::ALL_TCTokens[j2].size() - 1;
					break;
				}
			}
			if (SpecialToken.type == TOKEN_UNKNOWN){
				// check for single char tokens
				for (int j = 0; j < SCTokens::SCToken_COUNT; j++){
					if (code[i] == SCTokens::ALL_SCTokens[j]){
						SpecialToken.type = SCTokens::map[j];

						break;
					}
				}
			}
			if ((code[i] == ' ' or code[i] == '\t') and not catch_whitespace or SpecialToken.type != TOKEN_UNKNOWN) {
				if (code[i] == ' ')
					SpecialToken.debug += "W";
				else
					SpecialToken.debug += "S";
				if (catch_whitespace)
					SpecialToken.debug += "C";
				else
					SpecialToken.debug += " ";

				if (current_token == ""){
					if (SpecialToken.type != TOKEN_UNKNOWN){
						SpecialToken.debug += "a";
						if (!catch_whitespace and (1<<SpecialToken.type & start_ws_ignore)){
							catch_whitespace = true;
							SpecialToken.debug += "+";
						}
						else if (catch_whitespace and (1<<SpecialToken.type & stop_ws_ignore)){
							catch_whitespace = false;
							SpecialToken.debug += "-";
						}
						tokens.push_back(SpecialToken);
						if (SpecialToken.value == "\n"){
							line++;
							column = 0;
						}
					}
					continue;
				}
				// if the current char is a newline, we will have to add a newline token after

				// check if the current token is valid
				Token token;
				token.type  = TOKEN_UNKNOWN;
				token.value = current_token;
				token.line  = line;
				if (code[i] == ' ')
					token.debug += "W";
				else
					token.debug += "S";
				if (catch_whitespace)
					token.debug += "C";
				else
					token.debug += " ";

				// since comparations are way faster than regexes
				// check for "keywords" first

				for (int i = 0; i < Keywords::KEYWORD_COUNT; i++){
					if (current_token == Keywords::ALL_KEYWORDS[i]){
						token.type = Keywords::map[i];
						break;
					}
				}

				// if not a keyword check for regexes

				if (token.type == TOKEN_UNKNOWN){
					for (int j = 0; j < RegexPatterns::PATTERN_COUNT; j++){
						if (std::regex_match(current_token, RegexPatterns::ALL_PATTERNS[j])){
							token.type = RegexPatterns::map[j];
							break;
						}
					}
				}

				token.debug += "b";
				// check if the added token turns whitespace ignoring on or off
				if (!catch_whitespace and 1<<token.type & start_ws_ignore){
					catch_whitespace = true;
					token.debug += "+";
				}
				else if (catch_whitespace and 1<<token.type & stop_ws_ignore){
					catch_whitespace = false;
					token.debug += "-";
				}

				// Add the token to the vector
				tokens.push_back(token);

				// if the charachter is a newline, add a newline token
				if (SpecialToken.type != TOKEN_UNKNOWN){
					SpecialToken.debug += "c";
					// check if the newline token turns whitespace ignoring on or off
					// (it most likely will turn off, but for the sake of not hardcoding it)
					if (!catch_whitespace and 1<<SpecialToken.type & start_ws_ignore){
						catch_whitespace = true;
						SpecialToken.debug += "+";
					}
					else if (catch_whitespace and 1<<SpecialToken.type & stop_ws_ignore){
						catch_whitespace = false;
						SpecialToken.debug += "-";
					}
					if (SpecialToken.value == "\n"){
						line++;
						column = 0;
					}
					tokens.push_back(SpecialToken);
				}

				// yes there is a chance that the current token is unknown
				// the parser will have to handle this

				// reset the current token
				current_token = "";

				continue;
			}

			// if all the above checks fail, add the char to the current token
			current_token += code[i];
		}


		return tokens;
	}

}

namespace Parser{
	enum _ValType{
		TYPE_VOID,
		TYPE_NUM,
		TYPE_STR,
	};

	union ValType{
		bool is_struct;
		_ValType type;
		std::string* struct_name;
		ValType(_ValType type){
			is_struct = false;
			this->type = type;
		}
		ValType(std::string name){
			if (name == "void"){
				is_struct = false;
				type = (TYPE_VOID);
			}
			else if (name == "num"){
				is_struct = false;
				type = (TYPE_NUM);
			}
			else if (name == "str"){
				is_struct = false;
				type = (TYPE_STR);
			}
			else if (name == "structure"){
				is_struct = true;
				struct_name = new std::string("UKNOWN");
			}
			else{
				is_struct = true;
				struct_name = new std::string(name);
			}
		}
	};

	enum ElementType{
		ELEMENT_VOID,
		ELEMENT_TOKEN,
		ELEMENT_OPERATION,
		ELEMENT_LITERAL,
		ELEMENT_MACRO_DEF,
		ELEMENT_REF,
		ELEMENT_FUNCTION_DEF,
		ELEMENT_FUNCTION_CALL,
		ELEMENT_STRUCT_DEF,
		ELEMENT_IMPORT,
		ELEMENT_IDENTIFIER,
		ELEMENT_ERROR,
		ELEMENT_SERVE,
	};

	struct Element{
		ElementType type;
		int line;
		std::string value;
		std::string debug;
		union Data{
			int void_;
			Lexer::Token* token;
			struct Operation{
				Element* l;
				Element* r;
				char op;
			} operation;
			struct Literal{
				_ValType type;
				union Value{
					int num;
					std::string * str;
				} value;
			} literal;
			struct MacroDef{
				std::string* name;
				Element* body;
				ValType type;
			} macro_def;
			struct FuncDef{
				std::string* name;
				std::vector<std::string> *args;
				std::vector<ValType> *argTypes;
				std::vector<Element> *argDefaults; // void = no default
				std::vector<Element> *body;
				ValType ret_type;
			} function_def;
			struct FuncCall{
				std::string* name;
				std::vector<Element> *args;
			} function_call;
			std::string* ref;
			struct Serve{
				std::string* name;
				std::vector<Element> *args;
			} serve;
		} data;
	};

	struct ParserError{
		int line;
		std::string message;
		ParserError *previous;
	};

	struct ParseResult{
		std::vector<Element> elements;
		std::vector<ParserError> errors;
		bool successful;
	};

	ValType get_type(std::string type){
		if (type == "void"){
			return ValType(TYPE_VOID);
		}
		else if (type == "num"){
			return ValType(TYPE_NUM);
		}
		else if (type == "str"){
			return ValType(TYPE_STR);
		}
		else{
			return ValType(TYPE_VOID);
		}
	}

	ParseResult _parse(std::vector<Element> elements){
		std::vector<ParserError> errors;
		bool successful = true;

		bool did_something;
		do{
			did_something = false;
			for (int i = 0; i < elements.size(); i++)
			{
				if (elements[i].type == ELEMENT_TOKEN){
					switch (elements[i].data.token->type){
						// create number literals out of tokens
						case Lexer::TOKEN_NUMBER:
							{
								Element element = {ELEMENT_LITERAL, elements[i].line, elements[i].value, "L", 0};
								element.data.literal.type = _ValType::TYPE_NUM;
								element.data.literal.value.num = std::stoi(elements[i].data.token->value);
								elements[i] = element;
								did_something = true;
							}
							break;
						// create null literals out of tokens
						case Lexer::TOKEN_NULL:
							{
								Element element = {ELEMENT_LITERAL, elements[i].line, elements[i].value, "L", 0};
								element.data.literal.type = _ValType::TYPE_VOID;
								elements[i] = element;
								did_something = true;
							}
							break;
						// try creating strings out of tokens
						/**
						 * strings are more complicated as they are not just a one token
						 * single line strings have:
						 *
						 *     TOKEN_QUOTE <random token(s)> TOKEN_QUOTE
						 *
						 * which is relatively simple
						 *
						 * multi line strings have:
						 *
						 *     TOKEN_MULTILINE_STRING_START <random token(s) (optional)> TOKEN_NEWLINE
						 *     TOKEN_QUOTE <random token(s)> TOKEN_NEWLINE
						 *     TOKEN_MULTILINE_STRING_END
						 *
						 */

						// single line string
						case Lexer::TOKEN_QUOTE:
							{
								Element element = {ELEMENT_LITERAL, elements[i].line, "", "L", 0};
								std::string value = "";
								// lookahead for the end of the string
								int j = i+1;
								for (
									;
									elements[j].type == ELEMENT_TOKEN and (
										elements[j].data.token->type != Lexer::TOKEN_QUOTE and
										elements[j].data.token->type != Lexer::TOKEN_NEWLINE
									);
									j++
								)
									value += elements[j].data.token->value;

								if (elements[j].data.token->type != Lexer::TOKEN_QUOTE){
									// if it is not create an error
									ParserError error = {elements[i].line, "Expected closing quote"};
									errors.push_back(error);
									successful = false;
									// remove the quote token and the token which would be the content of the string
									elements.erase(elements.begin()+i, elements.begin()+j+1);
									did_something = true;
								}
								else{
									element.data.literal.type = _ValType::TYPE_STR;
									element.data.literal.value.str = &value;
									element.value = elements[i].value + value + elements[j].value;
									did_something = true;
									elements[i] = element;
									elements.erase(elements.begin()+i+1, elements.begin()+j+1);
								}
							}
							break;

						// multi line string
						case Lexer::TOKEN_MULTILINE_STRING_START:
							{
								Element element = {ELEMENT_LITERAL, elements[i].line, "", "L", 0};
								std::string value = "";
								// lookahead for the end of the string
								int j = i+1;
								if (elements[j].type == ELEMENT_TOKEN and elements[j].data.token->type == Lexer::TOKEN_NEWLINE)
									j++;

								bool in_string = true;
								for(
									;
									elements[j].type == ELEMENT_TOKEN and (
										elements[j].data.token->type != Lexer::TOKEN_MULTILINE_STRING_END
									);
									j++
								){
									if(in_string)
									{
										if(elements[j].data.token->type == Lexer::TOKEN_NEWLINE){
											value += "\n";
											in_string = false;
										}
										else{
											value += elements[j].data.token->value;
										}
									}
									else{
										if (elements[j].data.token->type != Lexer::TOKEN_QUOTE)
											// error will be created after that
											break;
										in_string = true;
									}
								}
								// check why the loop ended
								if (elements[j].type != ELEMENT_TOKEN or elements[j].data.token->type != Lexer::TOKEN_MULTILINE_STRING_END)
								{
									// error
									// lookahead to see if there is a closing quote
									int k = j+1;
									for (
										;
										k < elements.size();
										k++
									)
										if (elements[k].type == ELEMENT_TOKEN and elements[k].data.token->type == Lexer::TOKEN_MULTILINE_STRING_END
										                                       or elements[k].data.token->type == Lexer::TOKEN_MULTILINE_STRING_START)
											break;
									if (k == elements.size() or elements[k].data.token->type != Lexer::TOKEN_MULTILINE_STRING_END)
									{
										ParserError error = {elements[j].line, "Expected closing multiline string"};
										errors.push_back(error);
										successful = false;
										// remove the quote token and the token which would be the content of the string
										elements.erase(elements.begin()+i+1, elements.begin()+j+1);
										Element error_element = {ELEMENT_ERROR, elements[j].line, "<ERROR>", "", 0};
										elements[i] = error_element;
									}
									else{
										ParserError error = {elements[j].line, "Multiline string continuation missing"};
										errors.push_back(error);
										successful = false;
										// remove the quote token and the token which would be the content of the string
										elements.erase(elements.begin()+i+1, elements.begin()+k+1);
										// place an error element
										Element error_element = {ELEMENT_ERROR, elements[j].line, "<ERROR>", "", 0};
										elements[i] = error_element;
									}
								}
								else{
									element.data.literal.type = _ValType::TYPE_STR;
									element.data.literal.value.str = &value;
									element.value = elements[i].value + value + elements[j].value;
									did_something = true;
									elements[i] = element;
									elements.erase(elements.begin()+i+1, elements.begin()+j+1);
								}
							}
							break;
						// error
						case Lexer::TOKEN_MULTILINE_STRING_END:
							{
								ParserError error = {elements[i].line, "Unexpected closing multiline string"};
								errors.push_back(error);
								successful = false;
								// remove the quote token
								elements.erase(elements.begin()+i);
							}
							break;
						case Lexer::TOKEN_OPERATOR:
							{
								if(elements[i].data.token->value != "~"){
									// check if all the operands cannot be expressions
									if (i == 0){
										// error
										ParserError error = {elements[i].line, "Unexpected operator"};
										errors.push_back(error);
										elements.erase(elements.begin()+i);
										successful = false;
										break;
									}
									if (elements[i-1].type == ELEMENT_TOKEN){
										if (elements[i-1].data.token->type == Lexer::TOKEN_NEWLINE){
											// error
											ParserError error = {elements[i].line, "Unexpected operator (expected operand before operator)"};
											if (elements[i].data.token->value == "-"){
												error.message += " (did you mean to use `~`?)";
											}
											errors.push_back(error);
											// remove the token
											elements.erase(elements.begin()+i);
											successful = false;
											break;
										}
										if (elements[i-1].data.token->type == Lexer::TOKEN_OPERATOR and elements[i-1].data.token->value != "@"){
											// error
											ParserError error = {elements[i].line, "Unexpected operator"};
											error.message += " (" + elements[i-1].data.token->value + ")";
											errors.push_back(error);
											// remove the token
											elements.erase(elements.begin()+i+1);
											successful = false;
											break;
										}
									}
									// check if it is not an expression yet
									if (
										    elements[i-1].type != ELEMENT_LITERAL
										and elements[i-1].type != ELEMENT_FUNCTION_CALL
										and elements[i-1].type != ELEMENT_REF
										and elements[i-1].type != ELEMENT_OPERATION
									){
										// this operator is not yet ready to be resolved, we will return here later
										elements[i].debug = "op l ";
										int a = elements[i-1].type;
										while (a){
											elements[i].debug += '0'+a%10;
											a /= 10;
										}
										break;
									}
								}
								if(elements[i].data.token->value != "@"){
									// check if all the operands cannot be expressions
									if (i == elements.size()-1){
										// error
										ParserError error = {elements[i].line, "EOF while looking for operands"};
										errors.push_back(error);
										elements.erase(elements.begin()+i);
										successful = false;
										break;
									}
									if (elements[i+1].type == ELEMENT_TOKEN){
										if (elements[i+1].data.token->type == Lexer::TOKEN_NEWLINE){
											// error
											ParserError error = {elements[i].line, "Unexpected operator (expected operand after operator)"};
											errors.push_back(error);
											elements.erase(elements.begin()+i);
											successful = false;
											break;
										}
										if (elements[i+1].data.token->type == Lexer::TOKEN_OPERATOR and elements[i+1].data.token->value != "~"){
											// error
											ParserError error = {elements[i].line, "Unexpected operator"};
											error.message += " (" + elements[i-1].data.token->value + ")";
											if (elements[i+1].data.token->value == "-"){
												error.message += " (did you mean to use `~`?)";
											}
											errors.push_back(error);
											elements.erase(elements.begin()+i+1);
											successful = false;
											break;
										}
									}
									// check if it is not an expression yet
									if (
										    elements[i+1].type != ELEMENT_LITERAL
										and elements[i+1].type != ELEMENT_FUNCTION_CALL
										and elements[i+1].type != ELEMENT_REF
										and elements[i+1].type != ELEMENT_OPERATION
									){
										// this operator is not yet ready to be resolved, we will return here later
										elements[i].debug = "op r ";
										int a = elements[i+1].type;
										while (a){
											elements[i].debug += '0'+a%10;
											a /= 10;
										}
										break;
									}
								}
								// construct the new element
								Element element = {ELEMENT_OPERATION, elements[i].line, elements[i].data.token->value, ""};
								element.data.operation = {0,0,elements[i].data.token->value[0]};
								if (elements[i].data.token->value != "~"){
									element.data.operation.l = &elements[i-1];
									element.value = elements[i-1].value + element.value;
									elements.erase(elements.begin()+i-1);
									i--; // since we removed an element before this one we need to decrement the index
								}
								if (elements[i].data.token->value != "@"){
									element.data.operation.r = &elements[i+1];
									element.value += elements[i+1].value;
									elements.erase(elements.begin()+i+1);
								}
								// replace the tokens with the new element
								elements[i] = element;
								did_something = true;
							}
							break;
						case Lexer::TOKEN_IDENTIFIER:
							{
								// check if it is a function call
								if(elements[i+1].type == ELEMENT_TOKEN and elements[i+1].data.token->type == Lexer::TOKEN_BRACKET_O){
									// look for the matching bracket
									int bracket_c = i+2;
									for (;bracket_c != elements.size(); bracket_c++)
									{
										if (elements[bracket_c].type == ELEMENT_TOKEN and elements[bracket_c].data.token->type == Lexer::TOKEN_BRACKET_C){
											break;
										}
									}
									if (bracket_c == elements.size()){
										// error
										ParserError error = {elements[i].line, "Missing closing bracket"};
										errors.push_back(error);
										successful = false;
										break;
									}
									// construct the new element
									Element element = {ELEMENT_FUNCTION_CALL, elements[i].line, elements[i].data.token->value, ""};
									element.data.function_call = {};
									element.data.function_call.name = &elements[i].data.token->value;
									element.data.function_call.args = new std::vector<Element>();
									bool can_add_new_arg = true;
									bool complete = false;
									bool a_error = false;
									for (int j = i+2; j < bracket_c; j++)
									{
										if (can_add_new_arg){
											if (elements[j].type == ELEMENT_TOKEN and elements[j].data.token->type == Lexer::TOKEN_COMMA){
												// syntax error
												ParserError error = {elements[i].line, "Unexpected comma, expected expression"};
												errors.push_back(error);
												successful = false;
												a_error = true;
												break;
											}
											element.data.function_call.args->push_back(elements[j]);
											can_add_new_arg = false;
										}
										else if (elements[j].type == ELEMENT_TOKEN and elements[j].data.token->type == Lexer::TOKEN_COMMA){
											can_add_new_arg = true;
										}
										else{
											// we must not be ready
										}
									}
									if (a_error){
										// clean up, remove the function call
										for (int j = i+1; j <= bracket_c; j++)
										{
											elements.erase(elements.begin()+i);
										}
										Element error_element = {ELEMENT_ERROR, elements[i].line, "<ERROR>", "", 0};
										elements[i] = error_element;
										break;
									}

									// replace the tokens with the new element
									for (int j = i+1; j <= bracket_c; j++)
									{
										elements.erase(elements.begin()+i);
									}
									elements[i] = element;
									did_something = true;
									break;
								}
								// it is a refernce

								Element element = {ELEMENT_REF, elements[i].line, elements[i].data.token->value, ""};
								element.data.ref = &elements[i].data.token->value;
								elements[i] = element;
								did_something = true;
								break;
							}
							break;

						case Lexer::TOKEN_KEYWORD:
							{
								if (elements[i].value == "static"){
									// this is a macro | function | structure | namespace declaration
									// check if the next token is a keyword
									if (
										   i == elements.size()-1
										or elements[i+1].type != ELEMENT_TOKEN
										or elements[i+1].data.token->type != Lexer::TOKEN_KEYWORD) {
										// syntax error
										ParserError error = {elements[i].line, "\"static\" keyword has to be followed by \"macro\", \"function\", \"structure\" or \"namespace\" keyword"};
										errors.push_back(error);
										successful = false;
										break;
									}
									else;
									// split off for each of those
									if (elements[i+1].value == "macro"){
										// macro
										/** structure:
										 * static macro <type> <name> <body> \n
										 */
										int last_good = i+1;
										bool all_good = true;
										// check element types
										if (
											   elements.size() < i+2
											or elements[i+2].type != ELEMENT_TOKEN
											or elements[i+2].data.token->type != Lexer::TOKEN_TYPE
										){
											// syntax error
											ParserError error = {elements[i].line, "Macro declaration must have a type after the \"macro\" keyword"};
											errors.push_back(error);
										}
										else{
											last_good = i+2;
										}
										if (
											   elements.size() < i+3
											or elements[i+3].type != ELEMENT_TOKEN
											or elements[i+3].data.token->type != Lexer::TOKEN_IDENTIFIER
										){
											// syntax error
											ParserError error = {elements[i].line, "Macro declaration must have a name after the type"};
											errors.push_back(error);
										}
										else{
											last_good = i+3;
										}
										if (
											    elements.size() < i+4
											or  elements[i+4].type == ELEMENT_TOKEN
											and elements[i+4].data.token->type == Lexer::TOKEN_NEWLINE
										){
											// syntax error
											ParserError error = {elements[i].line, "Macro declaration must have a body after the name"};
											errors.push_back(error);
										}

										if (not all_good){
											// clean up
											for (int j = i+1; j <= last_good; j++)
											{
												elements.erase(elements.begin()+i);
											}
											successful = false;
											break;
										}
										// get the body
										std::vector<Element> body;
										do{
											body.push_back(elements[i+4]);
											elements.erase(elements.begin()+i+4);
										}
										while (
												elements[i+4].type != ELEMENT_TOKEN
											and elements[i+4].data.token->type != Lexer::TOKEN_NEWLINE
											and i+4 < elements.size()
										);
										// force the body to be parsed
										ParseResult res = _parse(body);

										ParserError rootError = {elements[i].line, "In macro declaration:"};
										for (ParserError error: res.errors){
											error.previous = &rootError;
											errors.push_back(error);
										}
										if (res.elements.size() != 1){
											ParserError error = {elements[i].line, "Macro declaration must have a single statement in the body"};
											errors.push_back(error);
										}
										if (not res.successful){
											successful = false;
											// clean up
											for (int j = i+1; j <= last_good; j++)
											{
												elements.erase(elements.begin()+i);
											}
											break;
										}

										// compose the macro
										Element macro = {ELEMENT_MACRO_DEF, elements[i+2].line, elements[i+3].value, ""};
										macro.data.macro_def.body = &body[0];
										macro.data.macro_def.name = &elements[i+3].value;
										macro.data.macro_def.type = get_type(elements[i+2].value);

										// delete the old elements
										for (int j = i; j <= last_good; j++)
										{
											elements.erase(elements.begin()+i-1);
										}
									}
									if (elements[i+1].value == "func"){
										// function declaration
										/** structure
										 * static func <type> <name> (<parameters>)
										 */
										int last_good = i+1;
										bool all_good = true;
										if (
											   elements.size() < i+2
											or elements[i+2].type != ELEMENT_TOKEN
											or elements[i+2].data.token->type != Lexer::TOKEN_TYPE
										){
											// syntax error
											ParserError error = {elements[i].line, "Function declaration must have a type after the \"static func\" keywords"};
											errors.push_back(error);
											all_good = false;
										}
										else{
											last_good = i+2;
										}
										if (
											   elements.size() < i+3
											or elements[i+3].type != ELEMENT_TOKEN
											or elements[i+3].data.token->type != Lexer::TOKEN_IDENTIFIER
										){
											// syntax error
											ParserError error = {elements[i].line, "Function declaration must have a name after the type"};
											errors.push_back(error);
											all_good = false;
										}
										else{
											last_good = i+3;
										}
										if (
											   elements.size() < i+4
											or elements[i+4].type != ELEMENT_TOKEN
											or elements[i+4].data.token->type != Lexer::TOKEN_BRACKET_O
										){
											// syntax error
											ParserError error = {elements[i].line, "Function declaration must have parameters in brackets after the name"};
											errors.push_back(error);
											all_good = false;
										}
										else{
											last_good = i+4;
										}
										// compose the parameters
										std::vector<std::string> parameters;
										std::vector<ValType> parameter_types;
										std::vector<Element> defaults;

										int j = i+5;
										while (
											   elements[j].type != ELEMENT_TOKEN
											or elements[j].data.token->type != Lexer::TOKEN_BRACKET_C
										){
											// check for syntax errors
											if (
												   elements.size() < j
												or elements[j].type != ELEMENT_TOKEN
												or elements[j].data.token->type != Lexer::TOKEN_TYPE
											){
												// syntax error
												ParserError error = {elements[i].line, "Parameter declaration must start with a type"};
												errors.push_back(error);
												all_good = false;
											}
											else{
												last_good = j;
											}
											if(
												   elements.size() < j+1
												or elements[j+1].type != ELEMENT_TOKEN
												or elements[j+1].data.token->type != Lexer::TOKEN_IDENTIFIER
											){
												// syntax error
												ParserError error = {elements[i].line, "Parameter declaration must have a name after the type"};
												errors.push_back(error);
												all_good = false;
											}
											else{
												last_good = j+1;
											}
											if (
												   elements.size() >= j+2
												or elements[j+2].type != ELEMENT_TOKEN
												or (
													    elements[j+2].data.token->type != Lexer::TOKEN_COMMA
													and elements[j+2].data.token->type != Lexer::TOKEN_BRACKET_C
													and elements[j+2].data.token->type != Lexer::TOKEN_NUMBER
													and elements[j+2].data.token->type != Lexer::TOKEN_QUOTE
													and elements[j+2].data.token->type != Lexer::TOKEN_NULL
												)
											){
												// syntax error
												ParserError error = {elements[i].line, "Syntax error:\"" + elements[j+2].data.token->value + "\" is unexpected here"};
												errors.push_back(error);
												all_good = false;
											}
											else{
												last_good = j+2;
											}
											// if not all_good, don't bother with creating the parameters
											if (all_good){
												if
												parameter_types.push_back(get_type(elements[j].value));
												parameters.push_back(elements[j+1].data.token->value);

												// check if there is a default value
												if (
													    elements[j+2].data.token->type != Lexer::TOKEN_QUOTE
													and elements[j+2].data.token->type != Lexer::TOKEN_BRACKET_C
												){

												}
											}
										}
									}
								}
							}
							break;
						// error
						// default:
						// 	{
						// 		PaserError error = {elements[i].line, "Unexpected token"};
						// 		errors.push_back(error);
						// 		successfull = false;
						// 		// remove the token
						// 	}
						// 	break;
					}
				}
			}


		}
		while(did_something);

		return {elements, errors, successful};
	}

	ParseResult _parse(std::vector<Lexer::Token> tokens){
		std::vector<Element> elements;

		for (int i = 0; i < tokens.size(); i++){
			Element element = {ELEMENT_TOKEN, tokens[i].line, tokens[i].value, tokens[i].debug};
			element.data.token = &tokens[i];
			elements.push_back(element);
		}

		return _parse(elements);
	}

}

int main(int argc, char *argv[]){
	argparse::ArgumentParser program("CFuSS");

	program.add_argument("input")
		.required()
		.help("input file");

	program.add_argument("--output", "-o")
		.default_value("output")
		.help("output file");

	program.add_argument("--c-compiler", "-c")
		.default_value("gcc")
		.help("c compiler to use");

	program.add_argument("--stop-at-c", "-s")
		.default_value(false)
		.implicit_value(true)
		.help("Don't compile the C code, and output it.");

	program.add_argument("--tokens")
		.default_value(false)
		.implicit_value(true)
		.help("Print the tokens.");

	program.add_argument("--ast")
		.default_value(false)
		.implicit_value(true)
		.help("Print the AST.");

	try{
		program.parse_args(argc, argv);
	}
	catch(const std::runtime_error& e){
		std::cerr << e.what() << std::endl;
		std::cerr << program;
		return 1;
	}

	// try to open the input file
	std::ifstream input(program.get<std::string>("input"));
	if(!input.is_open()){
		std::cerr << "Could not open input file. Terminating." << std::endl;
		return 1;
	}
	// tokenize the input file
	std::string code;
	std::getline(input, code, '\0');
	input.close();
	std::vector<Lexer::Token> tokens = Lexer::tokenize(code);

	printf("tokenized successfully\n");

	if (program.get<bool>("--tokens")){
		// print the tokens
		printf("\n");

		int max_value_length = 0;
		for (Lexer::Token token : tokens){
			if (escape(token.value).length() > max_value_length)
				max_value_length = escape(token.value).length();
		}

		for (int i = 0; i < tokens.size(); i++){
			Lexer::Token token = tokens[i];
			printf(
				"[%2d] %2d: \"%s\"%*s%d %-4s\n",
				i,
				token.type, escape(token.value).c_str(),
				max_value_length - escape(token.value).length() + 6,
				" line ", token.line, token.debug.c_str()
			);
		}
	}

	// parse the tokens
	Parser::ParseResult res = Parser::_parse(tokens);
	std::vector<Parser::Element> elements = res.elements;

	if (!res.successful){
		std::vector<std::string> lines = split_string(code, "\n");
		// show the errors
		for (Parser::ParserError error : res.errors){
			printf("%s:\n", error.message.c_str());
			// print the line
			printf("%d | %s\n", error.line, lines[error.line-1].c_str());

		}

		std::cerr << "Parsing failed. Terminating." << std::endl;
		return 1;
	}

	else{
		printf("parsed successfully\n");
	}
	printf("got %d elements\n", elements.size());

	// print the elements
	printf("\n");

	int max_value_length = 0;
	for (Parser::Element element : elements){
		if (escape(element.value).length() > max_value_length)
			max_value_length = escape(element.value).length();
	}

	for (Parser::Element element : elements){
		printf(
			"%2d: \"%s\"%*s%d %-4s\n",
			element.type, escape(element.value).c_str(),
			max_value_length - escape(element.value).length() + 6,
			" line ", element.line, element.debug.c_str()
		);
	}
}
